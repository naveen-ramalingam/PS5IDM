// ═══════════════════════════════════════════════════════════════════════════════
// PS5 Download Manager - Event Bus Implementation
// ═══════════════════════════════════════════════════════════════════════════════
#include "core/event_bus.h"
#include "core/logger.h"

namespace ps5dm {

static const char* TAG = "EventBus";

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

HandlerId EventBus::subscribe(EventType type, EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    HandlerId id = nextId_++;
    handlers_[type].push_back({id, std::move(handler)});
    return id;
}

HandlerId EventBus::subscribeAll(EventHandler handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    HandlerId id = nextId_++;
    globalHandlers_.push_back({id, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(HandlerId id) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Remove from typed handlers
    for (auto& [type, subs] : handlers_) {
        subs.erase(
            std::remove_if(subs.begin(), subs.end(),
                [id](const Subscription& s) { return s.id == id; }),
            subs.end()
        );
    }

    // Remove from global handlers
    globalHandlers_.erase(
        std::remove_if(globalHandlers_.begin(), globalHandlers_.end(),
            [id](const Subscription& s) { return s.id == id; }),
        globalHandlers_.end()
    );
}

void EventBus::publish(const Event& event) {
    std::vector<EventHandler> toCall;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Collect typed handlers
        auto it = handlers_.find(event.type);
        if (it != handlers_.end()) {
            for (auto& sub : it->second) {
                toCall.push_back(sub.handler);
            }
        }

        // Collect global handlers
        for (auto& sub : globalHandlers_) {
            toCall.push_back(sub.handler);
        }
    }

    // Call outside lock to prevent deadlocks
    for (auto& handler : toCall) {
        try {
            handler(event);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, std::string("Event handler threw exception: ") + e.what());
        }
    }
}

void EventBus::queueEvent(const Event& event) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    eventQueue_.push_back(event);
}

void EventBus::processQueue() {
    std::vector<Event> events;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        events.swap(eventQueue_);
    }

    for (const auto& event : events) {
        publish(event);
    }
}

void EventBus::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.clear();
    globalHandlers_.clear();

    std::lock_guard<std::mutex> qLock(queueMutex_);
    eventQueue_.clear();
}

} // namespace ps5dm
