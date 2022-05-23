#pragma once

#include <string>
#include <vector>

#include <bytestream.hpp>

#include "common.hpp"


struct Mod {
    Mod() = default;

    Mod(const std::string&) {

    }

    std::string to_str() const {
        return {};
    }
};

struct InnerLobbyPlayer {
    std::string name;
    ENetPeer* peer;
};

enum class LobbyState : uint8_t {
    waiting, playing
};
template<typename player_t>
struct Lobby {
    std::string name;
    std::string description;
    std::vector<Mod> mods;
    std::vector<player_t> players;
    uint8_t max_players;
    uint16_t max_mmr;
    uint16_t min_mmr;
    uint16_t avg_mmr;
    LobbyState state;
};

using client_lobby_t = Lobby<LobbyPlayer>;
using server_lobby_t = Lobby<InnerLobbyPlayer>;

template<typename player_t>
inline OutByteStream& operator<<(OutByteStream& ostr, const Lobby<player_t>& lobby) {
    ostr << lobby.name << lobby.description << lobby.mods.size();
    for (const auto& mod : lobby.mods) {
        ostr << mod;
    }
    return ostr << lobby.max_players 
        << lobby.max_mmr << lobby.min_mmr 
        << lobby.avg_mmr << lobby.state;
}

template<typename player_t>
inline InByteStream& operator>>(InByteStream& istr, Lobby<player_t>& lobby) {
    istr >> lobby.name >> lobby.description;

    size_t mods_size;
    istr >> mods_size;
    lobby.mods.reserve(mods_size);
    for (size_t i = 0; i < mods_size; ++i) {
        lobby.mods.push_back(istr.get<Mod>());
    }

    return istr >> lobby.max_players >> lobby.max_mmr >> lobby.min_mmr >> lobby.avg_mmr >> lobby.state;
}
