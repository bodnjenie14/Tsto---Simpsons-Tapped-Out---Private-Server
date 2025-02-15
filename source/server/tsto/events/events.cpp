#include <std_include.hpp>
#include "events.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include "debugging/serverlog.hpp"

namespace tsto::events {

    std::map<time_t, std::string> tsto_events = {
        {0, "Normal Play"},
        {1348142400, "Start Town"},
        {1348228800, "The first new level : Level 21"},
        {1348833600, "Episode Tie-in : Season Premiere 2012 Moonshine River"},
        {1349265600, "Level 22 and Event : Treehouse of Horror XXIII"},
        {1352116800, "Level 23"},
        {1352376000, "Event : Thanksgiving 2012"},
        {1352808000, "Episode Tie-in : Penny-Wiseguys"},
        {1354276800, "Episode Tie-in : The Day the Earth Stood Cool"},
        {1354708800, "Level 24 and Event : Christmas 2012"},
        {1357905600, "Level 25"},
        {1359547200, "Event : Valentine's Day 2013 and Level 26 Pre-release"},
        {1360843200, "Level 26"},
        {1362052800, "Episode Tie-in : Gorgeous Grampa"},
        {1362657600, "Event : St. Patrick's Day 2013"},
        {1363262400, "Episode Tie-in : Dark Knight Court"},
        {1363867200, "Level 27"},
        {1365422400, "Episode Tie-in : What Animated Women Want"},
        {1365595200, "Event : Whacking Day 2013"},
        {1366372800, "Level 28"},
        {1367496000, "Episode Tie-in : Whiskey Business"},
        {1368187200, "Level 29"},
        {1368705600, "Season 24 Yard Sale"},
        {1369742400, "Gil Offer : Day Old Donuts"},
        {1370001600, "Level 30"},
        {1371038400, "Expansion : Squidport"},
        {1372420800, "Event : July 4th 2013"},
        {1373889600, "Level 31"},
        {1374753600, "Level 32"},
        {1375272000, "Expansion : Krustyland"},
        {1376568000, "Level 33"},
        {1377777600, "Level 34"},
        {1378987200, "Level 35"},
        {1379937600, "Episode Tie-in : Season Premiere 2013 Homerland"},
        {1380628800, "Event : Treehouse of Horror XXIV"},
        {1382529600, "Level 36"},
        {1383825600, "Level 37"},
        {1384516800, "Event : Thanksgiving 2013"},
        {1386158400, "Episode Tie-in : Yellow Subterfuge"},
        {1386676800, "Event : Christmas 2013"},
        {1389182400, "Episode Tie-in : Married to the Blob"},
        {1389873600, "Level 38"},
        {1390996800, "Event : Super Bowl"},
        {1391601600, "Event : Valentine's Day 2014"},
        {1391688000, "Friend Points"},
        {1394020800, "Episodes Tie-in : Diggs and The Man Who Grew Too Much"},
        {1394625600, "Event : St. Patrick's Day 2014"},
        {1395230400, "Episode Tie-in : The War of Art"},
        {1396008000, "Level 39"},
        {1397044800, "Episode Tie-in : Days of Future Future"},
        {1397563200, "Event : Easter 2014"},
        {1398675600, "Event : Terwilligers"},
        {1398859200, "Level 40"},
        {1399885200, "Event : Terwilligers Act 2"},
        {1400155200, "Episode Tie-in : Yellow Badge of Cowardge"},
        {1400760000, "Level 41"},
        {1401796800, "Event : Stonecutters"},
        {1403092800, "Level 42"},
        {1403784000, "Gil Offer : Mansion of Solid Gold"},
        {1404302400, "Event : 4th 2014"},
        {1405598400, "Yard Sale 2014"},
        {1406116800, "Level 43"},
        {1407153600, "Gil Offer : Summer Donuts Extravaganza"},
        {1407931200, "Gil Offer : Back to School"},
        {1408449600, "Event : Clash of Clones"},
        {1409918400, "Level 44"},
        {1410955200, "Level 45"},
        {1411560000, "Episode Tie-in : Season Premiere 2014 Clown in the Dumps"},
        {1412251200, "Gil Offer : Shadow Knight"},
        {1412683200, "Event : Treehouse of Horror 2014"},
        {1413460800, "Episode Tie-in : Halloween of Horror"},
        {1413979200, "Episode Tie-in : Treehouse of Horror XXVI"},
        {1414584000, "Gil Offer : Ghost Pirate Airship"},
        {1415188800, "Episode Tie-in : Matt Groening"},
        {1415793600, "Level 46"},
        {1416484800, "Event : Thanksgiving 2014 and Episode Tie-in : Covercraft"},
        {1417176000, "Gil Offer : Black Friday 2014"},
        {1417608000, "Event : Winter 2014"},
        {1418731200, "Level 47"},
        {1421323200, "Gil Offer : Queen Helvetica"},
        {1421841600, "Episode Tie-in : The Musk Who Fell to Earth"},
        {1422446400, "Level 48"},
        {1423137600, "Offer : Stonecutters Black Market Sale"},
        {1423742400, "Event : Valentine's Day 2015"},
        {1424260800, "Event : Superheroes"},
        {1425470400, "Level 49"},
        {1426075200, "Gil Offer : The Homer"},
        {1426161600, "Event : St. Patrick's Day 2015"},
        {1427889600, "Mystery Box Upgrade and Event : Easter 2015"},
        {1428494400, "Level 50"},
        {1429012800, "Event : Terwilligers"},
        {1429704000, "Episode Tie-in : The Kids Are All Fight"},
        {1430913600, "Level 51 and Money Mountain"},
        {1431518400, "Episode Tie-in : Mathlete's Feat"},
        {1432900800, "Level 52"},
        {1433332800, "Event : Pride Month 2015"},
        {1434110400, "Gil Offer : End of School"},
        {1434542400, "Level 53"},
        {1435060800, "Event : Tap Ball"},
        {1435665600, "Event : 4th of July 2015"},
        {1436961600, "Level 54"},
        {1437566400, "Expansion : Springfield Heights"},
        {1438257600, "Gil Offer : Ice Cream Man Homer"},
        {1438862400, "Level 55"},
        {1439294400, "Event : Monorail"},
        {1440590400, "Gil Offer : Muscular Marge"},
        {1441281600, "Level 56"},
        {1442491200, "Level 57"},
        {1443009600, "Episode Tie-in : Season Premiere 2015 Every Man's Dream"},
        {1443096000, "IRS and Job Manager"},
        {1443614400, "Gil Deal : Oktoberfest"},
        {1444132800, "Event : Treehouse of Horror 2015"},
        {1444910400, "Episode Tie-in : Halloween of Horror"},
        {1445428800, "Episode Tie-in : Treehouse of Horror XXVI"},
        {1445515200, "New User Power Ups"},
        {1446033600, "Gil Offer : Halloween Promo"},
        {1447156800, "Level 58"},
        {1447934400, "Event : Gobble, Gobble, Toil and Trouble"},
        {1448539200, "Gil Offer : Black Friday 2015"},
        {1449057600, "Expansion : Springfield Heights Chapter 2"},
        {1449576000, "Event : Winter 2015"},
        {1451635200, "Event : Tailgate"},
        {1452686400, "Episode Tie-in : Much Apu About Something"},
        {1453377600, "Event : Deep Space Homer"},
        {1454580000, "Event : DSH"},
        {1454580000, "Event : Margeian"},
        {1454587200, "Event : June Update"},
        {1454587200, "Event : Homerball"},
        {1456221600, "Event : Casino"},
        {1457348400, "Event : Casino Act 2"},
        {1458205200, "Event : Casino Act 3"},
        {1461049200, "Event : Wild West"},
        {1462266000, "Event : Wild West Act 2"},
        {1463475600, "Event : Wild West Act 3"},
        {1464868800, "Event : Homer's Chiliad"},
        {1465905600, "Event : Superheroes 2"},
        {1467201600, "Event : 4th of July 2016"},
        {1469534400, "Dilapidated Rail Yard"},
        {1470225600, "Event : Springfield Games"},
        {1471348800, "Event : SciFi"},
        {1474459200, "Episode Tie-in : Season Premiere 2016 \"Monty Burns' Fleeing Circus\""},
        {1475582400, "Event : Treehouse of Horror XXVII"},
        {1478692800, "Episode Tie-in : Havana Wild Weekend"},
        {1479297600, "Event : The Most Dangerous Game"},
        {1481025600, "Event : Winter 2016"},
        {1482408000, "First Time Packs"},
        {1483444800, "Event : Homer the Heretic"},
        {1484136000, "Episode Tie-in : The Great Phatsby"},
        {1485345600, "Lunar New Year 2017"},
        {1485864000, "Event : Destination Springfield"},
        {1486036800, "Football 2017"},
        {1486555200, "Valentine's 2017"},
        {1488974400, "Episode Tie-in : 22 for 30"},
        {1489579200, "Rommelwood Academy"},
        {1489752000, "St. Patrick's 2017"},
        {1490097600, "Hellfish Bonanza"},
        {1490702400, "Event : Secret Agents"},
        {1492344000, "Easter 2017"},
        {1600862400, "Event : All This Jazz" },
        {1602676800, "Event : Treehouse of Horror XXXI"},
        {1605700800, "Event : Blargsgiving" },
        {1607428800, "Event : Clash of Creeds: Christmas Royale"},
        {1611144000, "Event : New Year New You"},
        {1712750400, "Event: The Simpsanos"},
        {1716379200, "Event : Charity Case"},
        {1717588800, "Event : Summer of Our Discontent"},
        {1717675200, "Donut Day 2024 Promotion"},
        {1719835200, "4th of July 2024 Promotion"},
        {1721217600, "Event : The Mayflower Maple Bowl"},
        {1723032000, "Event : A Bart Future"},
        {1735689600, "Final Event : Taps"} // shud be 1727352000 changed to jan for taps 5
    };

    static time_t current_event_override = 0;

    void Events::handle_events_set(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            std::string uri(ctx->uri().data(), ctx->uri().size());
            std::string body(ctx->body().data(), ctx->body().size());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[EVENTS] Request URI: %s, Body: %s",
                uri.c_str(), body.c_str());

            std::string event_time_str;

            if (!body.empty()) {
                rapidjson::Document doc;
                doc.Parse(body.c_str());
                if (!doc.HasParseError() && doc.HasMember("event_time")) {
                    if (doc["event_time"].IsString()) {
                        event_time_str = doc["event_time"].GetString();
                    }
                    else if (doc["event_time"].IsInt64()) {
                        event_time_str = std::to_string(doc["event_time"].GetInt64());
                    }
                }
            }

            if (event_time_str.empty()) {
                size_t pos = uri.find("event_time=");
                if (pos != std::string::npos) {
                    event_time_str = uri.substr(pos + 11);
                    pos = event_time_str.find("&");
                    if (pos != std::string::npos) {
                        event_time_str = event_time_str.substr(0, pos);
                    }
                }
            }

            if (event_time_str.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Missing event_time parameter in request. URI: %s, Body: %s",
                    uri.c_str(), body.c_str());
                cb("{\"error\": \"Missing event_time parameter\"}");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[EVENTS] Parsed event_time: %s", event_time_str.c_str());

            time_t event_time;
            try {
                event_time = std::stoll(event_time_str);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Invalid event time format: %s", event_time_str.c_str());
                cb("{\"error\": \"Invalid event time format\"}");
                return;
            }

            if (Events::set_current_event(event_time)) {
                auto current_event = Events::get_current_event();
                std::string response = "{\"success\": true, \"current_event\": \"" + current_event.name + "\"}";
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[EVENTS] Successfully set event: %s (time=%lld)",
                    current_event.name.c_str(), event_time);
                cb(response);
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Invalid event time: %lld", event_time);
                cb("{\"error\": \"Invalid event time\"}");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Error setting event: %s", ex.what());
            std::string error = "{\"error\": \"" + std::string(ex.what()) + "\"}";
            cb(error);
        }
    }

    void Events::initialize_events() {
        std::filesystem::path config_path = "config/event_override.json";

        if (std::filesystem::exists(config_path)) {
            try {
                std::ifstream file(config_path);
                std::stringstream buffer;
                buffer << file.rdbuf();

                rapidjson::Document doc;
                doc.Parse(buffer.str().c_str());

                if (!doc.HasParseError() && doc.HasMember("event_override")) {
                    current_event_override = doc["event_override"].GetInt64();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[EVENTS] Loaded event override: %lld", current_event_override);
                }
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Failed to load event override: %s", ex.what());
            }
        }
    }

    Event Events::get_current_event() {
        Event current_event;
        current_event.name = "Normal Play";
        current_event.start_time = std::time(nullptr); // Current time
        current_event.end_time = 4070908800;   // January 1, 2099 00:00:00 UTC

        current_event.is_active = true;

        if (current_event_override != 0) {
            auto it = tsto_events.find(current_event_override);
            if (it != tsto_events.end()) {
                current_event.name = it->second;
                current_event.start_time = it->first;
                current_event.end_time = (std::next(it) != tsto_events.end()) ? std::next(it)->first : it->first + (30 * 24 * 60 * 60);
                current_event.is_active = true;
                return current_event;
            }
        }

        return current_event;
    }

    bool Events::is_event_active(const time_t& time) {
        return true;
    }

    time_t Events::get_event_time(const time_t& current_time) {
        Event current_event = Events::get_current_event();
        return current_event.start_time;
    }

    bool Events::set_current_event(time_t event_time) {
        if (event_time != 0 && tsto_events.find(event_time) == tsto_events.end()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Attempted to set invalid event time: %lld", event_time);
            return false;
        }

        current_event_override = event_time;

        try {
            std::filesystem::create_directories("config");
            std::filesystem::path config_path = "config/event_override.json";

            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("event_override", rapidjson::Value(event_time), doc.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            std::ofstream file(config_path);
            file << buffer.GetString();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Saved event override: %lld", event_time);

            auto current_event = Events::get_current_event();
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Lobby time will be set to: %lld (%s)",
                current_event.start_time, current_event.name.c_str());

            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Failed to save event override: %s", ex.what());
            return false;
        }
    }

}
