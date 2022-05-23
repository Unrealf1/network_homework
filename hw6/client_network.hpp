#pragma once

#include <algorithm>
#include <tuple>
#include <iostream>

#include <enet/enet.h>

#include "client_state.hpp"
#include "spdlog/spdlog.h"


static std::pair<ENetHost*, ENetPeer*> setup_enet() {
    ENetHost* client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
        exit(1);
    }
    
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_matchmaking_server_port;

    ENetPeer* server = enet_host_connect(client, &address, 2, 0);
    if (server == nullptr) {
        spdlog::error("Cannot connect to the matchmaking server");
        exit(2);
    }

    return {client, server};
}

struct Network {
public:
    Network(ClientState& state): state(state) {
        std::tie(client, matchmaking) = setup_enet();
        atexit(enet_deinitialize);
        state.tasks.add_task([this]{
            ask_for_lobby_update();
            return true;
        }, 1s);

        state.tasks.add_task([this] {
            auto& state = this->state;
            if (state.mode != ClientMode::matchmaking) {
                return false;
            }
            if (state.should_connect) {
                state.should_connect = false;
                if (state.lobbies.empty()) {
                    return true;
                }
                OutByteStream msg;
                msg << MessageType::lobby_join << state.chosen_lobby << state.name;
                send_bytes<true>(msg.get_span(), matchmaking);
            } else if (state.should_create) {
                state.should_create = false;
                spdlog::info("requesting lobby creation");
                OutByteStream msg;
                std::cout << "Please enter lobby name\n";
                std::cin >> state.lobby_creation.name;
                std::cout << "Enter lobby description\n";
                std::cin >> state.lobby_creation.description;
                state.lobby_creation.max_players = 16;
                msg << MessageType::lobby_create << state.lobby_creation;
                send_bytes<true>(msg.get_span(), matchmaking);
            }
            return true;
        }, 50ms);
    }
    ~Network() {
        enet_host_destroy(client);
    }

    void process_events() {
        int processed_enet_events = 0;
        ENetEvent net_event;
        while ( enet_host_service(client , &net_event, 0) > 0 
                && processed_enet_events < max_events_in_frame) 
        {
            ++processed_enet_events;
            if (net_event.type == ENET_EVENT_TYPE_RECEIVE) {
                InByteStream istr(net_event.packet->data, net_event.packet->dataLength);
                process_data(istr, net_event.packet);
                enet_packet_destroy(net_event.packet);
            }
        }
    }

private:
    ENetHost* client;
    ENetPeer* server = nullptr;
    ENetPeer* matchmaking;
    ClientState& state;

    Snapshot parse_snapshot(InByteStream& istr) {
        uint32_t num_objects;
        istr >> num_objects;
        std::vector<GameObject> objects;
        objects.reserve(num_objects);
        for (uint32_t i = 0; i < num_objects; ++i) {
            GameObject obj;
            istr >> obj;
            objects.push_back(std::move(obj));
        }
        return {objects, state.my_object, state.direction, game_clock_t::now()};
    }

    void process_snapshot(InByteStream& istr) {
        spdlog::debug("Got snapshot from the server");
        state.snapshots.push_back(parse_snapshot(istr));
    }

    void process_registration(InByteStream& istr) {
        istr >> state.my_info;
        spdlog::info("my id is {}, my object id is {}", state.my_info.client_id, state.my_info.controlled_object_id);
        state.last_snapshot = parse_snapshot(istr);
        state.my_object = *std::ranges::find_if(state.last_snapshot.objects, [&](const auto& obj) {return obj.id == state.my_info.controlled_object_id;});
        state.mode = ClientMode::connected;
        state.tasks.add_task([&]{
            // send data to the server
            OutByteStream message;
            message << MessageType::input;
            message << state.direction;
            send_bytes<false>(message.get_span(), server);
            return true;
        }, 10ms);
    }

    void process_ping(InByteStream& istr) {
        uint32_t num_players;
        istr >> num_players;
        for (uint32_t i = 0; i < num_players; ++i) {
            uint32_t id;
            uint32_t ping;
            istr >> id >> ping;
            auto player = std::find_if(state.players.begin(), state.players.end(), 
                    [&](const Player& player) {return player.id == id;});
            if (player == state.players.end()) {
                spdlog::warn("got unknown player in ping update: id = {}", id);
                continue;
            }
            player->ping = ping;
        }
    }

    void process_lobby_list_update(InByteStream& istr) {
        state.lobbies.clear();
        auto sz = istr.get<size_t>();
        state.lobbies.reserve(sz);
        for (size_t i = 0; i < sz; ++i) {
            state.lobbies.push_back(istr.get<client_lobby_t>());
        }
    }

    void process_data(InByteStream& istr, const auto& packet) {
        MessageType type;
        istr >> type;
        spdlog::debug("Client got message of type {}! ({} bytes)", type, packet->dataLength);
        if (type == MessageType::game_update) {
            process_snapshot(istr);
        } else if (type == MessageType::register_player) {
            process_registration(istr);
        } else if (type == MessageType::list_update) {
            state.players.emplace_back(istr.get<LobbyPlayer>());
        } else if (type == MessageType::ping) {
            process_ping(istr);
        } else if (type == MessageType::lobby_list_update) {
            process_lobby_list_update(istr);
        } else if (type == MessageType::lobby_join) {
            state.mode = ClientMode::in_lobby;
        }
    }

    void ask_for_lobby_update() {
        OutByteStream msg;
        msg << MessageType::lobby_list_update;
        send_bytes<false>(msg.get_span(), matchmaking);
    }

    const int max_events_in_frame = 100;
};
