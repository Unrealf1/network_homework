#pragma once

#include <span>
#include <algorithm>

#include <spdlog/spdlog.h>
#include <enet/enet.h>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>


//TODO fix colors
inline void setup_logger(const std::string& name = "") {
    std::string true_name = name.empty() ? "" : " [" + name + "] ";
    spdlog::set_pattern("[%H:%M:%S.%e]" + std::move(true_name) + "[%l] %v");
}

struct Message {
    enum class Type : uint8_t {
        list_update, game_data, ping_update, start_session
    } type;
    std::vector<std::byte> data;
};

template<bool is_reliable>
inline void send_message(const Message& message, ENetPeer* where) {
    auto buffer_size = sizeof(Message::Type) + message.data.size();
    auto local_buffer = new std::byte[buffer_size];

    memcpy(local_buffer, &(message.type), sizeof(message.type));
    memcpy(local_buffer + sizeof(message.type), message.data.data(), message.data.size());
    //spdlog::warn("buffer[0] is {}", local_buffer[0]);

    const auto reliability_flag = is_reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED;
    const auto channel_number = is_reliable ? 0 : 1;
    auto packet = enet_packet_create(local_buffer, buffer_size, reliability_flag); //TODO: add NO_ALLOC after debug

    enet_peer_send(where, channel_number, packet);

    //enet_packet_destroy(packet);
    delete[] local_buffer;
}

struct Player {
    bool operator<(const Player& right) const {
        return id < right.id;
    }

    std::string name;
    ENetAddress address;
    uint32_t id;
    uint32_t ping;
    int64_t data;
};

inline std::vector<std::byte> player_to_bytes(const Player& player) {
    using nlohmann::json;
    json js;
    js["name"] = player.name;
    js["host"] = player.address.host;
    js["port"] = player.address.port;
    js["id"] = player.id;
    js["ping"] = player.ping;
    js["data"] = player.data;
    auto str = js.dump();
    std::vector<std::byte> result;
    result.reserve(str.size());
    std::transform(str.begin(), str.end(), std::back_inserter(result), [](char c) {return std::byte(c);});
    return result; 
}

inline nlohmann::json json_from_bytes(std::span<std::byte> bytes) {
    using nlohmann::json;
    std::string str;
    str.reserve(bytes.size());
    std::transform(bytes.begin(),bytes.end(), std::back_inserter(str), [](std::byte b) {return char(b);});
    return json::parse(std::move(str));
}

inline Player player_from_bytes(std::span<std::byte> bytes) {
    auto js = json_from_bytes(bytes);
    return {
        .name = js["name"],
        .address = { js["host"], js["port"]  },
        .id = js["id"],
        .ping = js["ping"],
        .data = js["data"]
    };
}

inline Message message_from_bytes(std::span<std::byte> bytes) {
    Message::Type type = *reinterpret_cast<Message::Type*>(bytes.data());
    return { type, { bytes.begin() + sizeof(Message::Type), bytes.end() } };
}

template<typename T>
inline Message message_from_chars(std::span<T> chars) {
    return message_from_bytes({reinterpret_cast<std::byte*>(chars.data()), chars.size()});
}

inline Message game_data_message(auto time) {
    auto data = decltype(Player::data)(std::chrono::duration_cast<std::chrono::milliseconds>(time).count());
    Message message;
    message.type = Message::Type::game_data;
    auto bytes = reinterpret_cast<std::byte*>(&data);
    message.data = {bytes, bytes + sizeof(data)};
    return message;
}


inline static const uint16_t s_game_server_port = 7946;
inline static const uint16_t s_lobby_server_port = 7964;

