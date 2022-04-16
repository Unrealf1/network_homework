#pragma once

#include <tuple>

#include <enet/enet.h>

#include "client_state.hpp"


static std::pair<ENetHost*, ENetPeer*> setup_enet() {
    ENetHost* client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
        exit(1);
    }
    
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_game_server_port;

    ENetPeer* server = enet_host_connect(client, &address, 2, 0);
    if (server == nullptr) {
        spdlog::error("Cannot connect to the game server");
        exit(2);
    }

    return {client, server};
}

struct Network {
public:
    Network(ClientState& state): state(state) {
        std::tie(client, server) = setup_enet();
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
            }
        }
    }

private:
    ENetHost* client;
    ENetPeer* server;
    ClientState& state;

    void update_objects (InByteStream& istr) {
        auto my_copy = *state.my_object;
        uint32_t num_objects;
        istr >> num_objects;
        spdlog::debug("updating objects (received {})", num_objects);
        state.objects.clear();
        state.objects.reserve(num_objects);
        for (uint32_t i = 0; i < num_objects; ++i) {
            GameObject obj;
            istr >> obj;
            state.objects.push_back(std::move(obj));
        }
        state.objects.push_back(std::move(my_copy));
        state.my_object = &state.objects.back();
    }

    void update_my_object(InByteStream& istr) {
        istr >> *state.my_object;
    }

    void process_registration(InByteStream& istr) {
        istr >> state.my_info;
        spdlog::info("my id is {}, my object id is {}", state.my_info.client_id, state.my_info.controlled_object_id);
        update_objects(istr);
        state.connected = true;
        state.tasks.add_task([&]{
            // send data to the server
            OutByteStream message;
            message << MessageType::game_update;
            message << state.my_object->position;
            send_bytes<false>(message.get_span(), server);
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

    void process_data(InByteStream& istr, const auto& packet) {
        MessageType type;
        istr >> type;
        spdlog::debug("Client got message of type {}! ({} bytes)", type, packet->dataLength);
        if (type == MessageType::game_update) {
            update_objects(istr);
        } else if (type == MessageType::reset) {
            update_my_object(istr);
        } else if (type == MessageType::register_player) {
            process_registration(istr);
        } else if (type == MessageType::list_update) {
            state.players.emplace_back(istr);
        } else if (type == MessageType::ping) {
            process_ping(istr);
        }
    }

    const int max_events_in_frame = 100;
};
