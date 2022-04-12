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
    GameObject* my_object = nullptr;
    bool connected_to_game = false;
    //TODO: get from server
    float speed = 1.0f;
    float dt = 1.0f / 100.0f;
    
    auto update_objects = [&](InByteStream& istr) {
        uint32_t num_objects;
        istr >> num_objects;
        objects.clear();
        objects.reserve(num_objects);
        for (uint32_t i = 0; i < num_objects; ++i) {
            GameObject obj;
            istr >> obj;
            objects.push_back(std::move(obj));
            if (objects.back().id == my_info.controlled_object_id) {
                my_object = &objects.back();
            }    
        }
    };

    auto model_to_screen_ratio = [&]() {
        float model_min_dim = my_object->radius * 2;

        auto screen_min_dim = std::min(window_width, window_height);
        
        return float(screen_min_dim) / model_min_dim;
    };
    auto model_to_screen_coords = [&](glm::vec2 coords) {
        auto model_center = my_object->position;

        auto screen_center = glm::vec2{window_width, window_height} / 2.0f;
        
        float ratio = model_to_screen_ratio();
        return (coords - model_center) * ratio  + screen_center;
    };

    bool alive = true;
    spdlog::info("starting main loop");
    while(alive) {
        auto frame_start_time = game_clock_t::now();
        auto frame_end_time = frame_start_time + s_client_frame_time;
        // check allegro events
        const int max_events_in_frame = 100;
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
            vec2 direction = {0, 0};
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_DOWN)) {
                direction = {0, -1};
            } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_UP)) {
                direction = {0, 1};
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
        
        // check enet events
        int processed_enet_events = 0;
        ENetEvent net_event;
        while ( enet_host_service(client , &net_event, 0) >= 0 
                && processed_enet_events < max_events_in_frame) 
        {
            ++processed_enet_events;
            if (net_event.type == ENET_EVENT_TYPE_RECEIVE) {
                InByteStream istr(net_event.packet->data, net_event.packet->dataLength);
                MessageType type;
                istr >> type;
                spdlog::info("Client got message of type {}! ({} bytes)", type, net_event.packet->dataLength);
                if (type == MessageType::game_update) {
                    update_objects(istr);
                } else if (type == MessageType::register_player) {
                    istr >> my_info;
                    update_objects(istr);
                    connected_to_game = true;
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
                            spdlog::warn("got unknown player in ping update");
                            continue;
                        }
                        player->ping = ping;
                    }
                }
            }
        }

        // draw frame
        if (!connected_to_game) {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, "Connecting...");
            al_flip_display();
        } else {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            auto score_message = fmt::format("Your size: {}", my_object->radius);
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
        }
        
        std::this_thread::sleep_until(frame_end_time);
    }

    al_destroy_font(font);
    al_destroy_display(disp);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);
}
