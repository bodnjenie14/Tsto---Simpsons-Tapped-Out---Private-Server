#include <std_include.hpp>
#include "game.hpp"
#include "game_config_cache.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <configuration.hpp>
#include <io.hpp>

#include <vector>
#include <tuple>
#include <string>

//todo proto config to json 

namespace tsto::game {

    void Game::handle_gameplay_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        const std::string method = ctx->GetMethod();

        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
            "[GAMEPLAY-CONFIG] Handling request from %s, Method: %s, URI: %s",
            ctx->remote_ip().data(), method.c_str(), ctx->uri().c_str());

        bool is_web_request = false;

        if (method == "POST") {
            is_web_request = true;
        }
        else if (ctx->uri().find("/api/config/game") == 0) {
            is_web_request = true;
        }
        else {
            try {
                const char* accept_header_ptr = ctx->FindRequestHeader("Accept");
                if (accept_header_ptr != nullptr) {
                    std::string accept_header = accept_header_ptr;
                    if (!accept_header.empty() &&
                        (accept_header.find("application/json") != std::string::npos ||
                            accept_header.find("text/html") != std::string::npos)) {
                        is_web_request = true;
                    }
                }
            }
            catch (...) {
            }
        }

        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
            "[GAMEPLAY-CONFIG] Request identified as: %s",
            is_web_request ? "Web Browser" : "Game Client");

        if (method == "GET" && is_web_request) {
            try {
                std::string json_data;
                if (!utils::io::read_file("config.json", &json_data)) {
                    throw std::runtime_error("Failed to read config.json");
                }

                rapidjson::Document doc;
                doc.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(json_data.c_str());

                if (doc.HasParseError()) {
                    rapidjson::ParseErrorCode error = doc.GetParseError();
                    size_t errorOffset = doc.GetErrorOffset();

                    std::string errorContext;
                    size_t start = (errorOffset > 20) ? errorOffset - 20 : 0;
                    size_t length = 40;        
                    if (start + length > json_data.size()) {
                        length = json_data.size() - start;
                    }
                    errorContext = json_data.substr(start, length);

                    std::replace(errorContext.begin(), errorContext.end(), '\n', ' ');

                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[GAMEPLAY-CONFIG] JSON parse error: code %d at offset %zu, context: '...%s...'",
                        error, errorOffset, errorContext.c_str());

                    throw std::runtime_error("Failed to parse config.json: parse error at offset " + std::to_string(errorOffset));
                }

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                std::string formatted_json = buffer.GetString();

                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Sending JSON response to web client (%zu bytes)",
                    formatted_json.size());

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                ctx->AddResponseHeader("Pragma", "no-cache");
                ctx->AddResponseHeader("Expires", "0");
                cb(formatted_json);
                return;
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Error reading config: %s", ex.what());
                ctx->set_response_http_code(500);
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb("{\"error\": \"Internal server error: " + std::string(ex.what()) + "\"}");
                return;
            }
        }

        if (method == "POST") {
            try {
                std::string body = ctx->body().ToString();
                rapidjson::Document doc;
                doc.Parse(body);

                if (doc.HasParseError()) {
                    throw std::runtime_error("Invalid JSON format in request body");
                }

                if (!utils::io::write_file("config.json", body)) {
                    throw std::runtime_error("Failed to write config.json");
                }

                tsto::game::GameConfigCache::invalidate();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Configuration updated, cache invalidated");

                ctx->AddResponseHeader("Content-Type", "application/json");
                cb("{\"status\": \"success\"}");
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Error updating config: %s", ex.what());
                ctx->set_response_http_code(500);
                cb("");
            }
            return;
        }

        try {
            Data::GameplayConfigResponse response;

            if (!tsto::game::GameConfigCache::is_valid()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Cache invalid, loading config from file");

                if (!tsto::game::GameConfigCache::load()) {
                    throw std::runtime_error("Failed to load config.json into cache");
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Using cached configuration");
            }

            const rapidjson::Document& doc = tsto::game::GameConfigCache::get_document();

            if (!doc.IsObject()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] JSON root is not an object");
                throw std::runtime_error("Invalid config.json structure: root is not an object");
            }

            if (!doc.HasMember("GameplayConfig")) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] Missing GameplayConfig object");
                throw std::runtime_error("Invalid config.json structure: missing GameplayConfig object");
            }

            if (!doc["GameplayConfig"].IsObject()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[GAMEPLAY-CONFIG] GameplayConfig is not an object");
                throw std::runtime_error("Invalid config.json structure: GameplayConfig is not an object");
            }

            const auto& config = doc["GameplayConfig"];
            response.mutable_item()->Reserve(config.MemberCount());

            for (auto it = config.MemberBegin(); it != config.MemberEnd(); ++it) {
                if (!it->name.IsString() || !it->value.IsString()) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                        "[GAMEPLAY-CONFIG] Skipping non-string config item");
                    continue;
                }

                auto* item = response.add_item();
                if (!item) {
                    throw std::runtime_error("Failed to add item to protobuf response");
                }
                item->set_name(it->name.GetString());
                item->set_value(it->value.GetString());
            }

            ctx->AddResponseHeader("Content-Type", "application/x-protobuf");

            std::string serialized;
            if (!response.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize protobuf response");
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                "[GAMEPLAY-CONFIG] Generated protobuf response with %zu items (%zu bytes)",
                config.MemberCount(), serialized.size());

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[GAMEPLAY-CONFIG] Error generating protobuf response: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    void Game::handle_proto_client_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        headers::set_protobuf_response(ctx);

        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME, "[PROTO-CLIENT-CONFIG] Handling config request from %s", ctx->remote_ip().data());


#ifdef DEBUG

#endif 

            Data::ClientConfigResponse response;

            const std::vector<std::tuple<int, std::string, std::string>> config_entries = {
                {0, "AppUrl.ios.na", "https://apps.apple.com/app/id497595276?ls=1&mt=8"},
                {1, "GameClient.MaxBundleSize", "50"},
                {2, "MinimumVersion.ios", "4.69.0"},
                {3, "CurrentVersion.ios", "4.69.0"},
                {5, "LocalSaveInterval", "10"},
                {6, "ServerSaveInterval", "100"},
                {7, "AppUrl.ios.row", "https://apps.apple.com/app/id498375892?ls=1&mt=8"},
                {8, "RateUrl.ios.na", "https://apps.apple.com/app/id497595276?ls=1&mt=8"},
                {9, "RateUrl.ios.row", "https://apps.apple.com/app/id498375892?ls=1&mt=8"},
                {10, "CheckDLCInterval", "3600"},
                {13, "AppUrl.android.na", "https://play.google.com/store/apps/details?id=com.ea.game.simpsons4_na"},
                {14, "AppUrl.android.row", "https://play.google.com/store/apps/details?id=com.ea.game.simpsons4_row"},
                {15, "RateUrl.android.na", "https://play.google.com/store/apps/details?id=com.ea.game.simpsons4_na"},
                {16, "RateUrl.android.row", "https://play.google.com/store/apps/details?id=com.ea.game.simpsons4_row"},
                {20, "MinimumVersion.android", "4.69.0"},
                {21, "CurrentVersion.android", "4.69.0"},
                {22, "AppUrl.android.amazon.azn", "amzn://apps/android?p=com.ea.game.simpsonsto_azn"},
                {23, "AppUrl.android.amazon.azn_row", "amzn://apps/android?p=com.ea.game.simpsons4_azn_row"},
                {24, "RateUrl.android.amazon.azn", "amzn://apps/android?p=com.ea.game.simpsonsto_azn"},
                {25, "RateUrl.android.amazon.azn_row", "amzn://apps/android?p=com.ea.game.simpsons4_azn_row"},
                {26, "CoppaEnabledNa", "1"},
                {27, "CoppaEnabledRow", "1"},
                {28, "MaxBuildingSoftCapEnabled", "1"},
                {29, "MaxBuildingSoftCapLimit", "19500"},
                {30, "MaxBuildingSoftCapRepeatAmount", "500"},
                {31, "MaxBuildingHardCapEnabled", "1"},
                {32, "MaxBuildingHardCapLimit", "24000"},
                {41, "ClientConfigInterval", "300"},
                {43, "TelemetryEnabled.android", "0"},
                {44, "TelemetryEnabled.ios", "0"},
                {45, "TelemetryEnabled.android.amazon", "0"},
                {47, "MinimumVersion.android.amazon", "4.69.0"},
                {48, "CurrentVersion.android.amazon", "4.69.0"},
                {52, "OriginAvatarsUrl", "https://m.avatar.dm.origin.com"},   
                {54, "TutorialDLCEnabled.io", "1"},
                {55, "TutorialDLCEnabled.android", "1"},
                {56, "TutorialDLCEnabled.android.amazon", "1"},
                {57, "AppUrl.android.nokia.row", "market://details?id=com.ea.game.simpsons4_nokia_row"},
                {58, "RateUrl.android.nokia.row", "market://details?id=com.ea.game.simpsons4_nokia_row"},
                {59, "ParamUrl.android.nokia.row", "market://details?id=com.ea.game.simpsons4_nokia_row"},
                {60, "TutorialDLCEnabled.android.nokia", "1"},
                {67, "EnableVBOCache", "1"},
                {68, "CoppaEnabledRow.ios.row.4.10.2", "0"},
                {69, "CoppaEnabledRow.android.row.4.10.2", "0"},
                {70, "CoppaEnabledRow.android.amazon.azn_row.4.10.2", "0"},
                {71, "CoppaEnabledRow.ios.row.4.10.3", "0"},
                {72, "CoppaEnabledRow.android.row.4.10.3", "0"},
                {73, "CoppaEnabledRow.android.amazon.azn_row.4.10.3", "0"},
                {74, "ExpiredTokenForcedLogoutEnabled", "1"},
                {75, "SortEnableDAGAndTopo", "2"},
                {76, "CustomFontEnabled.4.21.1", "1"},
                {77, "CustomFontEnabled.4.21.2", "1"},
                {82, "EnableOptAppSwitch.ios", "1"},
                {83, "FullAppSwitchInterval", "86400"},
                {85, "AkamaiClientEnabled.android", "1"},
                {86, "CustomFontEnabled.ios.row.4.13.2", "1"},
                {87, "CustomFontEnabled.ios.na.4.13.2", "1"},
                {88, "CustomFontEnabled.android.row.4.13.2", "1"},
                {89, "CustomFontEnabled.android.na.4.13.2", "1"},
                {90, "CustomFontEnabled.android.amazon.azn.4.13.2", "1"},
                {91, "CustomFontEnabled.android.amazon.azn_row.4.13.2", "1"},
                {92, "EnableOptAppSwitch.android", "1"},
                {93, "EnableOptAppSwitch.android.amazon", "1"},
                {101, "TopUpGrindEnabled", "1"},
                {102, "SynergySendAdditionalPurchaseFields.ios", "1"},
                {103, "SynergySendAdditionalPurchaseFields.android", "1"},
                {104, "SynergySendAdditionalPurchaseFields.android.amazon", "0"},
                {105, "EnableRoadCacheOptimization", "1"},
                {106, "FixNegativeObjectiveCount", "1"},
                {107, "LandDataVersionUpgraderEnabled", "1"},
                {108, "ConfigVersion", "131"},
                {109, "Game:Enable:JobManagerEnabled", "1"},
                {114, "MinimumOSVersion.ios", "7.0.0"},
                {115, "MinimumOSVersion.android", "3.0.0"},
                {116, "MinimumOSVersion.android.amazon", "3.0.0"},
                {117, "XZeroSortFix", "1"},
                {118, "DefaultJobCensus", "1"},
                {119, "TntForgotPassURL", "https://signin.ea.com/p/originX/resetPassword"},
                {120, "Game:Enable:ShowRestorePurchases.ios", "1"},
                {121, "CountriesWhereUpdatesUnavailable", "CN"},
                {122, "Game:Enable:ShowRestorePurchases.ios.4.26.0", "0"},
                {123, "FastQuestReload", "0"},
                {124, "FastQuestReload.4.26.5", "0"},
                {125, "DisableQuestAutostartBackups", "0"},
                {126, "EnableCheckIndex", "1"},
                {127, "CrashReportingIOSOn", "1"},
                {128, "CrashReportingKindleOn", "1"},
                {129, "CrashReportingAndroidOn", "1"},
                {130, "CrashReportingNokiaOn", "1"},
                {132, "UseNumPadCodeLogin", "1"},
                {133, "DeleteUserEnabled", "1"},
                {134, "EnableBGDownloadIos", "0"},
                {135, "EnableBGDownloadAndroid", "0"},
                {136, "EnableBGDownloadAmazon", "0"},
                {137, "MaxSimultaneousBGDownloadsIos", "5"},
                {138, "MaxSimultaneousBGDownloadsAndroid", "1"},
                {139, "MaxSimultaneousBGDownloadsAmazon", "1"},
                {140, "BGDownloadQuickCheckEnabled", "0"},
                {996, "GeolocationCountryCode", "US"},
                {997, "GeolocationCountryName", "United States"},
                {950, "TntAuthUrl", "https://auth.tnt-ea.com"},
                {951, "TntWalletUrl", "https://wallet.tnt-ea.com"},
                {952, "TntNucleusUrl", "https://nucleus.tnt-ea.com"},
                {953, "OriginFriendUrl", "https://m.friends.dm.origin.com"},
                {954, "SynergyUrl", "https://synergy.eamobile.com:443"},
                {949, "SynergyFormatUrl", "https://%s.sn.eamobile.com"},
                {995, "KillswitchAllowFriends", "0"},
                {994, "ServerVersion", "local"},
                {1000, "Origin.AppId", "febb96fb9f8eb468"},
                {1001, "ParamUrl.ea.na", "https://kaleidoscope.ea.com?tag=81c4640c68f5eee1"},
                {1002, "ParamUrl.ea.row", "https://kaleidoscope.ea.com?tag=4dada08e18133a83"},
                {1003, "TntUrl", "https://tnt-auth.ea.com?trackingKey=3e92a468de3d2c33"},
                {1010, "MHVersion", "1" },
                {1011, "RequestsPerSecond", "29" },
                {1011, "MH_Version", "2"},
                {1015, "FriendsProxyUrl", "https://friends.simpsons-ea.com" }
            };

            for (const auto& [id, name, value] : config_entries) {
                auto* item = response.add_items();
                item->set_clientconfigid(id);
                item->set_name(name);
                item->set_value(value);
            }

            std::string serialized;
            if (!response.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize protobuf response");
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME, "[PROTO-CLIENT-CONFIG] Generated response with %d items", response.items_size());


#ifdef DEBUG

#endif 

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, " [PROTO-CLIENT-CONFIG] Error in config: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
}