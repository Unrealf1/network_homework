#include <algorithm>
#include <vector>
#include <thread>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <enet/enet.h>
#include <spdlog/spdlog.h>
#include <bytestream.hpp>

#include "game_object.hpp"
#include "timed_task_manager.hpp"
#include "common.hpp"


void init_allegro() {
    al_init();
    al_install_keyboard();
    al_init_primitives_addon();
}

std::pair<ENetHost*, ENetPeer*> setup_enet() {
    ENetHost* client = enet_host_create(nullptr, 2, 2, 0, 0);
    if (client == nullptr) {
        spdlog::error("could node create enet client");
        exit(1);
    }
    
    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = s_game_server_port;

    ENetPeer* server = enet_host_connect(client, &address, 2, 0);
    if (server == nullptr) {
        spdlog::error("Cannot connect to the game server");
        exit(2);
    }

    return {client, server};
}

int main() {
    std::vector<GameObject> objects;
    std::vector<Player> players;
    auto [client, server] = setup_enet();

    init_allegro();

    auto timer = al_create_timer(1.0 / 100.0);
    auto queue = al_create_event_queue();
    
    int window_width = 1000;
    int window_height = 600;
    auto disp = al_create_display(window_width, window_height);
    auto font = al_create_builtin_font();

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_start_timer(timer);
    
    ClientInfo my_info;
    GameObject fake;
    GameObject* my_object = &fake;
    bool connected_to_game = false;
    //TODO: get from server?
    float speed = 1.0f;
    float dt = s_client_frame_time.count() / 1000.0f;
    
    auto update_objects = [&](InByteStream& istr) {
        auto my_copy = *my_object;
        uint32_t num_objects;
        istr >> num_objects;
        spdlog::debug("updating objects (received {})", num_objects);
        objects.clear();
        objects.reserve(num_objects);
        for (uint32_t i = 0; i < num_objects; ++i) {
            GameObject obj;
            istr >> obj;
            objects.push_back(std::move(obj));
        }
        objects.push_back(std::move(my_copy));
        my_object = &objects.back();
    };

    auto update_my_object = [&](InByteStream& istr) {
        istr >> *my_object;
    };

    auto model_to_screen_ratio = [&]() {
        float model_min_dim = my_object->radius * 8;

        auto screen_min_dim = std::min(window_width, window_height);
        
        return float(screen_min_dim) / model_min_dim;
    };
    auto model_to_screen_coords = [&](glm::vec2 coords) {
        auto model_center = my_object->position;

        auto screen_center = glm::vec2{window_width, window_height} / 2.0f;
        
        float ratio = model_to_screen_ratio();
        auto res = (coords - model_center) * ratio  + screen_center;
        return res;
    };

    TimedTaskManager<game_clock_t> tasks;

    bool alive = true;
    spdlog::info("starting main loop");
    while(alive) {
        auto frame_start_time = game_clock_t::now();
        auto frame_end_time = frame_start_time + s_client_frame_time;
        const int max_events_in_frame = 100;
        
        // check enet events
        int processed_enet_events = 0;
        ENetEvent net_event;
        while ( enet_host_service(client , &net_event, 0) > 0 
                && processed_enet_events < max_events_in_frame) 
        {
            ++processed_enet_events;
            if (net_event.type == ENET_EVENT_TYPE_RECEIVE) {
                InByteStream istr(net_event.packet->data, net_event.packet->dataLength);
                MessageType type;
                istr >> type;
                spdlog::debug("Client got message of type {}! ({} bytes)", type, net_event.packet->dataLength);
                if (type == MessageType::game_update) {
                    update_objects(istr);
                } else if (type == MessageType::reset) {
                    update_my_object(istr);
                } else if (type == MessageType::register_player) {
                    istr >> my_info;
                    spdlog::info("my id is {}, my object id is {}", my_info.client_id, my_info.controlled_object_id);
                    update_objects(istr);
                    connected_to_game = true;
                    tasks.add_task([&]{
                        // send data to the server
                        OutByteStream message;
                        message << MessageType::game_update;
                        message << my_object->position;
                        send_bytes<false>(message.get_span(), server);
                    }, 10ms);
                } else if (type == MessageType::list_update) {
                    players.emplace_back(istr);
                } else if (type == MessageType::ping) {
                    uint32_t num_players;
                    istr >> num_players;
                    for (uint32_t i = 0; i < num_players; ++i) {
                        uint32_t id;
                        uint32_t ping;
                        istr >> id >> ping;
                        auto player = std::find_if(players.begin(), players.end(), 
                                [&](const Player& player) {return player.id == id;});
                        if (player == players.end()) {
                            spdlog::warn("got unknown player in ping update: id = {}", id);
                            continue;
                        }
                        player->ping = ping;
                    }
                }
            }
        }
        
        // check allegro events
        int processed_allegro_events = 0;
        // TODO: "use al_is_event_queue_empty(queue)" instead?
        ALLEGRO_EVENT al_event;
        while (al_wait_for_event_timed(queue, &al_event, 0.0f) 
                && processed_allegro_events < max_events_in_frame) 
        {
            ++processed_allegro_events;
            if (al_event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                alive = false;
            } else if (al_event.type == ALLEGRO_EVENT_KEY_DOWN) {
                auto key = al_event.keyboard.keycode;
                if (key == ALLEGRO_KEY_ESCAPE) {
                    alive = false;
                }
            }
        }
        if (processed_allegro_events == 0) {
            spdlog::warn("can't keep up! too many allegro events");
        }
        if (connected_to_game) {
            // check user input
            ALLEGRO_KEYBOARD_STATE keyboard_state;
            al_get_keyboard_state(&keyboard_state);
            vec2 direction = {0, 0};
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_DOWN)) {
                direction = {0, 1};
            } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_UP)) {
                direction = {0, -1};
            } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_RIGHT)) {
                direction = {1, 0};
            } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_LEFT)) {
                direction = {-1, 0};
            }
            if (my_object == nullptr) {
                spdlog::error("cannot find object to manipulate");
                alive = false;
                break;
            }
            my_object->position += direction * speed * dt;                
        }
        

        // draw frame
        if (!connected_to_game) {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, "Connecting...");
            al_flip_display();
        } else {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            auto score_message = fmt::format(
                    "Your size: {}; Your coords: {},{}", 
                    my_object->radius, 
                    my_object->position.x, my_object->position.y
            );
            al_draw_text(
                    font, al_map_rgb(255, 255, 255), 
                    0, 100, 0, 
                    score_message.c_str()
            );

            auto ratio = model_to_screen_ratio();
            for (const auto& obj : objects) {
                auto screen_radius = obj.radius * ratio;
                auto coords = model_to_screen_coords(obj.position);
                auto color = colors::create_color(obj.id);
                
                al_draw_circle(coords.x, coords.y, screen_radius, 
                        al_map_rgb(color.r, color.g, color.b), 2
                );

            }
            vec2 box_start = model_to_screen_coords({0, 0});
            vec2 box_end = model_to_screen_coords(s_simulation_borders);
            al_draw_rectangle(box_start.x, box_start.y, box_end.x, box_end.y, al_map_rgb(255, 0, 0), 5);
            al_flip_display();
        }
        tasks.launch();
        std::string players_msg;
        for (const auto& player : players) {
            players_msg += fmt::format("\n{} ({})", player.name, player.ping);
        }
        spdlog::info(players_msg);
        std::this_thread::sleep_until(frame_end_time);
    }

    al_destroy_font(font);
    al_destroy_display(disp);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);
}
