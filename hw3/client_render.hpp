#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <glm/glm.hpp>

#include "client_state.hpp"
#include "common.hpp"



struct Render {
public:
    using vec2 = glm::vec2;

    Render(ClientState& state): state(state) {
        al_init();
        al_install_keyboard();
        al_init_primitives_addon();

        queue = al_create_event_queue();
        timer = al_create_timer(1.0 / 100.0);
        display = al_create_display(window_width, window_height);
        font = al_create_builtin_font();

        al_register_event_source(queue, al_get_keyboard_event_source());
        al_register_event_source(queue, al_get_display_event_source(display));
        al_register_event_source(queue, al_get_timer_event_source(timer));

        al_start_timer(timer);
    }

    ~Render() {
        al_destroy_font(font);
        al_destroy_display(display);
        al_destroy_timer(timer);
        al_destroy_event_queue(queue);
    }

    void process_events() {
        int processed_allegro_events = 0;
        // TODO: "use al_is_event_queue_empty(queue)" instead?
        ALLEGRO_EVENT al_event;
        while (al_wait_for_event_timed(queue, &al_event, 0.0f) 
                && processed_allegro_events < max_events_in_frame) 
        {
            ++processed_allegro_events;
            if (al_event.type == ALLEGRO_EVENT_DISPLAY_CLOSE) {
                state.alive = false;
            } else if (al_event.type == ALLEGRO_EVENT_KEY_DOWN) {
                auto key = al_event.keyboard.keycode;
                if (key == ALLEGRO_KEY_ESCAPE) {
                    state.alive = false;
                }
            }
        }

        if (processed_allegro_events == 0) {
            spdlog::warn("can't keep up! too many allegro events");
        }
    }

    void check_input() {
        if (state.connected) {
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
            if (state.my_object == nullptr) {
                spdlog::error("cannot find object to manipulate");
                state.alive = false;
                throw std::logic_error("my oobjct is nullptr");
            }
            state.my_object->position += direction * state.speed * state.dt;                
        }

    }

    void render() {
        if (!state.connected) {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, "Connecting...");
            al_flip_display();
        } else {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            auto score_message = fmt::format(
                    "Your size: {}; Your coords: {},{}", 
                    state.my_object->radius, 
                    state.my_object->position.x, state.my_object->position.y
            );
            al_draw_text(
                    font, al_map_rgb(255, 255, 255), 
                    0, 100, 0, 
                    score_message.c_str()
            );

            auto ratio = model_to_screen_ratio();
            for (const auto& obj : state.objects) {
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

    }

private:
    float model_to_screen_ratio() const {
        float model_min_dim = state.my_object->radius * 8;
        auto screen_min_dim = std::min(window_width, window_height);
        return float(screen_min_dim) / model_min_dim;
    }

    vec2 model_to_screen_coords(vec2 coords) const {
        auto model_center = state.my_object->position;
        auto screen_center = glm::vec2{window_width, window_height} / 2.0f;
        float ratio = model_to_screen_ratio();
        auto res = (coords - model_center) * ratio  + screen_center;
        return res;
    }

    int window_width = 1000;
    int window_height = 600;
    
    ALLEGRO_TIMER* timer;
    ALLEGRO_EVENT_QUEUE* queue;
    ALLEGRO_DISPLAY* display;
    ALLEGRO_FONT* font;

    ClientState& state;

    const int max_events_in_frame = 100;
};

