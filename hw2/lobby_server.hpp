#pragma once

#include <bits/ranges_algo.h>
#include <chrono>

#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include "common.hpp"


class LobbyServer {
    using clock_t = std::chrono::steady_clock;
public:
    LobbyServer(ENetHost* host)
        : m_host(host)
        , m_start_time(clock_t::now())  
    {
        enet_address_set_host(&m_game_server_address, "localhost");
        m_game_server_address.port = s_game_server_port;
    }

    void run() {
        ENetEvent event;
        while(int num_events = enet_host_service(m_host , &event, 1000) >= 0) {
            if (num_events == 0) {
                spdlog::warn("no events (this is strange, because when no events happen NONE event should appear)");
            }

            if (event.type == ENET_EVENT_TYPE_CONNECT) {
                spdlog::info("connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                if (m_session_active) {
                    send_message<true>(create_start_session_message(), event.peer);
                }
            } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
                spdlog::info("got message from peer {}, {}", event.peer->address.host, event.peer->address.port);
                auto message = message_from_chars<enet_uint8>({ event.packet->data, event.packet->dataLength} );
                if (message.type == Message::Type::start_session) {
                    spdlog::info("starting a game session!");
                    start_session();
                }
            } else if (event.type == ENET_EVENT_TYPE_NONE) {
                spdlog::info("no events event");
            } else {
                spdlog::warn("unsupported event type: {}", event.type);

            }
        }
    }

    void start_session() {
        m_session_active = true;
        broadcast_message<true>(create_start_session_message());
    }

    Message create_start_session_message() {
        using nlohmann::json;
        json js;
        js["host"] = m_game_server_address.host;
        js["port"] = m_game_server_address.port;
        std::string str = js.dump();
        Message message;
        message.type = Message::Type::start_session;
        message.data.reserve(str.size());
        std::ranges::transform(str, std::back_inserter(message.data), [](char c) {return std::byte(c);});
        return message;
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
    ENetAddress m_game_server_address;
    ENetHost* m_host;
    typename clock_t::time_point m_start_time;
    bool m_session_active = false;
};
