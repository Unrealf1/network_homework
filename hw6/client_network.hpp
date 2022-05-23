#pragma once

#include <algorithm>
#include <iterator>
#include <sstream>
#include <string>
#include <tuple>
#include <iostream>
#include <ranges>

#include <enet/enet.h>

#include "client_state.hpp"
#include "spdlog/spdlog.h"


static ENetPeer* connect_to_matchmaking(ENetHost* host) {
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_matchmaking_server_port;
    return enet_host_connect(host, &address, 2, 0);
}

static std::pair<ENetHost*, ENetPeer*> setup_enet() {
    ENetHost* client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
        exit(1);
    }

    auto server = connect_to_matchmaking(client);
    
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
        
        // Ways to exit lobby screen
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
                std::getline(std::cin, state.lobby_creation.name);
                state.lobby_creation.description = fmt::format("{}'s lobby", state.name);
                if (matchmaking->state != ENET_PEER_STATE_CONNECTED) {
                    matchmaking = connect_to_matchmaking(client);
                }
                state.lobby_creation.max_players = 16;
                msg << MessageType::lobby_create << state.lobby_creation;
                send_bytes<true>(msg.get_span(), matchmaking);
            }
            return true;
        }, 50ms);

        state.tasks.add_task([this] {
            lobby_controls();
            return true;
        }, 100ms);
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
    ENetAddress server_address;

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

    void lobby_controls() {
        if (state.mode != ClientMode::in_lobby) {
            return;
        }
        if (state.send_ready) {
            state.send_ready = false;
            OutByteStream msg;
            msg << MessageType::player_ready;
            send_bytes<true>(msg.get_span(), matchmaking);
        }
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
            std::stringstream debug;
            debug << state.lobbies.back().name << ":\n";
            std::ranges::copy(state.lobbies[i].players | std::views::transform([](const auto& player) {return player.name;}), std::ostream_iterator<std::string>(debug));
            //spdlog::warn("{}", debug.str());
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
        } else if (type == MessageType::lobby_start) {
            connect_to_game_server(istr);
        }
    }

    void connect_to_game_server(InByteStream& istr) {
        spdlog::info("Starting connection process...");
        state.mode = ClientMode::connecting;
        auto server_info = istr.get<GameServerInfo>();
        server_address = server_info.address;
        state.tasks.add_task([this]{
            server_connection();
            return state.mode != ClientMode::connected;
        }, 10ms);
    }

    void server_connection() {
        if (state.mode == ClientMode::connecting) {
            if (server != nullptr) {
                OutByteStream msg;
                msg << MessageType::register_player << state.name;
                if (send_bytes<true>(msg.get_span(), server)) {
                    state.mode = ClientMode::connected;
                }
                spdlog::info("registering at game server");
            } else {
                server = enet_host_connect(client, &server_address, 2, 0);
            }
        }
    }

    void ask_for_lobby_update() {
        OutByteStream msg;
        msg << MessageType::lobby_list_update;
        send_bytes<false>(msg.get_span(), matchmaking);
    }

    const int max_events_in_frame = 100;
};
