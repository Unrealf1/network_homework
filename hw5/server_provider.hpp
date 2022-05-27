#pragma once

#include <optional>
#include <enet/enet.h>

#include "common.hpp"
#include "lobby.hpp"


class ServerProvider {
public:
    ServerProvider(ENetPeer* peer): proxy(peer) {}

    void request_server(const std::vector<Mod>& mods, const std::string& name) {
        spdlog::info("asking port {} for a new server", proxy->address.port);
        OutByteStream msg;
        msg << MessageType::lobby_start << name << mods.size();
        for (const auto& mod : mods) {
            msg << mod;
        }
        send_bytes<true>(msg.get_span(), proxy);
    }

private:
    ENetPeer* const proxy;
};
