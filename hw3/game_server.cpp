#include "game_server.hpp"
#include <ftxui/dom/elements.hpp> 
#include <ftxui/dom/table.hpp> 
#include <ftxui/screen/screen.hpp>


GameServer::GameServer(ENetHost* host) 
        : m_host(host)
        , m_start_time(game_clock_t::now())  
{
    m_task_manager.add_task([this](){ return send_ping(); }, 100ms);

    m_task_manager.add_task([this](){ return update_players(); }, s_server_tick_time);

    spdlog::set_level(spdlog::level::warn);
}

void GameServer::run() {
    auto frame_start = game_clock_t::now();
    auto frame_end = frame_start + s_server_tick_time;
    // listen to net events
    ENetEvent event;
    while(int num_events = enet_host_service(m_host , &event, 0) >= 0) {
        if (num_events == 0) {
            spdlog::warn("no events");
        }

        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            auto player = create_player(event.peer->address);
            spdlog::info("added player {}", player.name);
            broadcast_new_player(player);
            
            auto new_object = create_game_object(player.id);

            OutByteStream register_message;
            register_message << new_object;
            write_objects(register_message);
            send_bytes<true>(register_message.get_span(), event.peer);

            for (const auto& old_player : m_players) {
                OutByteStream out;
                out << MessageType::list_update << old_player.to_bytes();
                send_bytes<true>(out.get_span(), event.peer);
            }
            add_player(player);
            event.peer->data = reinterpret_cast<void*>(player.id);
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            InByteStream istr(event.packet->data, event.packet->dataLength);
            MessageType type;
            istr >> type;
            if (type == MessageType::game_update) {
                GameObject object;
                istr >> object;
            } else {
                spdlog::warn("unsupported message type from client: {}", type);
            }
        } else if (event.type == ENET_EVENT_TYPE_NONE) {
            spdlog::info("no events event");
        } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
            spdlog::info("peer on port {} disconnected", event.peer->address.port);
            auto player = get_player(event.peer->address);
            m_players.erase(player); 
        } else {
            spdlog::warn("unsupported network event type: {}", event.type);
        }
        
        m_task_manager.launch();
        update_screen();
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
        auto row_data = { std::to_string(player.id), player.name, std::to_string(player.ping), std::to_string(player.address.host), std::to_string(player.address.port) };
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

