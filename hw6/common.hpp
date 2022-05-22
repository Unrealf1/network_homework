#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <span>

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>
#include <enet/enet.h>

#include <bytestream.hpp>

#include "game_object.hpp"


using namespace std::chrono_literals;


struct ClientInfo {
    uint32_t client_id;
    uint32_t controlled_object_id;
};

struct GameServerInfo {
    ENetAddress address;
    uint64_t id;
};

using color_t = glm::vec< 4, uint8_t, glm::defaultp >;
using game_clock_t = std::chrono::steady_clock;
using bytes_t = std::vector<std::byte>;

namespace colors {
    inline static color_t red = {255, 0, 0, 255};
    inline static color_t green = {0, 255, 0, 255};
    inline static color_t blue = {0, 0, 255, 255};

    inline color_t create_color(uint32_t idx) {
        color_t colors[] = {red, green, blue};
        return colors[idx % 3];
    }
}

template<typename T>
void print_bytes(T& stream) {
    auto bytes = stream.get_span();
    std::string msg;
    for (auto b : bytes) {
        msg += fmt::format("{}, ", b);
    }
    spdlog::warn("bytes: {}", msg);
}

enum class MessageType: uint8_t {
    game_update, ping, list_update, 
    register_player, reset, input, 
    lobby_list_update, lobby_start, lobby_create, 
    lobby_join, register_provider, server_ready
    
};

template<typename T>
std::vector<T> span_to_vector(const std::span<T>& span) {
    std::vector<T> result;
    result.reserve(span.size());
    std::copy(span.begin(), span.end(), std::back_inserter(result));
    return result;
}

inline bool operator==(const ENetAddress& first, const ENetAddress& second) {
    return first.host == second.host && first.port == second.port;
}

struct PlayerAddress {
    PlayerAddress(const ENetAddress& address): host(address.host), port(address.port) {}
    PlayerAddress() = default;

    uint32_t host;
    uint16_t port;

    bool operator==(const PlayerAddress& other) const {
        return host == other.host && port == other.port;
    }
};

struct LobbyPlayer {
    std::string name;
    PlayerAddress address;
};

inline OutByteStream& operator<<(OutByteStream& stream, const LobbyPlayer& player) {
    return stream << player.name << player.address.host << player.address.port;
}

inline InByteStream& operator>>(InByteStream& stream, LobbyPlayer& player) {
    return stream >> player.name >> player.address.host >> player.address.port;
}

struct Player : public LobbyPlayer {
    bool operator<(const Player& right) const {
        return id < right.id;
    }

    uint32_t id;
    uint32_t ping;
};

inline OutByteStream& operator<<(OutByteStream& ostr, const Player& player) {
    ostr << static_cast<const LobbyPlayer&>(player);
    ostr << player.id << player.ping;
    return ostr;
}
inline InByteStream& operator>>(InByteStream& istr, Player& player) {
    istr >> static_cast<LobbyPlayer&>(player);
    istr >> player.id >> player.ping;
    return istr;
}


template<bool is_reliable>
inline void send_bytes(const std::span<std::byte>& bytes, ENetPeer* where) {
    const auto reliability_flag = is_reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED;
    const auto channel_number = is_reliable ? 0 : 1;
    auto packet = enet_packet_create(bytes.data(), bytes.size(), reliability_flag); //TODO: add NO_ALLOC after debug

    enet_peer_send(where, channel_number, packet);

    // enet_packet_destroy(packet);
}

template<typename T>
T interpolate(const T& from, const T& to, float ratio) {
    return from + (to - from) * ratio;
}

inline GameObject interpolate(const GameObject& from, const GameObject& to, float ratio) {
    GameObject res = from;
    res.position = interpolate(from.position, to.position, ratio);
    res.velocity = interpolate(from.velocity, to.velocity, ratio);
    res.radius = interpolate(from.radius, to.radius, ratio);
    return res;
}


inline static const std::chrono::milliseconds s_client_frame_time = 20ms;
inline static const std::chrono::milliseconds s_server_tick_time = 40ms;
inline static const vec2 s_simulation_borders = {25, 25};
inline static const uint16_t s_matchmaking_server_port = 7946;

