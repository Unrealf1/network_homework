#include <iostream>

#include <spdlog/spdlog.h>
#include "matchmaking_server.hpp"


int main() {
    spdlog::info("matchmaking...");
    auto host = create_host(s_matchmaking_server_port);
    MatchMakingServer matchmaking(host);
    matchmaking.run();
    enet_host_destroy(host);
}

