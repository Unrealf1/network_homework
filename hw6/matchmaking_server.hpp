#pragma once

#include <chrono>
#include <unordered_set>
#include <vector>
#include <string_view>

#include <enet/enet.h>

#include "lobby.hpp"
#include "common.hpp"
#include "server_provider.hpp"
#include "base_server.hpp"


class MatchMakingServer: public BaseServer<MatchMakingServer> {
public:
    MatchMakingServer(ENetHost* host): BaseServer(host) {}

    static constexpr std::chrono::milliseconds s_update_time = 100ms;
private:
    std::vector<server_lobby_t> m_lobbies;
    std::vector<ServerProvider> m_providers;
    std::unordered_set<std::string_view> m_pending_games;
  
    void make_game_server(const server_lobby_t& lobby);
    void launch_lobby(server_lobby_t& lobby, const GameServerInfo& server);
public:
    void update() {}
    void on_start() {}
    void on_finish() {}
    void process_new_connection(ENetEvent&);
    void process_data(ENetEvent&);
    void process_disconnect(ENetEvent&);

    void send_lobby_list(ENetPeer*);

    void start_lobby(server_lobby_t&);
};
