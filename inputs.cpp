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

void move_mouse(Inputs *inputs, float x, float y) {
    inputs->mouse_pos = glm::vec2(x, y);
}
