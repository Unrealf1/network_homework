#include "matchmaking_server.hpp"

#include <algorithm>
#include <bits/ranges_algo.h>
#include <iterator>
#include <stdexcept>
#include <thread>

#include <spdlog/spdlog.h>


void MatchMakingServer::process_new_connection(ENetEvent& event) {
    spdlog::info("new client connected (port: {})", event.peer->address.port);
}

void MatchMakingServer::process_data(ENetEvent& event) {
    InByteStream istr(event.packet->data, event.packet->dataLength);
    MessageType type;
    istr >> type;
    if (type == MessageType::lobby_list_update) {
        send_lobby_list(event.peer);
    } else if (type == MessageType::lobby_create) {
        auto new_lobby = istr.get<server_lobby_t>();
        if (std::ranges::find_if(m_lobbies, [&](const auto& lobby) {return lobby.name == new_lobby.name;}) != m_lobbies.end()) {
            return;
        }
        m_lobbies.push_back(std::move(new_lobby));
        spdlog::info("created new lobby with name: \"{}\"", m_lobbies.back().name);
    } else if (type == MessageType::lobby_join) {
        auto index = istr.get<size_t>();
        if (index > m_lobbies.size()) {
            return;
        }
        if (m_lobbies[index].max_players == m_lobbies[index].players.size()) {
            return;
        }
        spdlog::info("connection player to the lobby {}", index);
        OutByteStream msg;
        msg << MessageType::lobby_join << index;
        send_bytes<true>(msg.get_span(), event.peer);
        m_lobbies[index].players.emplace_back(istr.get<std::string>(), event.peer);
    } else if (type == MessageType::lobby_start) {
        auto index = istr.get<size_t>();
        start_lobby(m_lobbies[index]); 
    } else if (type == MessageType::register_provider) {
        m_providers.emplace_back(event.peer);
    } else if (type == MessageType::server_ready) {
        auto lobby_name = istr.get<std::string>();
        if (!m_pending_games.contains(lobby_name)) {
            return;
        }
        GameServerInfo info;
        istr >> info.address.host >> info.address.port >> info.id;
        auto lobby = std::ranges::find_if(m_lobbies, [&lobby_name](const server_lobby_t& l) {return l.name == lobby_name;});
        if (lobby == m_lobbies.end()) {
            throw std::runtime_error("can't find lobby to launch the game");
        }
        launch_lobby(*lobby, info);
    }
}


void MatchMakingServer::process_disconnect(ENetEvent& event) {
    for (auto& lobby : m_lobbies) {
        auto it = std::ranges::find_if(lobby.players, [&event](const auto& player) {return player.peer->address == event.peer->address;});  
        if (it != lobby.players.end()) {
            lobby.players.erase(it);
        }
    }
}

void MatchMakingServer::send_lobby_list(ENetPeer* to) {
    OutByteStream ostr;
    ostr << MessageType::lobby_list_update << m_lobbies.size();
    for (const auto& lobby : m_lobbies) {
        client_lobby_t client_lobby = {
            .name = lobby.name,
            .description = lobby.description,
            .mods = lobby.mods,
            .players = {},
            .max_players = lobby.max_players,
            .max_mmr = lobby.max_mmr,
            .min_mmr = lobby.min_mmr,
            .avg_mmr = lobby.avg_mmr,
            .state = lobby.state
        };
        client_lobby.players.reserve(lobby.players.size());
        std::ranges::transform(lobby.players, std::back_inserter(client_lobby.players), [](const InnerLobbyPlayer& server_player) -> LobbyPlayer {
            return {server_player.name, server_player.peer->address};
        });
        ostr << client_lobby;
    } 

    send_bytes<false>(ostr.get_span(), to);
}

void MatchMakingServer::start_lobby(server_lobby_t& lobby) {
    m_task_manager.add_task([this, provider = m_providers.begin(), lobby]() mutable -> bool {
        if (provider == m_providers.end()) {
            throw std::runtime_error("could not create game server");
        }

        if (m_pending_games.contains(lobby.name)) {
            provider->request_server(lobby.mods, lobby.name);
            ++provider;
            return true;
        } else {
            return false;
        }
    }, 1000ms);
    m_pending_games.insert(lobby.name);
}

void MatchMakingServer::launch_lobby(server_lobby_t& lobby, const GameServerInfo& server) {
    lobby.state = LobbyState::playing;
    OutByteStream msg;
    msg << MessageType::lobby_start << server;
    for (auto& player : lobby.players) {
        send_bytes<true>(msg.get_span(), player.peer);
    }
    m_pending_games.erase(lobby.name);
}

