#include <iostream>
#include <spdlog/spdlog.h>

#include "game_server.hpp"
#include "lobby.hpp"


int main(int argc, char** argv) {
    if (argc < 3) {
        spdlog::error("not enough command line arguments. Usage: [port], [name], [mods...]");
        return 1;
    }

    uint16_t port = std::stoul(argv[1]);
    auto name = argv[2];
    std::vector<Mod> mods;
    for (int i = 3; i < argc; ++i) {
        mods.emplace_back(argv[i]);
    }

    auto host = create_host(port);
    GameServer server(host, name, port);
    server.run();
}
