#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

enum EventCode {
    EVENT_CODE_MOUSE_MOVE,
};

struct EventData {
    union {
        uint32_t u32[4];
        float f32[4];
        double f64[2];
    };
};

struct Events {
    std::unordered_map<EventCode, std::vector<std::function<bool(const EventData &event_data)>>> event_handlers;
};

void register_event_handler(Events *events, EventCode event_code,
                            std::function<bool(const EventData &event_data)> &&handler);

bool dispatch_event(Events *events, EventCode event_code, const EventData &event_data);
