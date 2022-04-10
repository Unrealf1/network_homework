#include  <spdlog/spdlog.h>
#include  <enet/enet.h>
#include "common.hpp"
#include "game_server.hpp"


int main() {
    setup_logger("Game Server");
    spdlog::info("starting server");

    if (int error = enet_initialize() != 0) {
        spdlog::error("could not initialize enet, error code {}", error);
        exit(error);
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = game_server_port;
    
    size_t peer_count = 32;
    size_t channel_limit = 2;
    uint32_t incoming_bandwith = 0;
    uint32_t outcoming_bandwith = 0;
    ENetHost* server = enet_host_create(
            &address, 
            peer_count, 
            channel_limit, 
            incoming_bandwith, 
            outcoming_bandwith
    );

    if (server == nullptr) {
        spdlog::error("could node create enet server");
    }
    GameServer game_server(server);
    game_server.run();
}

