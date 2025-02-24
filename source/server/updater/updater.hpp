#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "debugging/serverlog.hpp"

namespace updater
{
    std::string get_server_version();
    bool check_for_updates();
    void download_and_update();

    class UpdateManager
    {
    public:
    };
}
