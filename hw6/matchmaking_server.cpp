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
        auto client_lobby = istr.get<client_lobby_t>();
        auto new_lobby = server_lobby_t {
            .name = client_lobby.name,
            .description = client_lobby.description,
            .mods = client_lobby.mods,
            .players = {},
            .max_players = client_lobby.max_players,
            .max_mmr = client_lobby.max_mmr,
            .min_mmr = client_lobby.min_mmr,
            .avg_mmr = client_lobby.avg_mmr,
            .state = client_lobby.state
        };
        if (std::ranges::find_if(m_lobbies, [&](const auto& lobby) {return lobby.name == new_lobby.name;}) != m_lobbies.end()) {
            spdlog::warn("failed to create lobby with name {}, as this name is already taken", new_lobby.name);
            return;
        }
        spdlog::info("creating lobby {}", new_lobby.name);
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
        m_lobbies[index].players.emplace_back(istr.get<std::string>(), event.peer, false);
        spdlog::info("not lobby has {} players", m_lobbies[index].players.size());
    } else if (type == MessageType::lobby_start) {
        auto index = istr.get<size_t>();
        start_lobby(m_lobbies[index]); 
    } else if (type == MessageType::register_provider) {
        spdlog::info("registered new server provider on port {}", event.peer->address.port);
        m_providers.emplace_back(event.peer);
    } else if (type == MessageType::server_ready) {
        auto lobby_name = istr.get<std::string>();
        spdlog::info("server for lobby {} is ready", lobby_name);
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
    } else if (type == MessageType::set_name) {
        auto name = istr.get<std::string>();
        for (auto& lobby : m_lobbies) {
            auto it = std::ranges::find_if(lobby.players, [&event](const auto& player) {return player.peer->address == event.peer->address;});  
            if (it != lobby.players.end()) {
                it->name = name;
            }
        }
    } else if (type == MessageType::player_ready) {
        spdlog::info("player ready");
        for (auto& lobby : m_lobbies) {
            auto it = std::ranges::find_if(lobby.players, [&event](const auto& player) {return player.peer->address == event.peer->address;});  
            if (it != lobby.players.end()) {
                it->ready = !it->ready;
                spdlog::info("changing ready for player {} in lobby {}, now {}", it->name, lobby.name, it->ready);
            }
        }
    }
}


void MatchMakingServer::process_disconnect(ENetEvent& event) {
    spdlog::info("client disconnected (port: {})", event.peer->address.port);
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
            return {server_player.name, server_player.peer->address, server_player.ready};
        });
        ostr << client_lobby;
    } 

    send_bytes<false>(ostr.get_span(), to);
}

void MatchMakingServer::start_lobby(server_lobby_t& lobby) {
    m_task_manager.add_task([this, provider = m_providers.begin(), lobby]() mutable -> bool {
        spdlog::info("attempt {} to create a server for lobby {}", provider - m_providers.begin(), lobby.name);

        if (m_pending_games.contains(lobby.name)) {
            if (provider == m_providers.end()) {
                //throw std::runtime_error("could not create game server");
                return false;
            }

            provider->request_server(lobby.mods, lobby.name);
            ++provider;
            return true;
        } else {
            return false;
        }
    }, 5s);
    m_pending_games.insert(lobby.name);
}

void MatchMakingServer::launch_lobby(server_lobby_t& lobby, const GameServerInfo& server) {
    lobby.state = LobbyState::playing;
    OutByteStream msg;
    msg << MessageType::lobby_start << server;
    for (auto& player : lobby.players) {
        spdlog::info("lanching client on port {}", player.peer->address.port);
        while (send_bytes<true>(msg.get_span(), player.peer) != 0) {

        }
    }
    m_pending_games.erase(lobby.name);
    spdlog::info("launched lobby {}", lobby.name);
}

void MatchMakingServer::on_start() {
    m_task_manager.add_task([this] {
        for (auto& lobby : m_lobbies) {
            if (lobby.players.size() > 0 && std::ranges::all_of(lobby.players, [](const auto& player){return player.ready;}) 
                    && !m_pending_games.contains(lobby.name) && lobby.state != LobbyState::playing) {
                start_lobby(lobby);
            }
        }
        return true;
    }, 1s);
}

