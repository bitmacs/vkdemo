#include "inputs.h"
#include <cstring>

void init_inputs(Inputs *inputs) {
    memset(inputs, 0, sizeof(Inputs));
}

void shutdown_inputs(Inputs *inputs) {
    memset(inputs, 0, sizeof(Inputs));
}

void begin_inputs_frame(Inputs *inputs) {
    memcpy(inputs->prev_keys, inputs->keys, sizeof(inputs->keys));
    memcpy(inputs->prev_mouse_buttons, inputs->mouse_buttons, sizeof(inputs->mouse_buttons));
    inputs->prev_mouse_pos = inputs->mouse_pos;
    inputs->mouse_scroll_delta = glm::vec2(0.0f);
}

void press_key(Inputs *inputs, int key) { inputs->keys[key] = true; }

void release_key(Inputs *inputs, int key) { inputs->keys[key] = false; }

void press_mouse_button(Inputs *inputs, int button) { inputs->mouse_buttons[button] = true; }

void release_mouse_button(Inputs *inputs, int button) { inputs->mouse_buttons[button] = false; }

void move_mouse(Inputs *inputs, float x, float y) {
    inputs->mouse_pos = glm::vec2(x, y);
}

bool is_key_pressed(Inputs *inputs, int key) { return inputs->keys[key]; }
