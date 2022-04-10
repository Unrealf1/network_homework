#pragma once

#include <algorithm>
#include <iterator>
#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <array>
#include <chrono>
#include <span>
#include <iostream>

#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <ftxui/dom/elements.hpp> 
#include <ftxui/dom/table.hpp> 
#include <ftxui/screen/screen.hpp>

#include "common.hpp"

using namespace std::chrono_literals;


class GameServer {
    using players_t = std::vector<Player>;
    using clock_t = std::chrono::steady_clock;
    
    template<typename clock_t>
    struct TimerTask {
        std::chrono::milliseconds execution_period;
        std::function<void(void)> task;
        typename clock_t::time_point last_execution{clock_t::now() - execution_period};
    };

public:
    GameServer(ENetHost* host)
        : m_host(host)
        , m_start_time(clock_t::now())  
    {
        m_timer_tasks.emplace_back(
            100ms, [this]() {
                using nlohmann::json;
                // send ping
                auto peers = get_peers();
                json players;
                for (const auto& peer : peers) {
                    if (peer.address.port == 0) {
                        // don't know what it is. Enet implementation detail, I suppose
                        continue;
                    }
                    if (peer.state != ENET_PEER_STATE_CONNECTED) {
                        continue;
                    }
                    json player_js;
                    
                    //TODO: make this more effective(map/set)/add/store host in player/serarch by id in map
                    //id is stored in peer.data
                    auto player = get_player(peer.address);
                    if (player == m_players.end()) {
                        spdlog::warn("host on port {} is not a player!", peer.address.port);
                        continue;
                    }
                    player_js["id"] = player->id;
                    player_js["name"] = player->name;
                    player_js["ping"] = peer.roundTripTime;
                    player->ping = peer.roundTripTime;
                    players.push_back(player_js);
                }
                auto str = players.dump();
                Message message;
                message.type = Message::Type::ping_update;
                message.data.reserve(str.size());
                std::transform(str.begin(), str.end(), std::back_inserter(message.data), [](char c) {return std::byte(c);});
                
                broadcast_message<false>(message);
            }
        );

        m_timer_tasks.emplace_back(
            10ms, [this]() {
                // send system_time
                auto time = clock_t::now() - m_start_time;
                broadcast_message<false>(game_data_message(time));
            }
        );
        spdlog::set_level(spdlog::level::warn);
    }

    void run() {
        ENetEvent event;
        while(int num_events = enet_host_service(m_host , &event, 10) >= 0) {
            auto current_time = clock_t::now();
            if (num_events == 0) {
                spdlog::warn("no events (this is strange, because when no events happen NONE event should appear)");
            }

            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                spdlog::info("connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                auto player = create_player(event.peer->address);
                spdlog::info("added player {}", player.name);
                broadcast_new_player(player);
                for (const auto& old_player : m_players) {
                    send_message<true>({Message::Type::list_update, player_to_bytes(old_player)}, event.peer);
                }
                add_player(player);
                event.peer->data = reinterpret_cast<void*>(player.id);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                spdlog::info("game server got data from {},{}", event.peer->address.host, event.peer->address.port);
                Message message = message_from_chars<uint8_t>({event.packet->data, event.packet->dataLength});
                if (message.type == Message::Type::game_data) {
                    decltype(Player::data) player_data;
                    memcpy(&player_data, message.data.data(), sizeof(player_data));
                    auto player = get_player(event.peer->address);
                    player->data = player_data;
                }
            } else if (event.type == ENET_EVENT_TYPE_NONE) {
                spdlog::info("no events event");
            } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                spdlog::info("peer on port {} disconnected", event.peer->address.port);
                auto player = get_player(event.peer->address);
                m_players.erase(player); 
            } else {
                spdlog::warn("unsupported event type: {}", event.type);
            }

            for (auto& task : m_timer_tasks) {
                auto since_last_execution = std::chrono::duration_cast<std::chrono::milliseconds>(
                        current_time - task.last_execution
                );
                if (since_last_execution > task.execution_period) {
                    task.task();
                }          
            }
            update_screen();
        }
    }

    size_t m_last_num_players = 0;
    void update_screen() {
        using namespace ftxui;
        std::vector<std::vector<Element>> table_data;
        table_data.reserve(m_players.size() + 1);
        auto header_data = { "id", "login", "gamedata", "ping", "ip", "port" };
        std::vector<Element> header;
        header.reserve(header_data.size());
        std::transform(header_data.begin(), header_data.end(), std::back_inserter(header), [](const auto& item) { return text(item); });
        table_data.push_back(std::move(header));
        for (const auto& player : m_players) {
            auto row_data = { std::to_string(player.id), player.name, std::to_string(player.data), std::to_string(player.ping), std::to_string(player.address.host), std::to_string(player.address.port) };
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

    Player create_player(const ENetAddress& address) {
        auto id = generate_id();
        return { generate_name(id), address, id, 0, 0 };
    }

    template<typename T>
    players_t::iterator add_player(T&& player) {
        m_players.push_back(std::forward<T>(player));
        return m_players.end() - 1;
    }

    players_t::iterator get_player(const ENetAddress& address) {
        return std::find_if(m_players.begin(), m_players.end(), [&](const Player& player) { 
                return player.address.host == address.host && player.address.port == address.port; 
        } );
    }

    void broadcast_new_player(const Player& player) {
        return broadcast_message<true>({Message::Type::list_update, player_to_bytes(player)});
    }

    template<bool reliable>
    void broadcast_message(const Message& message) {
        auto peers = get_peers();        
        for (auto& peer : peers) {
            if (peer.state != ENET_PEER_STATE_CONNECTED) {
                continue;
            }
            send_message<reliable>(message, &peer);
        }
    }

    std::span<ENetPeer> get_peers() {
        return {m_host->peers,  m_host->peerCount};

    }

private:
    std::string generate_name(uint64_t id) {
        return s_nicknames[std::hash<uint64_t>{}(id) % s_nicknames.size()];      
    }

    uint32_t generate_id() {
        return m_next_player_id++;
    }

    players_t m_players;
    uint32_t m_next_player_id = 0;
    ENetHost* m_host;
    std::vector<TimerTask<clock_t>> m_timer_tasks;
    typename clock_t::time_point m_start_time;   

    inline static const std::array s_nicknames = {
        "Rames Janor", "Nova", "Deckard Cain", "Dark Wanderer",
        "Malfurion", "Illidan", "Tyrande", "Arthas", "Uther",
        "Sylvanas", "Mario", "Waluigi", "Toad", "Bowser", "Dovakin",
        "Kuro", "Johny Gat", "The Boss", "Steve", "Alex", "We need more lumber",
        "Hornet", "Elderbug", "Ugandan Knuckles", "Samus"
    };
};

