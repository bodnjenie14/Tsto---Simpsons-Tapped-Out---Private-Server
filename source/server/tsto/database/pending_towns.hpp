#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <sqlite3.h>
#include <optional>

namespace tsto {
    namespace database {

        struct PendingTownData {
            std::string id;
            std::string email;
            std::string town_name;
            std::string description;
            std::string file_path;
            size_t file_size;
            std::chrono::system_clock::time_point submitted_at;
            std::string timestamp_str;     
            std::string status;    
            std::string rejection_reason;
        };

        class PendingTowns {
        public:
            static PendingTowns& get_instance();
            ~PendingTowns();

            bool initialize();
            void close();

            std::optional<std::string> add_pending_town(
                const std::string& email,
                const std::string& town_name,
                const std::string& description,
                const std::string& file_path,
                size_t file_size
            );

            std::vector<PendingTownData> get_pending_towns();

            std::optional<PendingTownData> get_pending_town(const std::string& id);

            bool approve_town(const std::string& id, const std::string& target_email = "");

            std::optional<PendingTownData> get_and_approve_town(const std::string& id, const std::string& target_email = "");

            bool reject_town(const std::string& id, const std::string& reason = "");

            std::vector<PendingTownData> get_pending_towns_by_email(const std::string& email);

            bool cleanup_old_towns(int days_to_keep = 30);

        private:
            PendingTowns();
            PendingTowns(const PendingTowns&) = delete;
            PendingTowns& operator=(const PendingTowns&) = delete;

            bool create_tables();
            std::string generate_unique_id();

            sqlite3* db_;
            std::recursive_mutex mutex_;
        };

    }   
}   
