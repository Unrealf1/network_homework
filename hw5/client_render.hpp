#pragma once

#include <allegro5/allegro5.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_primitives.h>
#include <glm/glm.hpp>
#include <string>

#include "client_state.hpp"
#include "common.hpp"
#include "spdlog/spdlog.h"



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
                if (state.mode == ClientMode::matchmaking) {
                    if (key == ALLEGRO_KEY_DOWN) {
                        state.chosen_lobby = std::min(state.chosen_lobby + 1, state.lobbies.size() - 1);
                    } 
                    if (key == ALLEGRO_KEY_UP) {
                        state.chosen_lobby = std::min(state.chosen_lobby - 1, size_t(0));
                    }
                    if (key == ALLEGRO_KEY_ENTER) {
                        state.should_connect = true;
                    }
                    if (key == ALLEGRO_KEY_SPACE) {
                        state.should_create = true;
                    }
                } else if (state.mode == ClientMode::in_lobby) {
                    if (key == ALLEGRO_KEY_ENTER) {
                        state.send_ready = true;
                    }
                } else if (state.mode == ClientMode::connected) {
                    if (key == ALLEGRO_KEY_Q) {
                        state.nasty = !state.nasty;
                    }
                }
            }

        }

        if (processed_allegro_events == 0) {
            spdlog::warn("can't keep up! too many allegro events");
        }
    }

    void check_input() {
        if (state.mode == ClientMode::connected) {
            // check user input
            ALLEGRO_KEYBOARD_STATE keyboard_state;
            al_get_keyboard_state(&keyboard_state);
            vec2 direction = {0, 0};
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_DOWN)) {
                direction += vec2{0, 1};
            } 
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_UP)) {
                direction += vec2{0, -1};
            }
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_RIGHT)) {
                direction += vec2{1, 0};
            } 
            if (al_key_down(&keyboard_state, ALLEGRO_KEY_LEFT)) {
                direction += vec2{-1, 0};
            }
            direction = glm::length(direction) > 0.5f ? glm::normalize(direction) : vec2{0, 0};
            state.direction = direction;
        }
    }

    void render() {
        if (state.mode == ClientMode::matchmaking) {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, "In matchmaking...");
            for (size_t i = 0; i < state.lobbies.size(); ++i) {
                uint8_t blue = state.chosen_lobby == i ? 255 : 100;
                uint8_t red = 255; //state.lobbies[i].state == LobbyState::playing ? 200 : 255;
                al_draw_text(font, al_map_rgb(red, 200, blue), 0, float(i + 1) * 20.0f, 0, (std::to_string(i + 1) + ".").c_str());
                al_draw_text(font, al_map_rgb(red, 200, blue), 10.0f, float(i + 1) * 20.0f, 0, state.lobbies[i].name.c_str());
                al_draw_text(font, al_map_rgb(red, 200, blue), 150.0f, float(i + 1) * 20.0f, 0, state.lobbies[i].description.c_str());
                al_draw_text(font, al_map_rgb(red, 200, blue), 350.0f, float(i + 1) * 20.0f, 0, fmt::format("{} / {}", state.lobbies[i].players.size(), state.lobbies[i].max_players).c_str());
                if (state.lobbies[i].state == LobbyState::playing) {
                    al_draw_text(font, al_map_rgb(red, 200, blue), 400.0f, float(i + 1) * 20.0f, 0, "[playing]");
                }
            }
            al_flip_display();
        } else if (state.mode == ClientMode::in_lobby) {
            al_clear_to_color(al_map_rgb(0, 0, 0));
            al_draw_text(font, al_map_rgb(255, 255, 255), 0, 0, 0, fmt::format("In lobby \"{}\"", state.lobbies[state.chosen_lobby].name).c_str());
            auto& players = state.lobbies[state.chosen_lobby].players;
            for (size_t i = 0; i < players.size(); ++i) {
                uint8_t blue = 150;
                al_draw_text(font, al_map_rgb(255, 200, blue), 0, float(i + 1) * 20.0f, 0, (std::to_string(i + 1) + ".").c_str());
                al_draw_text(font, al_map_rgb(255, 200, blue), 10.0f, float(i + 1) * 20.0f, 0, players[i].name.c_str());
                al_draw_text(font, al_map_rgb(255, 200, blue), 150.0f, float(i + 1) * 20.0f, 0, players[i].ready ? "READY" : "NOT READY");
            }
            al_flip_display();
        } else {
            if (state.interpolation_length > 0) {
                float ratio = float(state.interpolation_progress) / float(state.interpolation_length);
                local = interpolate(state.from_interpolation, state.my_object, ratio);
                ++state.interpolation_progress;
                if (state.interpolation_progress >= state.interpolation_length) {
                    state.interpolation_length = 0;
                }
            } else {
                local = state.my_object;
            }
            al_clear_to_color(al_map_rgb(0, 0, 0));
            vec2 box_start = model_to_screen_coords({0, 0});
            vec2 box_end = model_to_screen_coords(s_simulation_borders);
            al_draw_rectangle(box_start.x, box_start.y, box_end.x, box_end.y, al_map_rgb(255, 0, 0), 5);
            for (float percent = 10; percent < 99; percent += 10.0f) {
                vec2 vline_start = {box_start.x + (box_end.x - box_start.x) * percent / 100.0f, box_start.y};
                vec2 vline_end = {  box_start.x + (box_end.x - box_start.x) * percent / 100.0f, box_end.y};
                al_draw_line(vline_start.x, vline_start.y, vline_end.x, vline_end.y, al_map_rgb(100, 100, 100), 3);

                vec2 hline_start = {box_start.x, box_start.y + (box_end.y - box_start.y) * percent / 100.0f};
                vec2 hline_end = {  box_end.x,   box_start.y + (box_end.y - box_start.y) * percent / 100.0f};
                al_draw_line(hline_start.x, hline_start.y, hline_end.x, hline_end.y, al_map_rgb(100, 100, 100), 3);
            }

            auto ratio = model_to_screen_ratio();
            for (const auto& obj : state.objects) {
                auto screen_radius = obj.radius * ratio;
                auto coords = model_to_screen_coords(obj.position);
                auto color = colors::create_color(obj.id);
                
                al_draw_circle(coords.x, coords.y, screen_radius, 
                        al_map_rgb(color.r, color.g, color.b), 2
                );

            }
            {
                // estimated client object
                if (!state.objects.empty()) {
                    
                    auto screen_radius = local.radius * ratio;
                    auto coords = model_to_screen_coords(local.position);
                    
                    al_draw_circle(coords.x, coords.y, screen_radius, 
                            al_map_rgb(255, 255, 255), 2
                    );
                }
            }

            auto score_message = fmt::format(
                    "Your size: {}; Your coords: {},{}", 
                    state.my_object.radius, 
                    state.my_object.position.x, state.my_object.position.y
            );
            al_draw_text(
                    font, al_map_rgb(255, 255, 255), 
                    0, 100, 0, 
                    score_message.c_str()
            );
            if (state.nasty) {
                al_draw_text(font, al_map_rgb(255, 255, 0), 0, 150, 0, "not signing messages");
            }
            al_flip_display();
        }

    }

private:
    float model_to_screen_ratio() const {
        float model_min_dim = local.radius * 8;
        auto screen_min_dim = std::min(window_width, window_height);
        return float(screen_min_dim) / model_min_dim;
    }

    vec2 model_to_screen_coords(vec2 coords) const {
        auto model_center = local.position;
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
    GameObject local;

    const int max_events_in_frame = 100;
};

