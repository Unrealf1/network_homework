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
#include <nlohmann/json.hpp>

#include "common.hpp"
#include "timed_task_manager.hpp"

using namespace std::chrono_literals;

class GameServer {
    using players_t = std::vector<Player>;
    using game_clock_t = std::chrono::steady_clock;

public:
    GameServer(ENetHost* host);

    void run();

    size_t m_last_num_players = 0;
    void update_screen();

    Player create_player(const ENetAddress& address) {
        auto id = generate_id();
        return { generate_name(id), address, id, 0 };
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
        OutByteStream ostr;
        ostr << MessageType::list_update << player.to_bytes();
        return broadcast_message<true>(ostr.get_span());
    }

    template<bool reliable>
    void broadcast_message(const std::span<std::byte>& message) {
        auto peers = get_peers();        
        for (auto& peer : peers) {
            if (peer.state != ENET_PEER_STATE_CONNECTED) {
                continue;
            }
            send_bytes<reliable>(message, &peer);
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

    void send_ping() {
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

        OutByteStream ostr;
        ostr << MessageType::ping << str;
        
        broadcast_message<false>(ostr.get_span());
    }

    void update_players() {
        // TODO: send update info to players
    }

    players_t m_players;
    uint32_t m_next_player_id = 0;
    ENetHost* m_host;
    TimedTaskManager<game_clock_t> m_task_manager;
    typename game_clock_t::time_point m_start_time;   

    inline static const std::array s_nicknames = {
        "Rames Janor", "Nova", "Deckard Cain", "Dark Wanderer",
        "Malfurion", "Illidan", "Tyrande", "Arthas", "Uther",
        "Sylvanas", "Mario", "Waluigi", "Toad", "Bowser", "Dovakin",
        "Kuro", "Johny Gat", "The Boss", "Steve", "Alex", "We need more lumber",
        "Hornet", "Elderbug", "Ugandan Knuckles", "Samus"
    };
};

