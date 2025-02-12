#pragma once
#include <map>
#include <string>
#include <chrono>
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>

namespace tsto::events {

    struct Event {
        std::string name;
        time_t start_time;
        time_t end_time;
        bool is_active;
    };

    class Events {
    public:
        static void initialize_events();
        static Event get_current_event();
        static bool is_event_active(const time_t& time);
        static time_t get_event_time(const time_t& current_time);
        static bool set_current_event(time_t event_time);
        static void handle_events_set(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);
    };

    extern std::map<time_t, std::string> tsto_events;

}
