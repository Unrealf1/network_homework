#pragma once

#include "common.hpp"
#include "client_state.hpp"
#include "glm/geometric.hpp"
#include "spdlog/spdlog.h"
#include "game_server.hpp"


class Physics {
public:
    Physics(ClientState& state) : state(state) {}
    
    void process(float dt) {
        if (state.mode != ClientMode::connected) {
            return;
        }
        process_object(state.my_object, state.direction, dt);

        if (state.snapshots.empty()) {
            spdlog::error("no snapshots available");
            return;
        }

        spdlog::debug("have {} snapshots in flight", state.snapshots.size());
        state.nosleep = state.snapshots.size() > 2;
        
        if (state.snapshot_progress == GameServer::s_update_time / s_client_frame_time) {
            // there's snapshot available and previous was finished.
            // moving to the next spanphot
            state.snapshot_progress = 0;
            state.last_snapshot = state.snapshots.front();

            state.objects = state.last_snapshot.objects;
            
            check_reset();
            state.snapshots.pop_front();
        }

        if (state.snapshots.empty()) {
            spdlog::error("no snapshots available after snapshot update");
            return;
            // wait
        } 

        auto& next_snapshot = state.snapshots.front();
        float snapshot_ratio = float(state.snapshot_progress) / float(GameServer::s_update_time / s_client_frame_time);
        for (size_t i = 0; i < state.objects.size(); ++i) {
            if (i == next_snapshot.objects.size()) {
                break;
            }
            state.objects[i].position = interpolate(state.last_snapshot.objects[i].position, next_snapshot.objects[i].position, snapshot_ratio);
            state.objects[i].velocity = interpolate(state.last_snapshot.objects[i].velocity, next_snapshot.objects[i].velocity, snapshot_ratio);
            state.objects[i].radius = interpolate(state.last_snapshot.objects[i].radius, next_snapshot.objects[i].radius, snapshot_ratio);
        }


        ++state.snapshot_progress;
        spdlog::debug("snapshot progress {}", state.snapshot_progress);
    }

private:
    ClientState& state;

    void process_object(GameObject& object, vec2 direction, float dt) {
        if (glm::length(direction) > 0.5f) {
            float speed = GameServer::speed_of_object(object);
            object.velocity = direction * speed;
        }
        GameServer::object_physics(object, dt);
        GameServer::object_borders(object);
    }

    void check_reset() {
        const auto& snapshot = state.last_snapshot;
        auto iter = std::find_if(
                snapshot.objects.begin(), snapshot.objects.end(), 
                [&](const auto& obj) { return obj.id == snapshot.my_object.id; } 
        );

        if (iter == state.objects.end()) {
            spdlog::error("cannot find my object in snapshot");
        } else {
            GameObject snapshot_object = *(iter);
            if (snapshot_object.radius != snapshot.my_object.radius || glm::distance(snapshot_object.position, snapshot.my_object.position) > 0.1f) {
                //TODO: reset
                spdlog::warn("resetting physics");
                GameObject new_estimation = snapshot_object;

                for (const Snapshot& snap : state.snapshots) {
                    process_object(new_estimation, snap.direction, float(GameServer::s_update_time.count()) / 1000.0f);
                }
                state.from_interpolation = state.my_object;
                state.my_object = new_estimation;
                state.interpolation_progress = 0;
                state.interpolation_length = 100 / s_client_frame_time.count();
            }
        }
    }
};
