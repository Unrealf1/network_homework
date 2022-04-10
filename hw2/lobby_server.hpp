#pragma once

#include <chrono>

#include <enet/enet.h>
#include <spdlog/spdlog.h>


class LobbyServer {
    using clock_t = std::chrono::steady_clock;
public:
    LobbyServer(ENetHost* host)
        : m_host(host)
        , m_start_time(clock_t::now())  
    { }

    void run() {
        ENetEvent event;
        while(int num_events = enet_host_service(m_host , &event, 1000) >= 0) {
            auto current_time = clock_t::now();
            if (num_events == 0) {
                spdlog::warn("no events (this is strange, because when no events happen NONE event should appear)");
            }

            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                spdlog::info("connection with {} established", event.peer->address.host, event.peer->address.port);
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                spdlog::info("got message from peer {}, {}", event.peer->address.host, event.peer->address.port);
                event.packet;
            } else if (event.type == ENET_EVENT_TYPE_NONE) {
                spdlog::info("no events event");
            } else {
                spdlog::warn("unsupported event type: {}", event.type);

            }
        }
    }

private:
    ENetHost* m_host;
    typename clock_t::time_point m_start_time;   
};
