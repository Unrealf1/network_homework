#pragma once

#include <vector>
#include <deque>

#include "game_object.hpp"
#include "common.hpp"
#include "timed_task_manager.hpp"
#include "lobby.hpp"
#include "secrets.hpp"


struct Snapshot {
    std::vector<GameObject> objects;
    GameObject my_object;
    vec2 direction;
    game_clock_t::time_point time;
};

enum class ClientMode {
    matchmaking, in_lobby, connected, connecting
};

struct ClientState {
    std::string name;
    TimedTaskManager<game_clock_t> tasks;
    ClientInfo my_info;
    std::vector<GameObject> objects;
    std::vector<Player> players;
    std::vector<client_lobby_t> lobbies;
    size_t chosen_lobby;
    bool should_connect = false;
    bool should_create = false;
    bool send_ready = false;
    client_lobby_t lobby_creation = {};
    GameObject my_object;
    GameObject from_interpolation;
    uint32_t interpolation_progress;
    uint32_t interpolation_length;
    std::deque<Snapshot> snapshots;
    Snapshot last_snapshot;
    uint32_t snapshot_progress = 0;
    vec2 direction = {0, 0};
    float dt = float(s_client_frame_time.count()) / 1000.0f;
    bool alive = true;
    ClientMode mode = ClientMode::matchmaking;
    secret_t<s_secret_size> secret;

    bool nasty = false;
    bool resetting = false;
    bool nosleep = true;
};

