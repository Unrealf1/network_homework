#pragma once

#include <span>

#include <spdlog/spdlog.h>
#include <enet/enet.h>
#include <nlohmann/json.hpp>



//TODO fix colors
inline void setup_logger(const std::string& name = "") {
    std::string true_name = name.empty() ? "" : " [" + name + "] ";
    spdlog::set_pattern("[%H:%M:%S.%e]" + std::move(true_name) + "[%l] %v");
}

struct Message {
    enum class Type {
        list_update, game_data, ping_update, start_session
    } type;
    std::vector<std::byte> data;
};

template<bool is_reliable>
inline void send_message(const Message& message, ENetPeer* where) {
    auto buffer_size = sizeof(Message::Type) + message.data.size();
    auto local_buffer = new std::byte[buffer_size];   

    const auto reliability_flag = is_reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED;
    const auto channel_number = is_reliable ? 0 : 1;
    auto packet = enet_packet_create(local_buffer, buffer_size, reliability_flag | ENET_PACKET_FLAG_NO_ALLOCATE);

    enet_peer_send(where, channel_number, packet);

    enet_packet_destroy(packet);
    delete[] local_buffer;
}

struct Player {
    bool operator<(const Player& right) const {
        return id < right.id;
    }

    std::string name;
    ENetAddress address;
    uint32_t id;
};

inline std::vector<std::byte> player_to_bytes(const Player& player) {
    using nlohmann::json;
    json js;
    auto data_size = sizeof(size_t) 
        + player.name.size() 
        + sizeof(player.address.host) 
        + sizeof(player.address.port)
        + sizeof(player.id);
    std::vector<std::byte> result(data_size);
    size_t name_size = player.name.size();
    memcpy(result.data(), &name_size, sizeof(name_size));
    size_t offset = name_size;
    memcpy(result.data() + offset, player.name.c_str(), player.name.size());
    offset += player.name.size();
    // assume identical endianness. hope it's ok  for this task. Better use bson or protobuf
    memcpy(result.data() + offset, &player.address.host, sizeof(player.address.host));
    offset += sizeof(player.address.host);
    memcpy(result.data() + offset, &player.address.port, sizeof(player.address.port));
    offset += sizeof(player.address.port);
    memcpy(result.data() + offset, &player.id, sizeof(player.id));
    return result;
}
/*
inline Player player_from_bytes(std::span<std::byte> bytes) {
    size_t name_size;

}
*/
inline static const uint16_t game_server_port = 7946;
inline static const uint16_t lobby_server_port = 7964;

