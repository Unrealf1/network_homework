#include <thread>

#include <spdlog/spdlog.h>

#include "game_object.hpp"
#include "client_state.hpp"
#include "client_render.hpp"
#include "client_network.hpp"
#include "client_physics.hpp"


int main() {
    ClientState state;
    Render renderer(state);
    Network network(state);
    Physics physics(state);
    
    spdlog::info("starting main loop");
    while(state.alive) {
        auto frame_start_time = game_clock_t::now();
        auto frame_end_time = frame_start_time + s_client_frame_time;
        
        physics.process(state.nosleep ? 1.0f / 100.0f : float(s_client_frame_time.count()) / 1000.0f);

        network.process_events();

        renderer.process_events();
        renderer.check_input();
        renderer.render();

        state.tasks.launch();

        {
            std::string players_msg;
            for (const auto& player : state.players) {
                players_msg += fmt::format("\n{} ({})", player.name, player.ping);
            }
            spdlog::info(players_msg);
        }

        if (!state.nosleep) {
            std::this_thread::sleep_until(frame_end_time);
        }
    }
}

