#pragma once

#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <array>
#include <chrono>
#include <span>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "common.hpp"

using namespace std::chrono_literals;


class GameServer {
    using players_t = std::set<Player>;
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
            1000ms, [this]() {
                // send ping
                auto peers = get_peers();
                Message message;
                message.type = Message::Type::ping_update;
                message.data.resize((sizeof(Player::id) + sizeof(uint32_t)) * peers.size());
                size_t offset = 0;
                for (const auto& peer : peers) {
                    memcpy(
                            message.data.data() + offset, 
                            &peer.data, // this is bad, but it's 3 am, so... 
                            sizeof(Player::id)
                    );
                    offset += sizeof(Player::id);
                    memcpy(message.data.data() + offset, &peer.roundTripTime, sizeof(peer.roundTripTime));
                    offset += sizeof(peer.roundTripTime);
                }
                broadcast_message<false>(message);
            }
        );

        m_timer_tasks.emplace_back(
            1000ms, [this]() {
                // send system_time
                auto time = clock_t::now() - m_start_time;
                broadcast_message<true>(game_data_message(time));
            }
        );
    }

    void run() {
        ENetEvent event;
        while(int num_events = enet_host_service(m_host , &event, 1000) >= 0) {
            auto current_time = clock_t::now();
            if (num_events == 0) {
                spdlog::warn("no events (this is strange, because when no events happen NONE event should appear)");
            }

            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                spdlog::info("connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                auto player = create_player(event.peer->address);
                broadcast_new_player(player);
                add_player(player);
                event.peer->data = reinterpret_cast<void*>(player.id);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                spdlog::warn("game server should not recieve any data");
            } else if (event.type == ENET_EVENT_TYPE_NONE) {
                spdlog::info("no events event");
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
        }
    }

    Player create_player(const ENetAddress& address) {
        auto id = generate_id();
        return { generate_name(id), address, id };
    }

    template<typename T>
    players_t::iterator add_player(T&& player) {
        auto [iter, inserted] = m_players.insert(std::forward<T>(player));
        if (!inserted) {
            spdlog::warn("attempted to add player duplicate");
        }
        return iter;
    }

    void broadcast_new_player(const Player& player) {
        return broadcast_message<true>({Message::Type::list_update, player_to_bytes(player)});
    }

    template<bool reliable>
    void broadcast_message(const Message& message) {
        auto peers = get_peers();        
        for (auto& peer : peers) {
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

