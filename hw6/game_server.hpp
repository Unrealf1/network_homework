#pragma once

#include <algorithm>
#include <iterator>
#include <set>
#include <map>
#include <string>
#include <cstdint>
#include <array>
#include <chrono>
#include <span>
#include <iostream>

#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "common.hpp"
#include "glm/geometric.hpp"
#include "timed_task_manager.hpp"
#include "base_server.hpp"

using namespace std::chrono_literals;

struct Robot {
    vec2 current_goal;
    uint32_t object_id;
};

class GameServer: public BaseServer<GameServer> {
    using players_t = std::vector<Player>;
    using objects_t = std::vector<GameObject>;
    using game_clock_t = std::chrono::steady_clock;

public:
    GameServer(ENetHost* host, const std::string& name, uint32_t id);

    //void run();
    void on_start();
    void on_finish();
    void update();
    void process_new_connection(ENetEvent&);
    void process_data(ENetEvent&);
    void process_disconnect(ENetEvent&);



    static void object_physics(GameObject& object, float dt);

    static float speed_of_object(const GameObject& object) {
        return 5.0f / (object.radius + 1.0f);
    }

    static void object_borders(GameObject& obj) {
        if (obj.position.x + obj.radius > s_simulation_borders.x) {
            obj.position.x = s_simulation_borders.x - obj.radius;
        }
        if (obj.position.x - obj.radius < 0) {
            obj.position.x = obj.radius;
        }
        if (obj.position.y + obj.radius > s_simulation_borders.y) {
            obj.position.y = s_simulation_borders.y - obj.radius;
        }
        if (obj.position.y - obj.radius < 0) {
            obj.position.y = obj.radius;
        }
    }

    static constexpr std::chrono::milliseconds s_update_time = 60ms;
private:
    size_t m_last_num_players = 0;
    std::string m_name;
    uint64_t m_id;

    void update_screen();


    void update_physics(float dt);

    Player create_player(const ENetAddress& address, const std::string& name);

    GameObject create_game_object() {
        GameObject obj = {
            .position = vec2{0, 0},
            .velocity = vec2{0, 0},
            .radius = 1.0f,
            .id = generate_obj_id()
        };
        random_teleport(obj);
        return obj;
    }

    template<typename T>
    players_t::iterator add_player(T&& player) {
        m_players.push_back(std::forward<T>(player));
        return m_players.end() - 1;
    }

    players_t::iterator get_player(const ENetAddress& address) {
        return std::find_if(m_players.begin(), m_players.end(), [&](const Player& player) { 
                return player.address.host == address.host && player.address.port == address.port; 
        } );
    }

    void broadcast_new_player(const Player& player) {
        OutByteStream ostr;
        ostr << MessageType::list_update << player;
        return broadcast_message<true>(ostr.get_span());
    }

    template<bool reliable>
    void broadcast_message(const std::span<std::byte>& message) {
        auto peers = get_peers();        
        for (auto& peer : peers) {
            if (peer.state != ENET_PEER_STATE_CONNECTED) {
                continue;
            }
            send_bytes<reliable>(message, &peer);
        }
    }

    std::span<ENetPeer> get_peers() {
        return {m_host->peers,  m_host->peerCount};
    }

    std::string generate_name(uint64_t id) {
        return s_nicknames[std::hash<uint64_t>{}(id) % s_nicknames.size()];      
    }

    uint32_t generate_id() {
        return m_next_player_id++;
    }

    uint32_t generate_obj_id() {
        return m_next_object_id++;
    }

    void send_ping();

    void update_players();

    vec2 random_point() {
        //TODO: proper random
        float x = float(rand()%int(s_simulation_borders.x));
        float y = float(rand()%int(s_simulation_borders.y));
        return {x, y};
    }

    void random_teleport(GameObject& obj) {
        obj.position = random_point();
    }

    void write_objects(OutByteStream& ostr, uint32_t exclude = uint32_t(-1)) {
        auto size = uint32_t(m_game_objects.size());
        if (exclude != uint32_t(-1)) {
            --size;
        }
        ostr << size;
        for (const auto& object : m_game_objects) {
            if (object.id == exclude) {
                continue;
            }
            ostr << object;
        }
    }

    void process_borders() {
        for (auto& obj : m_game_objects) {
            object_borders(obj);
        }
    }

    void processs_collisions() {
        for (size_t i = 0; i < m_game_objects.size(); ++i) {
            for (size_t j = i + 1; j < m_game_objects.size(); ++j) {
                auto& first = m_game_objects[i];
                auto& second = m_game_objects[j];
                if (glm::distance(first.position, second.position) < first.radius + second.radius) {
                    auto& bigger = first.radius < second.radius ? second : first;
                    auto& smaller = first.radius < second.radius ? first : second;
                    
                    smaller.radius = std::max(smaller.radius / 2.0f, 0.2f);
                    bigger.radius = std::min(bigger.radius + smaller.radius, std::max(s_simulation_borders.x, s_simulation_borders.y) / 8.0f);
                    random_teleport(smaller);
                }
            }
        }
    }

    void process_robots() {
        for (auto& robot : m_robots) {
            auto obj = std::find_if(m_game_objects.begin(), m_game_objects.end(), [&](const auto& o){return o.id == robot.object_id;});
            if (obj == m_game_objects.end()) {
                spdlog::error("robot without object (his obj id is {})", robot.object_id);
                continue;
            }

            if (obj->radius > 4.0f || obj->radius < 0.3f) {
                obj->radius = 0.9f;
                random_teleport(*obj);
            }
            
            auto direction = glm::normalize(robot.current_goal - obj->position);
            obj->velocity = direction * speed_of_object(*obj);

            if (glm::distance(obj->position, robot.current_goal) < obj->radius) {
                robot.current_goal = random_point();
            }
        }
    }

    void spawn_robot() {
        auto obj = create_game_object();
        obj.radius = 0.9f;
        auto robot = Robot();
        robot.current_goal = random_point();
        robot.object_id = obj.id;
        m_game_objects.push_back(obj);
        m_robots.push_back(robot);
    }

    objects_t::iterator get_object_from_player(const Player& player) {
        auto obj_id = m_player_to_object.at(player.id);
        auto object = std::find_if(m_game_objects.begin(), m_game_objects.end(), [&](const auto& obj) {return obj.id == obj_id;});
        if (object == m_game_objects.end()) {
            spdlog::error("Cannot find object for player {}", player.id);
        }
        return object;
    }


    players_t m_players;
    objects_t m_game_objects;
    std::map<uint32_t, uint32_t> m_player_to_object;
    std::vector<Robot> m_robots;
    uint32_t m_next_player_id = 0;
    uint32_t m_next_object_id = 0;
    typename game_clock_t::time_point m_start_time;   
    float m_dt = float(s_update_time.count()) / 1000.0f;
    bool m_connected = false;
    ENetPeer* matchmaking = nullptr;

    inline static const std::array s_nicknames = {
        "Rames Janor", "Nova", "Deckard Cain", "Dark Wanderer",
        "Malfurion", "Illidan", "Tyrande", "Arthas", "Uther",
        "Sylvanas", "Mario", "Waluigi", "Toad", "Bowser", "Dovakin",
        "Kuro", "Johny Gat", "The Boss", "Steve", "Alex", "We need more lumber",
        "Hornet", "Elderbug", "Ugandan Knuckles", "Samus"
    };
};

