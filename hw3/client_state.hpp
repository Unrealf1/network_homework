#pragma once

#include <vector>
#include "game_object.hpp"
#include "common.hpp"
#include "timed_task_manager.hpp"


struct ClientState {
    TimedTaskManager<game_clock_t> tasks;
    ClientInfo my_info;
    std::vector<GameObject> objects;
    std::vector<Player> players;
    GameObject* my_object;
    float speed = 1.0f;
    float dt = float(s_client_frame_time.count()) / 1000.0f;
    bool alive = true;
    bool connected = false;
};

