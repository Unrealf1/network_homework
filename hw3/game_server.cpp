#include "game_server.hpp"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <ftxui/dom/elements.hpp> 
#include <ftxui/dom/table.hpp> 
#include <ftxui/screen/screen.hpp>
#include <thread>


GameServer::GameServer(ENetHost* host) 
        : m_host(host)
        , m_start_time(game_clock_t::now())  
{
    m_task_manager.add_task([this](){ return send_ping(); }, 100ms);

    m_task_manager.add_task([this](){ return update_players(); }, s_server_tick_time);

    spdlog::set_level(spdlog::level::warn);
}


void GameServer::process_new_connection(ENetEvent& event) {
    auto player = create_player(event.peer->address);
    spdlog::info("added player {}", player.name);
    broadcast_new_player(player);
    
    auto new_object = create_game_object();
    m_game_objects.push_back(new_object);
    m_player_to_object.insert({player.id, new_object.id});
    
    reset_player(event.peer, new_object);

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
        vec2 position;
        istr >> position;
        auto player = get_player(event.peer->address);
        if (player == m_players.end()) {
            spdlog::warn("there's an imposter on this address: {}:{}", event.peer->address.host, event.peer->address.port);
            return;
        }
        auto obj_id = m_player_to_object.at(player->id);
        auto object = std::find_if(m_game_objects.begin(), m_game_objects.end(), [&](const auto& obj) {return obj.id == obj_id;});
        if (object == m_game_objects.end()) {
            spdlog::error("Cannot find object for player {}", player->id);
        }

        object->position = position;
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

