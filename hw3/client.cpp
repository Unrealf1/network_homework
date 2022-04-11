#include <algorithm>
#include <vector>

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <enet/enet.h>
#include <spdlog/spdlog.h>

#include "game_object.hpp"
#include "common.hpp"


void init_allegro() {
    al_init();
    al_install_keyboard();
}

void setup_allegro() {

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
    auto [client, server] = setup_enet();

    init_allegro();

    auto timer = al_create_timer(1.0 / 100.0);
    auto queue = al_create_event_queue();
    auto disp = al_create_display(1000, 800);
    auto font = al_create_builtin_font();

    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(disp));
    al_register_event_source(queue, al_get_timer_event_source(timer));
    al_start_timer(timer);

    //TODO: get from server
    uint32_t my_object_id = 0;
    float speed = 1.0f;
    float dt = 1.0f / 100.0f;

    bool alive = true;
    bool redraw = true;
    ALLEGRO_EVENT event;
    spdlog::info("starting main loop");
    while(alive) {
        if (al_wait_for_event_timed(queue, &event, 0.0f)) {
            if (event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                alive = false;
            } else if (event.type == ALLEGRO_EVENT_KEY_DOWN) {
                auto key = event.keyboard.keycode;
                if (key == ALLEGRO_KEY_ESCAPE) {
                    alive = false;
                }
            }
        }

        ALLEGRO_KEYBOARD_STATE keyboard_state;
        vec2 direction = {0, 0};
        if (al_key_down(&keyboard_state, ALLEGRO_KEY_DOWN) {
            direction = {0, -1};
        } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_UP) {
            direction = {0, 1};
        } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_RIGHT) {
            direction = {1, 0};
        } else if (al_key_down(&keyboard_state, ALLEGRO_KEY_LEFT) {
            direction = {-1, 0};
        }
        auto it = std::find_if(objects.begin(), objects.end(), [my_object_id](const auto& obj) { return obj.id == my_object_id; });
        if (it == objects.end()) {
            spdlog::error("cannot find object to manipulate");
            alive = false;
        }
        it->position += direction * speed * dt;                

        //TODO: network

        if(redraw && al_is_event_queue_empty(queue))
        {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, "Hello world!");
            al_flip_display();

            redraw = false;
        }
    }

    al_destroy_font(font);
    al_destroy_display(disp);
    al_destroy_timer(timer);
    al_destroy_event_queue(queue);
}
