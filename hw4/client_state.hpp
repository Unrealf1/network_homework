#pragma once

#include <vector>
#include <deque>

#include "game_object.hpp"
#include "common.hpp"
#include "timed_task_manager.hpp"


struct Snapshot {
    std::vector<GameObject> objects;
    GameObject my_object;
    vec2 direction;
    game_clock_t::time_point time;
};

struct ClientState {
    TimedTaskManager<game_clock_t> tasks;
    ClientInfo my_info;
    std::vector<GameObject> objects;
    std::vector<Player> players;
    GameObject my_object;
    std::deque<Snapshot> snapshots;
    Snapshot last_snapshot;
    uint32_t snapshot_progress = 0;
    vec2 direction = {0, 0};
    float dt = float(s_client_frame_time.count()) / 1000.0f;
    bool alive = true;
    bool connected = false;
    bool resetting = false;
    bool nosleep = true;
};

