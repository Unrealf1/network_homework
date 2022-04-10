#include <algorithm>
#include <iostream>
#include <future>
#include <chrono>

#include <iterator>
#include <spdlog/spdlog.h>
#include <enet/enet.h>
#include <nlohmann/json.hpp>

#include "common.hpp"

using namespace std::chrono_literals;
using nlohmann::json;


std::string get_user_input() {
    std::string res;
    std::getline(std::cin, res);
    return res;
}

int main() {
    setup_logger("client");
    spdlog::info("starting client");

    auto client = enet_host_create(nullptr, 1, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
    }

    
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_lobby_server_port;

    auto lobby = enet_host_connect(client, &address, 2, 0);
    if (lobby == nullptr) {
        spdlog::error("Cannot connect to lobby");
        return 1;
    }

    bool is_gaming = false;
    auto last_gaming_update = std::chrono::steady_clock::now();
    auto gaming_update_interval = 100ms;

    ENetPeer* game_server = nullptr;
    auto start_time = std::chrono::steady_clock::now();

    auto send_gaming_update = [&]() {
        if (game_server == nullptr) {
            spdlog::error("cannot send game update to the server: not connected");
            exit(1);
        }
        auto time = start_time - std::chrono::steady_clock::now();
        send_message<true>(game_data_message(time), game_server);
    };
    
    auto start_session = [&]() {
        Message msg;
        msg.type = Message::Type::start_session;
        send_message<true>(msg, lobby);
    };

    auto process_lobby_message = [&](Message& message) {
        if (message.type != Message::Type::start_session) {
            spdlog::error("unexpected message type from lobby server, skipping");
            return;
        }

        std::string str;
        str.reserve(message.data.size());
        std::ranges::transform(message.data, std::back_inserter(str), [](std::byte b) { return char(b); });
        
        json js = json::parse(str);
        ENetAddress game_address;
        game_address.port = js["port"];
        game_address.host = js["host"];
        spdlog::info("Connecting to the game server ({}:{})", game_address.host, game_address.port);
        game_server = enet_host_connect(client, &game_address, 2, 0);
        if (game_server == nullptr) {
            spdlog::error("Cannot connect to the game server");
            exit(2);
        }
        is_gaming = true;
    };
    auto process_game_message = [&](Message& message) {
        spdlog::info("god data from server");
        spdlog::warn("TODO: display this data");
        return;
    };

    auto process_message = [&](Message& message, ENetAddress& sender) {
        if (sender.port == s_lobby_server_port) {
            process_lobby_message(message);
        } else {
            process_game_message(message);
        }
    };

    auto user_input = std::async(std::launch::async, get_user_input);

    while (true) {
        if (user_input.wait_for(0ms) == std::future_status::ready) {
            auto user_string = user_input.get();
            spdlog::info("user input: \"{}\"", user_string);
            user_input = std::async(std::launch::async, get_user_input);
            if (user_string == "start") {
                start_session();
            }
        }
        ENetEvent event;
        int num_events = enet_host_service(client , &event, 1000) >= 0;
        if (num_events == 0) {
            spdlog::warn("no events (this is strange, because when no events happen NONE event should appear)");
        }

        if (event.type == ENET_EVENT_TYPE_CONNECT) {
            spdlog::warn("someone is connectiong to client. weird. (one time is ok, it is lobby server) {}, {}", event.peer->address.host, event.peer->address.port);
        } else if (event.type == ENET_EVENT_TYPE_RECEIVE) {
            spdlog::info("Client got message!");
            auto message = message_from_chars<enet_uint8>({ event.packet->data, event.packet->dataLength });
            process_message(message, event.peer->address);
        } else if (event.type == ENET_EVENT_TYPE_NONE) {
           
        }  else {
            spdlog::warn("unknown event type");
        }

        if (is_gaming) {
            auto time = std::chrono::steady_clock::now();
            if (time - last_gaming_update > gaming_update_interval) {
                send_gaming_update();
            }
        }
    }
}

