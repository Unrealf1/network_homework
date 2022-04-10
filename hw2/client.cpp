#include <algorithm>
#include <iostream>
#include <future>
#include <chrono>

#include <iterator>
#include <spdlog/spdlog.h>
#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <ftxui/dom/elements.hpp> 
#include <ftxui/dom/node.hpp>
#include <ftxui/dom/table.hpp> 
#include <ftxui/screen/screen.hpp>

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

    auto client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
        exit(1);
    }

    
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_lobby_server_port;

    auto lobby = enet_host_connect(client, &address, 2, 0);
    if (lobby == nullptr) {
        spdlog::error("Cannot connect to lobby");
        exit(2);
    }

    bool is_gaming = false;
    decltype(Player::data) server_data = 0;
    auto last_gaming_update = std::chrono::steady_clock::now();
    auto gaming_update_interval = 100ms;
    std::unordered_map<decltype(Player::id), decltype(Player::ping)> others;

    ENetPeer* game_server = nullptr;
    auto start_time = std::chrono::steady_clock::now();

    auto send_gaming_update = [&]() {
        if (game_server == nullptr) {
            spdlog::error("cannot send game update to the server: not connected");
            exit(3);
        }
        auto time =  std::chrono::steady_clock::now() - start_time;
        send_message<true>(game_data_message(time), game_server);
        spdlog::info("sent update to the server");
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
        spdlog::info("now gaming!");
        spdlog::set_level(spdlog::level::warn);
    };
    auto process_game_message = [&](Message& message) {
        spdlog::info("got data from server");
        if (message.type == Message::Type::game_data) {
            spdlog::info("game data");
            memcpy(&server_data, message.data.data(), sizeof(server_data));
        } else if (message.type == Message::Type::ping_update) {
            spdlog::info("ping");
            auto json = json_from_bytes(message.data);
            for (const auto& js : json) {
                auto player = others.find(js["id"]);
                if (player == others.end()) {
                    continue;
                }
                player->second = js["ping"];
            }
        } else if (message.type == Message::Type::list_update) {
            spdlog::info("list update");
            auto player = player_from_bytes(message.data);
            others.insert({player.id, 0});
        } else {
            spdlog::warn("unknown message type from server");
        }
    };

    auto process_message = [&](Message& message, ENetAddress& sender) {
        if (sender.port == s_lobby_server_port) {
            process_lobby_message(message);
        } else {
            process_game_message(message);
        }
    };

    auto display_others = [&]() {
        using namespace ftxui;
        std::vector<std::vector<Element>> table_data;
        table_data.reserve(others.size() + 1);
        auto header_data = { "id", "ping" };
        std::vector<Element> header;
        header.reserve(header_data.size());
        std::transform(header_data.begin(), header_data.end(), std::back_inserter(header), [](const auto& item) { return text(item); });
        table_data.push_back(std::move(header));
        for (const auto& [id, ping] : others) {
            auto row_data = { std::to_string(id), std::to_string(ping) };
            std::vector<Element> row;
            std::transform(row_data.begin(), row_data.end(), std::back_inserter(row), [](const auto& item) { return text(item); });
            table_data.push_back(std::move(row));            
        }

        auto table = Table(table_data);
        table.SelectAll().Border(LIGHT);
        table.SelectRow(0).Decorate(bold);
        table.SelectRow(0).SeparatorVertical(LIGHT);
        table.SelectRow(0).Border(DOUBLE);
        auto server_state = fmt::format("server state: {}", server_data);
        auto document = table.Render();
        auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
        std::cout << server_state << '\n';
        Render(screen, document);
        screen.Print();
        std::cout << screen.ResetPosition();
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
        int num_events = enet_host_service(client , &event, 10) >= 0;
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
                last_gaming_update = time;
            }
            display_others();
        }
    }
}

