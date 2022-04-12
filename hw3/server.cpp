#include <iostream>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "common.hpp"


ENetHost* create_host() {
    if (int error = enet_initialize() != 0) {
        spdlog::error("could not initialize enet, error code {}", error);
        exit(error);
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = s_game_server_port;
    
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
        exit(1);
    }

    return server;
}

int main() {
    auto server = create_host();
}

