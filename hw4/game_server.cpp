#include "game_server.hpp"
#include "glm/geometric.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <ftxui/dom/elements.hpp> 
#include <ftxui/dom/table.hpp> 
#include <ftxui/screen/screen.hpp>
#include <stdexcept>
#include <thread>


GameServer::GameServer(ENetHost* host) 
        : m_host(host)
        , m_start_time(game_clock_t::now())  
{
    m_task_manager.add_task([this](){ return send_ping(); }, 100ms);

    m_task_manager.add_task([this]()
        { 
            update_physics(m_dt);
            update_players();
        }, 
        s_server_tick_time);
        

    spdlog::set_level(spdlog::level::warn);
}


void GameServer::process_new_connection(ENetEvent& event) {
    auto player = create_player(event.peer->address);
    spdlog::info("added player {}", player.name);
    broadcast_new_player(player);
    
    auto new_object = create_game_object();
    m_game_objects.push_back(new_object);
    m_player_to_object.insert({player.id, new_object.id});

    OutByteStream register_message;
    register_message << MessageType::register_player;
    register_message << player.id << new_object.id;
    write_objects(register_message);
    send_bytes<true>(register_message.get_span(), event.peer);

    for (const auto& old_player : m_players) {
        OutByteStream out;
        out << MessageType::list_update << old_player.to_bytes();
        send_bytes<true>(out.get_span(), event.peer);
    }
    
    event.peer->data = new uint32_t(player.id);

    add_player(player);
}

void GameServer::process_data(ENetEvent& event) {
    InByteStream istr(event.packet->data, event.packet->dataLength);
    MessageType type;
    istr >> type;
    if (type == MessageType::game_update) {
        spdlog::error("game updates from clients are deprecated");
        vec2 position;
        istr >> position;
        auto player = get_player(event.peer->address);
        if (player == m_players.end()) {
            spdlog::warn("there's an imposter on this address: {}:{}", event.peer->address.host, event.peer->address.port);
            return;
        }
        auto object = get_object_from_player(*player);
        if (object == m_game_objects.end()) {
            return;
        }

        object->position = position;
    } else if (type == MessageType::input) {
        vec2 direction;
        istr >> direction;
        direction = glm::normalize(direction);
        if (direction.x != direction.x || glm::length(direction) < 0.1f) {
            return;
        }
        auto player = get_player(event.peer->address);
        if (player == m_players.end()) {
            spdlog::warn("there's an imposter on this address: {}:{}", event.peer->address.host, event.peer->address.port);
            return;
        }
        auto object = get_object_from_player(*player);
        if (object == m_game_objects.end()) {
            return;
        }
        object->velocity = direction * speed_of_object(*object);
    } else {
        spdlog::warn("unsupported message type from client: {}", type);
    }
}

void GameServer::process_disconnect(ENetEvent& event) {
    spdlog::info("peer on port {} disconnected", event.peer->address.port);
    auto player = get_player(event.peer->address);
    delete static_cast<uint32_t*>(event.peer->data);
    auto obj_mapping = m_player_to_object.find(player->id);
    auto obj = std::find_if(m_game_objects.begin(), m_game_objects.end(), [&](const auto& o) {return o.id == obj_mapping->second;});
    m_player_to_object.erase(obj_mapping);
    m_game_objects.erase(obj);
    m_players.erase(player); 
}

void GameServer::run() {
    for (int i = 0; i < 3; ++i) {
        spawn_robot();
    }

    while (true) {
        auto frame_start = game_clock_t::now();
        auto frame_end = frame_start + s_server_tick_time;
        uint32_t max_events_left = 100000;
        ENetEvent event;
        while(enet_host_service(m_host , &event, 0) > 0 && max_events_left > 0) {
            --max_events_left;
            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                process_new_connection(event);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                process_data(event);
            } else if (event.type == ENET_EVENT_TYPE_NONE) {
                spdlog::info("no events event");
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                process_disconnect(event);
            } else {
                spdlog::warn("unsupported network event type: {}", event.type);
            }
        }
        if (max_events_left == 0) {
            spdlog::warn("can't keep up! too many server events");
        }
        process_robots();
        processs_collisions();
        m_task_manager.launch();
        update_screen();
        std::this_thread::sleep_until(frame_end);
    }
}

void GameServer::update_screen() {
    using namespace ftxui;
    std::vector<std::vector<Element>> table_data;
    table_data.reserve(m_players.size() + 1);
    auto header_data = { "id", "login", "ping", "ip", "port" };
    std::vector<Element> header;
    header.reserve(header_data.size());
    std::transform(header_data.begin(), header_data.end(), std::back_inserter(header), [](const auto& item) { return text(item); });
    table_data.push_back(std::move(header));
    for (const auto& player : m_players) {
        auto row_data = { 
            std::to_string(player.id), 
            player.name, 
            std::to_string(player.ping), 
            std::to_string(player.address.host), 
            std::to_string(player.address.port) 
        };
        std::vector<Element> row;
        std::transform(row_data.begin(), row_data.end(), std::back_inserter(row), [](const auto& item) { return text(item); });
        table_data.push_back(std::move(row));            
    }

    auto table = Table(table_data);

    table.SelectAll().Border(LIGHT);

    table.SelectColumn(0).Border(LIGHT);

    table.SelectRow(0).Decorate(bold);
    table.SelectRow(0).SeparatorVertical(LIGHT);
    table.SelectRow(0).Border(DOUBLE);

    auto content = table.SelectRows(1, -1);
    content.DecorateCellsAlternateRow(color(Color::Blue), 3, 0);
    content.DecorateCellsAlternateRow(color(Color::Cyan), 3, 1);
    content.DecorateCellsAlternateRow(color(Color::White), 3, 2);

    auto document = table.Render();
    auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
    Render(screen, document);
    screen.Print();
    std::cout << screen.ResetPosition();
    if (m_last_num_players > m_players.size()) {
        std::cout << std::endl;
    }
    m_last_num_players = m_players.size();;
}

void GameServer::object_physics(GameObject& object, float dt) {
    object.velocity -= object.velocity * 0.3f * dt;
    if (object.position.x != object.position.x) {
        spdlog::warn("object {} has nan in position :(", object.id);
    }
    object.position += object.velocity * dt;
}

void GameServer::update_physics(float dt) {
    for (auto& object : m_game_objects) {
        object_physics(object, dt);
    }          
    process_borders();
}


Player GameServer::create_player(const ENetAddress& address) {
    auto id = generate_id();
    Player player;
    player.name = generate_name(id); 
    player.address = address; 
    player.id = id; 
    player.ping = 0;
    return player;
}

void GameServer::send_ping() {
    // send ping
    auto peers = get_peers();
    OutByteStream ping_msg;
    ping_msg << MessageType::ping << uint32_t(m_players.size());
    for (const auto& peer : peers) {
        if (peer.address.port == 0) {
            // don't know what it is. Enet implementation detail, I suppose
            continue;
        }
        if (peer.state != ENET_PEER_STATE_CONNECTED) {
            continue;
        }
        
        auto player = get_player(peer.address);
        if (player == m_players.end()) {
            spdlog::warn("host on port {} is not a player!", peer.address.port);
            continue;
        }
        ping_msg << player->id << peer.roundTripTime;
        player->ping = peer.roundTripTime;
    }

    broadcast_message<false>(ping_msg.get_span());
}


void GameServer::update_players() {
    auto peers = get_peers();        
    OutByteStream update_info;
    update_info << MessageType::game_update;
    write_objects(update_info);

    for (auto& peer : peers) {
        if (peer.state != ENET_PEER_STATE_CONNECTED) {
            continue;
        }
        auto player = get_player(peer.address);
        if (player == m_players.end()) {
            spdlog::error("can't find player at address {}:{}", peer.address.host, peer.address.port);
        }
        send_bytes<false>(update_info.get_span(), &peer);
    }
}
