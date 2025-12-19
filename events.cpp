#include "events.h"

void init_events(Events *events) {
    // TODO
}

void shutdown_events(Events *events) {
    // TODO
}

void register_event_handler(Events *events, EventCode event_code,
                            std::function<bool(const EventData &event_data)> &&handler) {
    events->event_handlers[event_code].push_back(std::move(handler));
}

bool dispatch_event(Events *events, EventCode event_code, const EventData &event_data) {
    if (const auto it = events->event_handlers.find(event_code); it != events->event_handlers.end()) {
        for (const auto &handler: it->second) {
            if (handler(event_data)) {
                return true; // if one handler returns true, the event is handled
            }
        }
    }
    return false;
}
