#include <std_include.hpp>
#include "game.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <vector>
#include <tuple>
#include <string>


namespace tsto::game {



    void Game::handle_proto_client_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        headers::set_protobuf_response(ctx);

        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME, "[PROTO-CLIENT-CONFIG] Handling config request from %s", ctx->remote_ip().data());


#ifdef DEBUG

            //logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
            //    "[CLIENT CONFIG] Request from %s - URI: %s",
            //    ctx->remote_ip().data(),
            //    ctx->uri().c_str());

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
                {1010, "MHVersion", "1"},
                {1011, "RequestsPerSecond", "29"},
                {996, "GeolocationCountryCode", "US"},
                {997, "GeolocationCountryName", "United States"},
                {950, "TntAuthUrl", "https://auth.tnt-ea.com"},
                {951, "TntWalletUrl", "https://wallet.tnt-ea.com"},
                {952, "TntNucleusUrl", "https://nucleus.tnt-ea.com"},
                {953, "OriginFriendUrl", "https://m.friends.dm.origin.com"},
                {954, "SynergyUrl", "https://synergy.eamobile.com:443"},
                {1015, "FriendsProxyUrl", "https://friends.simpsons-ea.com"},
                {949, "SynergyFormatUrl", "https://%s.sn.eamobile.com"},
                {995, "KillswitchAllowFriends", "0"},
                {994, "ServerVersion", "local"},
                {1000, "Origin.AppId", "febb96fb9f8eb468"},
                {1001, "ParamUrl.ea.na", "https://kaleidoscope.ea.com?tag=81c4640c68f5eee1"},
                {1002, "ParamUrl.ea.row", "https://kaleidoscope.ea.com?tag=4dada08e18133a83"},
                {1003, "TntUrl", "https://tnt-auth.ea.com?trackingKey=3e92a468de3d2c33"},
                {1011, "MH_Version", "2"}
            };

            // Add all config items
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

            //logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
            //    "[CLIENT CONFIG] Sending %zu items (%zu bytes)",
            //    config_entries.size(),
            //    serialized.size());

#endif /

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, " [PROTO-CLIENT-CONFIG] Error in config: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Game::handle_gameplay_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        headers::set_protobuf_response(ctx);

        try {
            Data::GameplayConfigResponse response;

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME, "[GAMEPLAY-CONFIG] Handling request from %s", ctx->remote_ip().data());


#ifdef DEBUG

            //logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
            //"[PROTO CONFIG] Request from %s - URI: %s",
            //ctx->remote_ip().data(),
            //ctx->uri().c_str());

#endif 

            const std::vector<std::pair<std::string, std::string>> config_entries = {
                {"MysteryBoxUpgrade_GameConfig:Enable:GambleCall", "0"},
                {"System:Disable:DynamicSteamer", "1"},
                {"TouchPriority_GameConfig:Enable:TouchPriority", "true"},
                {"Casino_GameConfig:Gamblers:LocalTapped_CurrencyAmount", "20"},
                {"Casino_GameConfig:Gamblers:LocalTapped_TokenChance", "0.15"},
                {"Validator_GameConfig:VariablesValidator:Validator", "1"},
                {"Validator_GameConfig:VariablesValidator:ScriptedRequirements", "1"},
                {"Validator_GameConfig:VariablesValidator:ScriptedRequirements_Variables", "1"},
                {"WildWest_GameConfig:Dates:Wildwest_Start", "2016-04-19 8:00"},
                {"WildWest_GameConfig:Dates:Wildwest_ActOne", "2016-04-19 8:00"},
                {"WildWest_GameConfig:Dates:Wildwest_ActTwo", "2016-05-03 8:00"},
                {"WildWest_GameConfig:Dates:Wildwest_ActThree", "2016-05-17 8:00"},
                {"WildWest_GameConfig:Dates:Wildwest_End", "2016-05-30 23:59"},
                {"WildWest_GameConfig:Dates:Wildwest_LastChance", "2016-05-24 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_GeneralStoreStartTime", "2016-04-19 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_GeneralStorePrizeTrackOneEndTime", "2016-05-03 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_GeneralStorePrizeTrackTwoEndTime", "2016-05-17 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_GeneralStorePrizeTrackThreeEndTime", "2016-05-30 23:59"},
                {"WildWest_GameConfig:Dates:WildWest_ReturningContentWave1", "2016-04-26 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_ReturningContentWave2", "2016-05-10 8:00"},
                {"WildWest_GameConfig:Dates:WildWest_ReturningContentWave3", "2016-05-24 8:00"},
                {"WildWestPromo_GameConfig:Dates:WildWestPromo_End", "2016-05-04 08:00"},
                {"DSH_GameConfig:Dates:DSH_End", "2016-02-04 7:00:00"},
                {"DSH_GameConfig:Dates:DSH_OutroEnd", "2016-03-04 7:00:00"},
                {"Casino_GameConfig:Dates:Casino_StartDate", "2016-02-23 8:00"},
                {"Casino_GameConfig:Dates:Casino_ActTwo", "2016-03-07 8:00"},
                {"Casino_GameConfig:Dates:Casino_ActThree", "2016-03-17 8:00"},
                {"Casino_GameConfig:Dates:Casino_EndDate", "2016-03-29 8:00"},
                {"Casino_GameConfig:Dates:Casino_ActTwo_Quests", "2016-03-03 8:00"},
                {"Casino_GameConfig:Dates:Casino_ActThree_Quests", "2016-03-13 8:00"},
                {"Casino_GameConfig:Dates:PlatScratcher_WindowOne_Start", "2016-03-01 8:00"},
                {"Casino_GameConfig:Dates:PlatScratcher_WindowOne_End", "2016-03-04 8:00"},
                {"Casino_GameConfig:Dates:PlatScratcher_WindowTwo_Start", "2016-03-24 8:00"},
                {"Casino_GameConfig:Dates:PlatScratcher_WindowTwo_End", "2016-03-27 8:00"},
                {"Casino_GameConfig:Dates:ReferAFriendEnd", "2016-03-23 8:00"},
                {"Margeian_GameConfig:Dates:Margeian_Start", "2016-02-04 7:00:00"},
                {"Margeian_GameConfig:Dates:Margeian_End", "2016-03-16 8:00:00"},
                {"Margeian_GameConfig:Dates:Margeian_Kill", "2016-04-16 8:00:00"},
                {"Tailgate_GameConfig:Dates:Tailgate_Start", "2016-01-01 08:00"},
                {"Tailgate_GameConfig:Dates:Tailgate_End", "2016-02-09 08:00"},
                {"Superheroes_GameConfig:Dates:BeachHouseStartDate", "2015-03-29 8:00"},
                {"Superheroes_GameConfig:Dates:BeachHouseEndDate", "2015-04-07 8:00"},
                {"Superheroes_GameConfig:Dates:MilhouseStartDate", "2015-03-24 8:00"},
                {"Superheroes_GameConfig:Dates:MilhouseEndDate", "2015-03-31 8:00"},
                {"Superheroes_GameConfig:Dates:RadStationStartDate", "2015-03-29 8:00"},
                {"Superheroes_GameConfig:Dates:RadStationEndDate", "2015-04-07 8:00"},
                {"Superheroes_GameConfig:Dates:BartCaveStartDate", "2015-03-29 8:00"},
                {"Superheroes_GameConfig:Dates:BartCaveEndDate", "2015-04-07 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_ActOne_Start", "2014-01-01 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_ActTwo_Start", "2014-04-28 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_ActThree_Start", "2014-05-12 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_End", "2014-05-26 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_KillDate", "2014-06-04 8:00"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_5MinBeforeActTwo", "2014-04-28 7:55"},
                {"Terwilligers_GameConfig:Dates:Terwilligers_5MinBeforeActThree", "2014-05-12 7:55"},
                {"June2015_GameConfig:Dates:June2015_StartDate", "2015-05-23 8:00"},
                {"June2015_GameConfig:Dates:HomerballActOne", "2015-05-23 8:00"},
                {"June2015_GameConfig:Dates:HomerballActTwo", "2015-05-03 8:00"},
                {"June2015_GameConfig:Dates:HomerballActThree", "2015-05-13 8:00"},
                {"June2015_GameConfig:Dates:HomerballEnd", "2015-05-23 16:00"},
                {"June2015_GameConfig:Dates:HomerballActOneCrossover", "2015-05-01 8:00"},
                {"June2015_GameConfig:Dates:HomerballActTwoCrossover", "2015-05-11 8:00"},
                {"June2015_GameConfig:Dates:JuneStorePhaseTwoStart", "2015-05-03 8:00"},
                {"June2015_GameConfig:Dates:JuneStorePhaseThreeStart", "2015-05-13 8:00"},
                {"June2015_GameConfig:Dates:RevertToThreePanel", "2015-06-07 8:00"},
                {"June2015_GameConfig:Dates:SoccerCupStart", "2015-05-25 8:00"},
                {"June2015_GameConfig:Dates:SoccerCupEnd", "2015-05-30 8:00"},
                {"June2015_GameConfig:Dates:JuneTakedownEnd", "2015-07-28 8:00"},
                {"StEaster_GameConfig:Dates:StEaster_PromoEnd", "2016-03-20 8:00:00"},
                {"StEaster_GameConfig:Dates:StEaster_End", "2016-03-30 8:00:00"},
                {"StEaster_GameConfig:Dates:StEaster_Switch", "2016-03-23 8:00:00"},
                {"StEaster_GameConfig:Dates:StEaster_Kill", "2016-04-29 8:00:00"},
                {"StEaster_GameConfig:Dates:CruddySunday", "2016-03-27 8:00:00"},
                {"StEaster_GameConfig:Dates:CruddySunday_End", "2016-03-29 8:00:00"},
                {"StEaster_GameConfig:Dates:StPatsDay", "2016-03-17 8:00:00"},
                {"THOH2015_GameConfig:Dates:THOH2015ActOne", "2015-10-06 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015ActTwo", "2015-10-20 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015ActThree", "2015-11-04 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015ActEnd", "2015-11-17 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015ActKill", "2015-11-27 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015EventEndReminder", "2015-11-14 8:00"},
                {"THOH2015_GameConfig:Dates:THOH2015EventEndReminderEnd", "2015-11-14 23:59"},
                {"THOH2015_GameConfig:Dates:THOH2015LastDayReminder", "2015-11-17 1:00"},
                {"THOH2015_GameConfig:Dates:THOH2015TakedownActive", "2015-11-18 1:00"},
                {"THOH2015_GameConfig:Dates:THOH2015PrizeTrackActOneEnd", "2015-10-20 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_StartDate", "2014-12-08 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_ActOne", "2014-12-08 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_ActTwo", "2014-12-19 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_EndDate", "2015-01-02 8:00"},
                {"XMAS2015_GameConfig:Dates:Maggie_Early_StartDate", "2014-12-23 8:00"},
                {"XMAS2015_GameConfig:Dates:Maggie_StartDate", "2015-01-02 8:00"},
                {"XMAS2015_GameConfig:Dates:Maggie_EndDate", "2015-01-19 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_NewYearsDay", "2015-01-01 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_NewYearsStart", "2014-12-29 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_NewYearsFinale", "2014-12-31 8:00"},
                {"XMAS2015_GameConfig:Dates:XMASFifteen_NewYearsEnd", "2015-01-02 8:00"},
                {"OldJewishMan_GameConfig:Dates:RoshHashanah_StartDate", "2015-09-13 8:00"},
                {"OldJewishMan_GameConfig:Dates:EarlyAccess_OldJewishMan", "2015-09-17 8:00"},
                {"OldJewishMan_GameConfig:Dates:RoshHashanah_EndDate", "2015-09-16 8:00"},
                {"MathletesFeat_GameConfig:Dates:MathletesFeat_Air", "2015-05-18 0:00"},
                {"MathletesFeat_GameConfig:Dates:MathletesFeat_End", "2015-05-19 8:00"},
                {"MathletesFeat_GameConfig:Dates:MathletesFeat_Kill", "2015-06-19 8:00"},
                {"THOH2015_TieIn_GameConfig:Dates:THOH2015_TieIn_StartDate", "2015-10-21 8:00"},
                {"THOH2015_TieIn_GameConfig:Dates:THOH2015_TieIn_AirshipStartDate", "2015-10-23 8:00"},
                {"THOH2015_TieIn_GameConfig:Dates:THOH2015_TieIn_AirDate", "2015-10-25 8:00"},
                {"THOH2015_TieIn_GameConfig:Dates:THOH2015_TieIn_EndDate", "2015-10-26 8:00"},
                {"THOH2015_TieIn_GameConfig:Dates:THOH2015_TieIn_KillDate", "2015-11-26 8:00"},
                {"Valentines2016_GameConfig:Dates:Valentines2016_End", "2015-02-17 8:00:00"},
                {"Valentines2016_GameConfig:Dates:Valentines2016_Day", "2015-02-14 8:00:00"},
                {"Valentines2016_GameConfig:Dates:Valentines2016_Air", "2015-02-15 3:00:00"},
                {"Valentines2016_GameConfig:Dates:Valentines2016_DayEnd", "2015-02-15 8:00:00"},
                {"Valentines2016_GameConfig:Dates:Valentines2016_MaudeAvailability", "2015-02-24 8:00:00"},
                {"HalloweenOfHorror_GameConfig:Dates:THOH2015TieInStart", "2015-10-14 8:00"},
                {"HalloweenOfHorror_GameConfig:Dates:THOH2015TieInEnd", "2015-10-19 8:00"},
                {"HalloweenOfHorror_GameConfig:Dates:THOH2015TieInKill", "2014-11-19 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekOne", "2015-07-21 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekTwo", "2015-07-28 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekThree", "2015-08-04 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekFour", "2015-08-11 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekNine", "2015-09-15 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:SpringfieldHeightsWeekTen", "2015-09-22 8:00"},
                {"SpringfieldHeights_GameConfig:Dates:WealthyWednesday_EndTime", "2014-12-04 8:00"},
                {"WhackingDayPromo_GameConfig:Dates:WhackingDayPromo_Start", "2016-05-09 7:00"},
                {"WhackingDayPromo_GameConfig:Dates:WhackingDayPromo_End", "2016-05-12 7:00"},
                {"WhackingDayPromo_GameConfig:Dates:WhackingDayPromo_Kill", "2016-06-11 7:00"},
                {"WhackingDayPromo_GameConfig:Dates:HomerLivePromo_Start", "2016-05-09 7:00"},
                {"WhackingDayPromo_GameConfig:Dates:HomerLivePromo_End", "2016-05-15 7:00"},
                {"Superheroes2_GameConfig:Dates:SuperheroesTwo_QuickBattle", "2016-06-28 7:00"},
                {"SpringfieldGames_GameConfig:FirmwareMessage:Enabled", "1"},
                {"SciFi_GameConfig:Campaign:Reward1", "60"},
                {"SciFi_GameConfig:Campaign:Reward2", "15"},
                {"SciFi_GameConfig:Enable:Interactions", "1"},
                {"SciFi_SciFighterConfig:SciFighterPayouts:BaseReward_Event", "100"},
                {"SciFi_SciFighterConfig:SciFighterPayouts:CompletionReward_Event", "45"},
                {"SciFi_SciFighterConfig:SciFighterPayouts:PerSecondReward_Event", "5"},
                {"SeasonPremiere2016_GameConfig:Dates:SeasonPremiere2016_4X_End", "2016-10-04 17:00:00"},
                {"SeasonPremiere2016_GameConfig:Dates:SeasonPremiere2016_End", "2016-10-04 17:00:00"},
                {"NewUserBalancingConfig:XPTarget:Lvl60_TargetToNext", "500000"},
                {"NewUserBalancingConfig:XPTarget:PrestigeIncreaseBonusExp", "1000000"},
                {"FirstTimeMTX_GameConfig:FirstTimePacks:Enabled", "true"},
                {"AroundTheWorld_BalancingConfig:AirportJobs:TicketSpawnTimer", "20m"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerCount_Layer1_Base", "1"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerCount_Layer1_Range", "3"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerValue_Layer1_Base", "61"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerValue_Layer1_Range", "33"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerCount_Layer1_Base", "1"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerCount_Layer1_Range", "4"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerValue_Layer1_Base", "14"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerValue_Layer1_Range", "11"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerCount_Layer2_Base", "8"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerCount_Layer2_Range", "8"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:SpecimenLayerValue_Layer2_Base", "163"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerCount_Layer2_Base", "7"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerCount_Layer", "7"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerValue_Layer2_Base", "56"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:TachyonLayerValue_Layer2_Range", "44"},
                {"TimeTravellingToaster_BalancingConfig:ExcavationSiteRewards:PrizeTrackPerShovelValue", "31"},
                {"Music_Spreadsheet_Config:PrizeTrack3:Prize1", "14600"},
                {"Music_Spreadsheet_Config:PrizeTrack3:Prize2", "33600"},
                {"Music_Spreadsheet_Config:PrizeTrack3:Prize4", "83200"},
                {"Music_Spreadsheet_Config:PrizeTrack3:Prize5", "109500"},
                {"Music_Spreadsheet_Config:PrizeTrack3:Prize6", "146000"},
                {"THOH2017_Config:PrizeTrack2:Prize1", "900"},
                {"THOH2017_Config:PrizeTrack2:Prize2", "2800"},
                {"THOH2017_Config:PrizeTrack2:Prize3", "5600"},
                {"THOH2017_Config:PrizeTrack2:Prize4", "9400"},
                {"THOH2017_Config:PrizeTrack2:Prize5", "13200"},
                {"THOH2017_Config:PrizeTrack2:Prize6", "17000"},
                {"THOH2017_Config:PrizeTrack2:Prize7", "22700"},
                {"THOH2017_Config:PrizeTrack3:Prize1", "1300"},
                {"THOH2017_Config:PrizeTrack3:Prize2", "3900"},
                {"THOH2017_Config:PrizeTrack3:Prize3", "7800"},
                {"THOH2017_Config:PrizeTrack3:Prize4", "13000"},
                {"THOH2017_Config:PrizeTrack3:Prize5", "18200"},
                {"THOH2017_Config:PrizeTrack3:Prize6", "23400"},
                {"THOH2017_Config:PrizeTrack3:Prize7", "31200"},
                {"BFCM2017_GameConfig:Dates:BlackFriday2017_End", "2017-11-28 8:00"},
                {"BFCM2017_GameConfig:Dates:CyberMonday2017_End", "2017-11-28 8:00"},
                {"THOH2018_GameConfig:PremiumCosts:Hellscape", "199"},
                {"THOHXXX_GameConfig:EnableMTX:THOHXXX_iOSMTXOffersEnabled", "true"},
                {"THOHXXX_GameConfig:EnableMTX:THOHXXX_AndroidMTXOffersEnabled", "true"},
                {"THOHXXX_GameConfig:Dates:THOHXXX_MERCH_TenCommandments_End", "2019-11-20 14:00"}
            };

            response.mutable_item()->Reserve(config_entries.size());

            // add all config items
            for (const auto& entry : config_entries) {
                auto* item = response.add_item();
                if (!item) {
                    throw std::runtime_error("Failed to add item to protobuf response");
                }
                item->set_name(entry.first);
                item->set_value(entry.second);
            }

            const size_t size = response.ByteSizeLong();
            std::vector<uint8_t> buffer(size);

            if (!response.SerializeToArray(buffer.data(), static_cast<int>(size))) {
                throw std::runtime_error("Failed to serialize protobuf response");
            }

            std::string serialized(reinterpret_cast<char*>(buffer.data()), size);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME, "[GAMEPLAY-CONFIG] Generated response with %zu items (%zu bytes)", config_entries.size(), size);

#ifdef DEBUG

            //logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
            //    "[GAMEPLAY CONFIG] Sending %zu items (%zu bytes)",
            //    config_entries.size(),
            //    size);

#endif 

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[GAMEPLAY-CONFIG] Error in config: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

}