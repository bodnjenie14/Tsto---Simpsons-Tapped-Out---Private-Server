#pragma once

#include <evpp/http/context.h>
#include <evpp/event_loop.h>
#include <string>
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto/includes/session.hpp"

namespace tsto::login {
    // Define Login class
    class Login {
    public:
        static void handle_progreg_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

        static std::string handle_smtp(const std::string& email, const std::string& display_name);
        static std::string handle_tsto_api(const std::string& email, const std::string& display_name);
        
    private:
        // Helper function to create a new town for a user
        static void create_new_town_for_user(const std::string& filename);
        
        // Helper function to migrate user data from anonymous to registered user
        static bool migrate_anonymous_user(const std::string& anonymous_email, const std::string& registered_email);
    };

}