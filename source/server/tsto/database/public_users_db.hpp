#pragma once
#include <string>
#include <sqlite3.h>
#include <mutex>
#include <chrono>

namespace tsto::database {

    class PublicUsersDB {
    public:
        static PublicUsersDB& get_instance();
        ~PublicUsersDB();

        bool initialize();
        void close();

        bool register_user(const std::string& email, const std::string& password, const std::string& display_name);
        bool authenticate_user(const std::string& email, const std::string& password);

        bool verify_email(const std::string& email, const std::string& code);
        std::string generate_verification_code(const std::string& email);
        bool reset_password(const std::string& email, const std::string& code, const std::string& new_password);

        bool user_exists(const std::string& email);
        bool change_password(const std::string& email, const std::string& old_password, const std::string& new_password);
        bool get_user_info(const std::string& email, std::string& display_name, bool& is_verified,
            int64_t& registration_date, int64_t& last_login);
        bool update_display_name(const std::string& email, const std::string& display_name);
        bool update_verification_status(const std::string& email, bool is_verified);
        bool update_user_cred(const std::string& email, const std::string& cred);

        bool link_to_tsto_account(const std::string& email, const std::string& tsto_email);
        bool get_linked_tsto_account(const std::string& email, std::string& tsto_email);

        bool store_access_token(const std::string& email, const std::string& token, int64_t expiry);
        bool validate_access_token(const std::string& email, const std::string& token);
        bool invalidate_access_token(const std::string& email, const std::string& token);

        bool get_all_emails(std::vector<std::string>& emails);
        bool search_users(const std::string& field, const std::string& term, std::vector<std::string>& matching_emails);
        bool delete_user(const std::string& email);

    private:
        PublicUsersDB();
        PublicUsersDB(const PublicUsersDB&) = delete;
        PublicUsersDB& operator=(const PublicUsersDB&) = delete;

        bool create_tables();

        std::string generate_salt();
        std::string hash_password(const std::string& password, const std::string& salt);
        std::string generate_random_code();
        bool update_failed_login_attempts(const std::string& email, int attempts, int64_t timestamp);
        bool update_password_hash(const std::string& email, const std::string& hash, const std::string& salt);
        
        static void migrate_password_hash_async(const std::string& email, const std::string& hash, const std::string& salt);

        sqlite3* db_;
        std::mutex mutex_;

        static constexpr const char* DB_FILE = "public_users.db";
        static constexpr int VERIFICATION_CODE_EXPIRY_HOURS = 24;
        static constexpr int ACCOUNT_LOCK_MINUTES = 30;
        static constexpr int MAX_FAILED_ATTEMPTS = 10;
    };

}   
