#pragma once

#include <cstdint>
#include <chrono>
#include <string>
#include <span>

#include <glm/glm.hpp>

#include <bytestream.hpp>

#include "game_object.hpp"


using namespace std::chrono_literals;


struct ClientInfo {
    uint32_t client_id;
    uint32_t controlled_object_id;
};

using color_t = glm::vec< 4, uint8_t, glm::defaultp >;
using game_clock_t = std::chrono::steady_clock;

namespace colors {
    inline static color_t red = {255, 0, 0, 255};
    inline static color_t green = {0, 255, 0, 255};
    inline static color_t blue = {0, 0, 255, 255};

    inline color_t create_color(uint32_t idx) {
        color_t colors[] = {red, green, blue};
        return colors[idx % 3];
    }
}

enum class MessageType: uint8_t {
    game_update, ping, list_update, register_player
};

template<typename T>
std::vector<T> span_to_vector(const std::span<T>& span) {
    std::vector<T> result;
    result.reserve(span.size());
    std::copy(span.begin(), span.end(), std::back_inserter(result));
    return result;
}

struct PlayerAddress {
    PlayerAddress(const ENetAddress& address): host(address.host), port(address.port) {}
    PlayerAddress() = default;

    uint32_t host;
    uint16_t port;

    auto operator<=>(const PlayerAddress&) const = default;

    std::vector<std::byte> to_bytes() const {
         size_t size = sizeof(host) + sizeof(port);
         OutByteStream ostr(size);
         ostr << host << port;
         return span_to_vector(ostr.get_span());
    }
};

struct Player {
    Player() = default;
    Player(InByteStream& istr) {
        istr >> name >> address.host >> address.port >> id >> ping;
    }

    bool operator<(const Player& right) const {
        return id < right.id;
    }

    std::string name;
    PlayerAddress address;
    uint32_t id;
    uint32_t ping;

    std::vector<std::byte> to_bytes() const {
        auto address_bytes = address.to_bytes();
        OutByteStream ostr;
        ostr << name << std::span(address_bytes) << id << ping;
        return span_to_vector(ostr.get_span());
    }
};


template<bool is_reliable>
inline void send_bytes(const std::span<std::byte> bytes, ENetPeer* where) {
    const auto reliability_flag = is_reliable ? ENET_PACKET_FLAG_RELIABLE : ENET_PACKET_FLAG_UNSEQUENCED;
    const auto channel_number = is_reliable ? 0 : 1;
    auto packet = enet_packet_create(bytes.data(), bytes.size(), reliability_flag); //TODO: add NO_ALLOC after debug

    enet_peer_send(where, channel_number, packet);

    // enet_packet_destroy(packet);
}


inline static const std::chrono::milliseconds s_client_frame_time = 10ms;
inline static const std::chrono::milliseconds s_server_tick_time = 100ms;
inline static const uint16_t s_game_server_port = 7946;

