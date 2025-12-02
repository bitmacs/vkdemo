#pragma once

#include <glm/glm.hpp>

struct Inputs {
    bool keys[512];
    bool prev_keys[512];
    bool mouse_buttons[32];
    bool prev_mouse_buttons[32];
    glm::vec2 mouse_pos;
    glm::vec2 prev_mouse_pos;
    glm::vec2 mouse_scroll_delta;
};

void init_inputs(Inputs *inputs);
void shutdown_inputs(Inputs *inputs);
void begin_inputs_frame(Inputs *inputs);

void press_key(Inputs *inputs, int key);
void release_key(Inputs *inputs, int key);
void press_mouse_button(Inputs *inputs, int button);
void release_mouse_button(Inputs *inputs, int button);
void move_mouse(Inputs *inputs, float x, float y);
void scroll_mouse(Inputs *inputs, float x, float y);

bool is_key_pressed(Inputs *inputs, int key);
bool is_key_released(Inputs *inputs, int key);
bool was_key_pressed(Inputs *inputs, int key);
bool was_key_released(Inputs *inputs, int key);
bool is_mouse_button_pressed(Inputs *inputs, int button);
bool is_mouse_button_released(Inputs *inputs, int button);
bool was_mouse_button_pressed(Inputs *inputs, int button);
bool was_mouse_button_released(Inputs *inputs, int button);
