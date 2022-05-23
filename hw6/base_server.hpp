#pragma once

#include <stdexcept>
#include <thread>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "timed_task_manager.hpp"
#include "common.hpp"


inline ENetHost* create_host(uint16_t port) {
    if (int error = enet_initialize() != 0) {
        atexit(enet_deinitialize);
        spdlog::error("could not initialize enet, error code {}", error);
        exit(error);
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;
    
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
    spdlog::info("created host on port {}", server->address.port);

    if (server == nullptr) {
        throw std::runtime_error("could node create enet server");
    }

    return server;
}

template<typename Derived>
class BaseServer {
public:
    BaseServer(ENetHost* host): m_host(host) {}

    void run() {
        spdlog::info("starting matchmaking server");
        auto& self = get_self();
        self.on_start();
        while (m_alive) {
            auto frame_start = game_clock_t::now();
            auto frame_end = frame_start + Derived::s_update_time;
            ENetEvent event;
            while(enet_host_service(m_host , &event, 0) > 0) {
                if (event.type == ENET_EVENT_TYPE_CONNECT) {
                    self.process_new_connection(event);
                } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                    self.process_data(event);
                } else if (event.type == ENET_EVENT_TYPE_NONE) {
                    spdlog::info("no events event");
                } else if (event.type == ENET_EVENT_TYPE_DISCONNECT) {
                    self.process_disconnect(event);
                } else {
                    spdlog::warn("unsupported network event type: {}", event.type);
                }
            }
            m_task_manager.launch();
            self.update();
            std::this_thread::sleep_until(frame_end);
        }
        self.on_finish();
    }

    auto& get_self() {
        return static_cast<Derived&>(*this);
    }

protected:
    TimedTaskManager<game_clock_t> m_task_manager;
    ENetHost* m_host;
    bool m_alive = true;
};

