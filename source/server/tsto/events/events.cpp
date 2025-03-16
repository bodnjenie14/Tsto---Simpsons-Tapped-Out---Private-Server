#include "std_include.hpp"
#include "events.hpp"
#include "debugging/serverlog.hpp"
#include "utilities/configuration.hpp"
#include "utilities/string.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>

namespace tsto {
namespace events {

    std::map<time_t, std::string> tsto_events = {
        {0, "Normal Play"},
        {1348819200, "Episode Tie-in : Season Premiere 2012 Moonshine River"},
        {1349251200, "Event : Treehouse of Horror XXIII"},
        {1352361600, "Event : Thanksgiving 2012"},
        {1352793600, "Episode Tie-in : Penny-Wiseguys"},
        {1354262400, "Episode Tie-in : The Day the Earth Stood Cool"},
        {1354694400, "Event : Christmas 2012"},
        {1359532800, "Event : Valentine's Day 2013"},
        {1362038400, "Episode Tie-in : Gorgeous Grampa"},
        {1362643200, "Event : St. Patrick's Day 2013"},
        {1363248000, "Episode Tie-in : Dark Knight Court"},
        {1365408000, "Episode Tie-in : What Animated Women Want"},
        {1365580800, "Event : Whacking Day 2013"},
        {1367481600, "Episode Tie-in : Whiskey Business"},
        {1368691200, "Season 24 Yard Sale"},
        {1369728000, "Gil Offer : Day Old Donuts"},
        {1371024000, "Expansion : Squidport"},
        {1372406400, "Event : July 4th 2013"},
        {1375257600, "Expansion : Krustyland"},
        {1379923200, "Episode Tie-in : Season Premiere 2013 Homerland"},
        {1380614400, "Event : Treehouse of Horror XXIV"},
        {1384502400, "Event : Thanksgiving 2013"},
        {1386144000, "Episode Tie-in : Yellow Subterfuge"},
        {1386662400, "Event : Christmas 2013"},
        {1389168000, "Episode Tie-in : Married to the Blob"},
        {1390982400, "Event : Super Bowl"},
        {1391587200, "Event : Valentine's Day 2014"},
        {1391673600, "Friend Points"},
        {1394006400, "Episodes Tie-in : Diggs and The Man Who Grew Too Much"},
        {1394611200, "Event : St. Patrick's Day 2014"},
        {1395216000, "Episode Tie-in : The War of Art"},
        {1397030400, "Episode Tie-in : Days of Future Future"},
        {1397548800, "Event : Easter 2014"},
        {1400140800, "Episode Tie-in : Yellow Badge of Cowardge"},
        {1401782400, "Event : Stonecutters"},
        {1403769600, "Gil Offer : Mansion of Solid Gold"},
        {1404288000, "Event : 4th 2014"},
        {1405584000, "Yard Sale 2014"},
        {1407139200, "Gil Offer : Summer Donuts Extravaganza"},
        {1407916800, "Gil Offer : Back to School"},
        {1408435200, "Event : Clash of Clones"},
        {1411545600, "Episode Tie-in : Season Premiere 2014 Clown in the Dumps"},
        {1412236800, "Gil Offer : Shadow Knight"},
        {1412668800, "Event : Treehouse of Horror XXV"},
        {1413446400, "Episode Tie-in : Treehouse of Horror XXV"},
        {1414569600, "Gil Offer : Ghost Pirate Airship"},
        {1415174400, "Episode Tie-in : Matt Groening"},
        {1416470400, "Event : Thanksgiving 2014 and Episode Tie-in : Covercraft"},
        {1417161600, "Gil Offer : Black Friday 2014 and Truckasaurus"},
        {1417593600, "Event : Winter 2014"},
        {1421308800, "Gil Offer : Queen Helvetica"},
        {1421827200, "Episode Tie-in : The Musk Who Fell to Earth"},
        {1423123200, "Offer : Stonecutters Black Market Sale"},
        {1423728000, "Event : Valentine's Day 2015"},
        {1424246400, "Event : Superheroes"},
        {1426060800, "Gil Offer : The Homer"},
        {1426147200, "Event : St. Patrick's Day 2015"},
        {1427875200, "Mystery Box Upgrade and Event : Easter 2015"},
        {1428998400, "Event : Terwilligers"},
        {1429689600, "Episode Tie-in : The Kids Are All Fight"},
        {1431504000, "Episode Tie-in : Mathlete's Feat"},
        {1433318400, "Event : Pride Month 2015"},
        {1434096000, "Gil Offer : End of School"},
        {1435046400, "Event : Tap Ball"},
        {1435651200, "Event : 4th of July 2015"},
        {1437552000, "Expansion : Springfield Heights"},
        {1438243200, "Gil Offer : Ice Cream Man Homer"},
        {1439280000, "Event : Monorail"},
        {1440576000, "Gil Offer : Muscular Marge"},
        {1442995200, "Episode Tie-in : Season Premiere 2015 Every Man's Dream"},
        {1443081600, "IRS and Job Manager"},
        {1443600000, "Gil Deal : Oktoberfest"},
        {1444118400, "Event : Treehouse of Horror 2015"},
        {1444896000, "Episode Tie-in : Halloween of Horror"},
        {1445414400, "Episode Tie-in : Treehouse of Horror XXVI"},
        {1445500800, "New User Power Ups"},
        {1446019200, "Gil Offer : Halloween Promo"},
        {1447920000, "Event : Gobble, Gobble, Toil and Trouble"},
        {1448524800, "Gil Offer : Black Friday 2015"},
        {1449043200, "Expansion : Springfield Heights Chapter 2"},
        {1449561600, "Event : Winter 2015"},
        {1452672000, "Episode Tie-in : Much Apu About Something"},
        {1453363200, "Event : Deep Space Homer"},
        {1454572800, "Gil Offer : Tailgate and Daily Challenges System"},
        {1455091200, "Event : Valentine's Day 2016"},
        {1455696000, "World's Largest Redwood"},
        {1456214400, "Event : Burns' Casino"},
        {1457510400, "Episode Tie-in : The Marge-ian Chronicles"},
        {1458115200, "Event : St. Patrick's Day and Easter 2016"},
        {1459411200, "Event : Crook and Ladder"},
        {1460534400, "Spring Cleaning"},
        {1461052800, "Event : Wild West"},
        {1462780800, "Whacking Day 2016"},
        {1464854400, "Event : Homer's Chiliad"},
        {1465891200, "Event : Superheroes 2"},
        {1467187200, "Event : 4th of July 2016"},
        {1469520000, "Dilapidated Rail Yard"},
        {1470211200, "Event : Springfield Games"},
        {1471334400, "Event : SciFi"},
        {1474444800, "Episode Tie-in : Season Premiere 2016 'Monty Burns' Fleeing Circus"},
        {1475568000, "Event : Treehouse of Horror XXVII"},
        {1478678400, "Episode Tie-in : Havana Wild Weekend"},
        {1479283200, "Event : The Most Dangerous Game"},
        {1481011200, "Event : Winter 2016"},
        {1482393600, "First Time Packs"},
        {1483430400, "Event : Homer the Heretic"},
        {1484121600, "Episode Tie-in : The Great Phatsby"},
        {1485331200, "Lunar New Year 2017"},
        {1485849600, "Event : Destination Springfield"},
        {1486022400, "Football 2017"},
        {1486540800, "Valentine's 2017"},
        {1488960000, "Episode Tie-in : 22 for 30"},
        {1489564800, "Rommelwood Academy"},
        {1489737600, "St. Patrick's 2017"},
        {1490083200, "Hellfish Bonanza"},
        {1490688000, "Event : Secret Agents"},
        {1492329600, "Easter 2017"},
        {1494403200, "Pin Pals"},
        {1495612800, "Forgotten Anniversary"},
        {1496131200, "Event : Time Traveling Toaster, Donut Day 2017 and Road to Riches"},
        {1498636800, "4th of July 2017"},
        {1499241600, "Pride 2017"},
        {1500451200, "Superheroes Return"},
        {1501056000, "Stunt Cannon"},
        {1501574400, "Event : Homerpalooza"},
        {1505203200, "County Fair"},
        {1506499200, "Episode Tie-in : The Serfsons"},
        {1506585600, "Classic Mini Events and Monorail Promo"},
        {1507017600, "Event : Treehouse of Horror XXVIII"},
        {1510646400, "This Thanksgiving's Gone to the Birds!"},
        {1511942400, "A Rigellian Christmas Promo"},
        {1512460800, "Event : The Invasion Before Christmas"},
        {1514966400, "Event : The Buck Stops Here and Episode Tie-In : 'Haw - Haw Land'"},
        {1516176000, "Classic Mini Events and Bart Royale Teaser"},
        {1516694400, "Event : Bart Royale"},
        {1518422400, "Valentine's Day 2018"},
        {1520409600, "Event : Homer vs the 18th Amendment"},
        {1521014400, "The Springfield Jobs Teaser"},
        {1521100800, "Episode Tie-In : 'Homer is Where the Art Isnt"},
        {1521619200, "Event : The Springfield Jobs"},
        {1525852800, "Event : Who Shot Mr. Burns? (Part Three)"},
        {1527062400, "Bart the Fink"},
        {1527667200, "Event : Itchy & Scratchy Land"},
        {1529308800, "Pride 2018"},
        {1530086400, "July 4th 2018"},
        {1531296000, "Event : Poochie's Dog Dayz"},
        {1532505600, "Moe's Ark Teaser"},
        {1533110400, "Event : Moe's Ark"},
        {1536739200, "Event : Super Powers"},
        {1537948800, "Treehouse of Horror XXIX Teaser"},
        {1538553600, "Event : Treehouse of Horror XXIX"},
        {1539763200, "Episode Tie-In : 'Treehouse of Horror XXIX'"},
        {1542182400, "Event : Thanksgiving 2018"},
        {1542787200, "Black Friday 2018"},
        {1543392000, "A Simpsons Christmas Special Teaser"},
        {1543910400, "Event : A Simpsons Christmas Special"},
        {1547020800, "Event : Not Yet Spring Cleaning"},
        {1548230400, "Event : Love, Springfieldian Style"},
        {1551254400, "Event : State of Despair"},
        {1552464000, "Event : A Classless Reunion"},
        {1554883200, "Marge at the Bat"},
        {1557907200, "Event : The Real Moms of Springfield"},
        {1560326400, "Event : Game of Games"},
        {1562140800, "4th of July 2019"},
        {1563350400, "Event : Flanders Family Reunion"},
        {1565164800, "Event : Simpsons Babies"},
        {1568188800, "Event : Krusty's Last Gasp Online"},
        {1569484800, "Event : Cthulhu's Revenge"},
        {1571817600, "Event : Treehouse of Horror XXX"},
        {1574236800, "Event : All American Auction and Black Friday 2019"},
        {1576051200, "Event : Abe's in Toyland"},
        {1579075200, "Event : Holidays of Future Past"},
        {1580889600, "Event : Black History"},
        {1582876800, "Share the Magic"},
        {1584518400, "Event : No Bucks Given"},
        {1586332800, "Event : Simpsons Wrestling"},
        {1589961600, "Event : Pride 2020"},
        {1591776000, "Event : Game of Games The Sequel"},
        {1595577600, "Event : Summer Games 2020"},
        {1597219200, "Event : The Van Houtens"},
        {1600848000, "Event : All This Jazz"},
        {1602662400, "Event : Treehouse of Horror XXXI"},
        {1605686400, "Event : Blargsgiving"},
        {1607414400, "Event : Clash of Creeds: Christmas Royale"},
        {1611129600, "Event : New Year New You"},
        {1612339200, "Event : Love and War"},
        {1615968000, "Event : Springfield Choppers"},
        {1617782400, "Event : Rise of the Robots"},
        {1621411200, "Event : Foodie Fight"},
        {1623225600, "Event : Springfield Enlightened"},
        {1626249600, "Event : Tavern Trouble"},
        {1628064000, "Event : Into the Simpsonsverse"},
        {1631692800, "Event : Breakout Bounty"},
        {1632729600, "Event : Season 33 Prize Track"},
        {1633507200, "Event : Treehouse of Horror XXXII"},
        {1637136000, "Event : Northward Bound"},
        {1638950400, "Event : Holiday Whodunnit"},
        {1642579200, "Event : Red Alert"},
        {1644393600, "Event : Cirque du Springfield"},
        {1648022400, "Event : Hot Diggity D'oh!"},
        {1649836800, "Event : Hell on Wheels"},
        {1653465600, "Event : When the Bough Breaks"},
        {1655280000, "Event : Dog Days"},
        {1658908800, "Event : Splash and Burn"},
        {1660118400, "Event : Showbiz Showdown"},
        {1660636800, "Tenth Anniversary"},
        {1663747200, "Event : Tragic Magic"},
        {1665561600, "Event : Treehouse of Horror XXXIII"},
        {1668585600, "Event : The Atom Smasher"},
        {1670400000, "Event : A Christmas Peril"},
        {1674028800, "Event : Springfield's Got Talent?"},
        {1675843200, "Event : A Warmfyre Welcome"},
        {1679472000, "Event : Shiver Me Barnacles"},
        {1681286400, "Event : Heaven Won't Wait"},
        {1684310400, "Event : For All Rich Mankind"},
        {1686124800, "Event : Fore!"},
        {1689148800, "Event : Food Wars"},
        {1690963200, "Event : Mirror Mayhem"},
        {1694592000, "Event : A Hard Play's Night"},
        {1696406400, "Event : Treehouse of Horror XXXIV"},
        {1700035200, "Event : Cold Turkey"},
        {1701849600, "Event : Snow Place Like the Woods"},
        {1705478400, "Event : Better Late Than Forever"},
        {1707292800, "Event : Fears of a Clown"},
        {1707724800, "Valentine's 2024 Promotion"},
        {1710921600, "Event : The Great Burnsby"},
        {1712736000, "Event: The Simpsanos"},
        {1716364800, "Event : Charity Case"},
        {1717574400, "Event : Summer of Our Discontent"},
        {1717660800, "Donut Day 2024 Promotion"},
        {1719820800, "4th of July 2024 Promotion"},
        {1721203200, "Event : The Mayflower Maple Bowl"},
        {1723017600, "Event : A Bart Future"},
        {1735689600, "Final Event : Taps"} // shud be 1727352000 changed to jan for taps 5
    };

    static time_t current_event_override = 0;
    static time_t event_start_time_override = 0;
    static time_t event_reference_time = 0;

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

    void Events::handle_events_adjust_time(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            std::string uri(ctx->uri().data(), ctx->uri().size());
            std::string body(ctx->body().data(), ctx->body().size());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[EVENTS] Adjust Time Request URI: %s, Body: %s",
                uri.c_str(), body.c_str());

            int minutes_offset = 0;

            if (!body.empty()) {
                rapidjson::Document doc;
                doc.Parse(body.c_str());
                if (!doc.HasParseError() && doc.HasMember("minutes_offset")) {
                    if (doc["minutes_offset"].IsInt()) {
                        minutes_offset = doc["minutes_offset"].GetInt();
                    }
                }
            }

            if (minutes_offset == 0) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Missing or invalid minutes_offset parameter in request. URI: %s, Body: %s",
                    uri.c_str(), body.c_str());
                cb("{\"error\": \"Missing or invalid minutes_offset parameter\"}");
                return;
            }

            time_t seconds_offset = minutes_offset * 60;

            if (current_event_override == 0) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[EVENTS] No event is currently active, but proceeding with time adjustment");

                if (event_start_time_override == 0) {
                    event_start_time_override = std::time(nullptr);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[EVENTS] Initializing event start time to current time: %lld", event_start_time_override);
                }
            }

            event_start_time_override += seconds_offset;

            event_reference_time = std::time(nullptr);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Adjusted event time by %d minutes (%d seconds). New start time: %lld",
                minutes_offset, seconds_offset, event_start_time_override);

            try {
                utils::configuration::WriteInteger64("Events", "CurrentEventOverride", current_event_override);
                utils::configuration::WriteInteger64("Events", "EventStartTimeOverride", event_start_time_override);
                utils::configuration::WriteInteger64("Events", "EventReferenceTime", event_reference_time);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[EVENTS] Saved updated event settings to server configuration");

                auto current_event = Events::get_current_event();
                std::string response;

                if (current_event_override == 0) {
                    response = "{\"success\": true, \"current_event\": \"none\", "
                        "\"adjusted_by_minutes\": " + std::to_string(minutes_offset) +
                        ", \"new_start_time\": " + std::to_string(event_start_time_override) + "}";
                }
                else {
                    response = "{\"success\": true, \"current_event\": \"" + current_event.name +
                        "\", \"adjusted_by_minutes\": " + std::to_string(minutes_offset) +
                        ", \"new_start_time\": " + std::to_string(event_start_time_override) + "}";
                }

                cb(response);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Failed to save adjusted event time: %s", ex.what());
                std::string error = "{\"error\": \"" + std::string(ex.what()) + "\"}";
                cb(error);
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Error adjusting event time: %s", ex.what());
            std::string error = "{\"error\": \"" + std::string(ex.what()) + "\"}";
            cb(error);
        }
    }

    void Events::handle_events_reset_time(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            time_t current_time = std::time(nullptr);
            
            event_start_time_override = current_time;

            event_reference_time = current_time;

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Reset event time to current time: %lld", current_time);

            try {
                utils::configuration::WriteInteger64("Events", "EventStartTimeOverride", event_start_time_override);
                utils::configuration::WriteInteger64("Events", "EventReferenceTime", event_reference_time);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[EVENTS] Saved reset event time to server configuration");

                auto current_event = Events::get_current_event();
                std::string response;

                if (current_event_override == 0) {
                    response = "{\"success\": true, \"current_event\": \"none\", "
                        "\"new_start_time\": " + std::to_string(event_start_time_override) + "}";
                }
                else {
                    response = "{\"success\": true, \"current_event\": \"" + current_event.name +
                        "\", \"new_start_time\": " + std::to_string(event_start_time_override) + "}";
                }

                cb(response);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[EVENTS] Failed to save reset event time: %s", ex.what());
                std::string error = "{\"error\": \"" + std::string(ex.what()) + "\"}";
                cb(error);
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Error resetting event time: %s", ex.what());
            std::string error = "{\"error\": \"" + std::string(ex.what()) + "\"}";
            cb(error);
        }
    }

    void Events::handle_events_get_time(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            time_t current_time = std::time(nullptr);
            
            auto current_event = Events::get_current_event();
            
            time_t event_time = current_event.start_time;
            
            if (event_time > 0) {
                time_t event_reference_time = Events::get_event_reference_time();
                
                if (event_reference_time > 0) {
                    time_t real_time_elapsed = current_time - event_reference_time;
                    
                    event_time += real_time_elapsed;
                    
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[EVENTS] Event reference time: %lld, Real time elapsed: %lld, Adjusted event time: %lld",
                        event_reference_time, real_time_elapsed, event_time);
                }
            } else {
                event_time = current_time;
            }

            rapidjson::Document doc;
            doc.SetObject();
            
            doc.AddMember("status", "success", doc.GetAllocator());
            doc.AddMember("current_time", rapidjson::Value(static_cast<int64_t>(current_time)), doc.GetAllocator());
            doc.AddMember("event_time", rapidjson::Value(static_cast<int64_t>(event_time)), doc.GetAllocator());
            doc.AddMember("event_name", rapidjson::Value(current_event.name.c_str(), doc.GetAllocator()), doc.GetAllocator());
            doc.AddMember("event_is_active", current_event.is_active, doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            cb(buffer.GetString());
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Sent time info: current_time=%lld, event_time=%lld, event=%s",
                current_time, event_time, current_event.name.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Error getting time: %s", ex.what());
                
            rapidjson::Document doc;
            doc.SetObject();
            
            doc.AddMember("status", "error", doc.GetAllocator());
            doc.AddMember("message", rapidjson::Value(ex.what(), doc.GetAllocator()), doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            cb(buffer.GetString());
        }
    }

    void Events::initialize_events() {
        current_event_override = utils::configuration::ReadInteger64("Events", "CurrentEventOverride", 0);
        event_start_time_override = utils::configuration::ReadInteger64("Events", "EventStartTimeOverride", 0);
        event_reference_time = utils::configuration::ReadInteger64("Events", "EventReferenceTime", 0);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[EVENTS] Loaded event configuration - Current Event: %lld, Start Time: %lld, Reference Time: %lld",
            current_event_override, event_start_time_override, event_reference_time);
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

                current_event.start_time = (event_start_time_override != 0) ? event_start_time_override : it->first;

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

    time_t Events::get_event_reference_time() {
        return event_reference_time;
    }

    bool Events::set_current_event(time_t event_time) {
        if (event_time != 0 && tsto_events.find(event_time) == tsto_events.end()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[EVENTS] Attempted to set invalid event time: %lld", event_time);
            return false;
        }

        current_event_override = event_time;

        if (event_time != 0) {
            auto it = tsto_events.find(event_time);
            if (it != tsto_events.end()) {
                event_start_time_override = it->first;
            }
        }
        else {
            event_start_time_override = 0;
        }

        event_reference_time = std::time(nullptr);

        try {
            utils::configuration::WriteInteger64("Events", "CurrentEventOverride", event_time);
            utils::configuration::WriteInteger64("Events", "EventStartTimeOverride", event_start_time_override);
            utils::configuration::WriteInteger64("Events", "EventReferenceTime", event_reference_time);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[EVENTS] Saved event override: %lld with start time: %lld, reference time: %lld",
                event_time, event_start_time_override, event_reference_time);

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

}
