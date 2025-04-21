#pragma once
#include <string>
#include <sqlite3.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>
#include <vector>
#include <tuple>

namespace tsto {
    namespace database {

        constexpr const char* DB_FILE = "tsto_users.db";

        struct UserData {
            std::string email;
            std::string id;
            std::string access_token;
            std::string lnglv_token;    
            std::string device_id;
            std::string user_user_id;
            std::string land_token;
            std::string mayhem_id;
            std::string access_code;
            std::string user_cred;
            std::string display_name;
            std::string town_name;
            std::string land_save_path;
            std::chrono::system_clock::time_point last_active;
            std::string android_id;
            std::string vendor_id;
            std::string client_ip;
            std::string combined_id;   
            std::string advertising_id;    
            std::string platform_id;    
            std::string manufacturer;     
            std::string model;     
            std::string session_key;      
        };

        class Database {
        public:
            static Database& get_instance();
            ~Database();

            bool initialize();
            void close();
            std::string get_next_user_id();
            std::string get_next_mayhem_id();
            bool get_last_user_id(std::string& user_id);
            bool get_last_mayhem_id(std::string& mayhem_id);
            bool store_user_id(const std::string& email, const std::string& user_id, const std::string& access_token, const std::string& mayhem_id = "", const std::string& access_code = "", const std::string& user_cred = "", const std::string& display_name = "", const std::string& town_name = "", const std::string& land_save_path = "", const std::string& device_id = "", const std::string& android_id = "", const std::string& vendor_id = "", const std::string& client_ip = "", const std::string& combined_id = "", const std::string& advertising_id = "", const std::string& platform_id = "", const std::string& manufacturer = "", const std::string& model = "", const std::string& session_key = "", const std::string& land_token = "", const std::string& anon_uid = "", const std::string& as_identifier = "");
            bool store_user_id(const std::string& old_email, const std::string& new_email, const std::string& user_id, const std::string& mayhem_id = "");
            bool delete_anonymous_user(const std::string& email);
            bool delete_anonymous_users_by_token(const std::string& access_token);
            bool delete_anonymous_users_by_user_id(const std::string& user_id);
            bool get_user_id(const std::string& email, std::string& user_id);
            bool get_email_by_token(const std::string& access_token, std::string& email);
            bool get_email_by_user_id(const std::string& user_id, std::string& email);
            bool get_email_by_mayhem_id(const std::string& mayhem_id, std::string& email);
            bool get_access_token(const std::string& email, std::string& access_token);
            bool get_access_code(const std::string& email, std::string& access_code);
            bool get_email_by_access_code(const std::string& access_code, std::string& email);
            bool get_mayhem_id(const std::string& email, std::string& mayhem_id);
            bool get_user_by_id(const std::string& user_id, std::string& access_token);
            bool update_access_token(const std::string& email, const std::string& new_token);
            bool update_access_token_by_userid(const std::string& user_id, const std::string& new_token);
            bool update_access_code(const std::string& email, const std::string& new_code);
            bool update_mayhem_id(const std::string& email, const std::string& mayhem_id);
            bool validate_access_token(const std::string& token);
            bool validate_access_token(const std::string& access_token, std::string& email);
            bool get_user_cred(const std::string& email, std::string& user_cred);
            bool update_user_cred(const std::string& email, const std::string& new_cred);
            bool get_display_name(const std::string& email, std::string& display_name);
            bool update_display_name(const std::string& email, const std::string& display_name);
            bool clear_access_token(const std::string& email);
            bool logout_user(const std::string& email);
            bool get_all_emails(std::vector<std::string>& emails);
            bool get_town_name(const std::string& email, std::string& town_name);
            bool update_town_name(const std::string& email, const std::string& new_town_name);
            bool get_land_save_path(const std::string& email, std::string& land_save_path);
            bool update_land_save_path(const std::string& email, const std::string& new_land_save_path);
            bool delete_user(const std::string& email);
            bool get_friend_requests(const std::string& user_id, std::vector<std::tuple<std::string, std::string, std::string, int64_t>>& requests);
            bool get_outbound_friend_requests(const std::string& user_id, std::vector<std::tuple<std::string, std::string, std::string, int64_t>>& requests);
            bool send_friend_request(const std::string& from_user_id, const std::string& to_user_id);
            bool accept_friend_request(const std::string& user_id, const std::string& friend_id);
            bool reject_friend_request(const std::string& user_id, const std::string& friend_id);
            bool remove_friend(const std::string& user_id, const std::string& friend_id);
            bool are_friends(const std::string& user_id1, const std::string& user_id2);
            bool get_device_id(const std::string& email, std::string& device_id);
            bool update_device_id(const std::string& email, const std::string& device_id);
            bool get_emails_by_device_id(const std::string& device_id, std::vector<std::string>& emails);
            bool get_user_by_device_id(const std::string& device_id, std::string& email);
            bool get_android_id(const std::string& email, std::string& android_id);
            bool update_android_id(const std::string& email, const std::string& android_id);
            bool get_vendor_id(const std::string& email, std::string& vendor_id);
            bool update_vendor_id(const std::string& email, const std::string& vendor_id);
            bool get_client_ip(const std::string& email, std::string& client_ip);
            bool update_client_ip(const std::string& email, const std::string& client_ip);
            bool get_combined_id(const std::string& email, std::string& combined_id);
            bool update_combined_id(const std::string& email, const std::string& combined_id);
            bool get_advertising_id(const std::string& email, std::string& advertising_id);
            bool update_advertising_id(const std::string& email, const std::string& advertising_id);
            bool get_platform_id(const std::string& email, std::string& platform_id);
            bool update_platform_id(const std::string& email, const std::string& platform_id);
            bool get_manufacturer(const std::string& email, std::string& manufacturer);
            bool update_manufacturer(const std::string& email, const std::string& manufacturer);
            bool get_model(const std::string& email, std::string& model);
            bool update_model(const std::string& email, const std::string& model);
            bool check_android_id_exists(const std::string& android_id, std::string& existing_email);
            bool check_vendor_id_exists(const std::string& vendor_id, std::string& existing_email);
            bool check_advertising_id_exists(const std::string& advertising_id, std::string& existing_email);
            bool check_combined_id_exists(const std::string& combined_id, std::string& existing_email);
            bool check_android_id_ip_exists(const std::string& combined_id, std::string& existing_email);
            bool check_vendor_id_ip_exists(const std::string& combined_id, std::string& existing_email);
            bool validate_token_and_userid(const std::string& token, const std::string& user_id);
            
            bool get_user_by_as_identifier(const std::string& as_identifier, std::string& email);
            bool update_as_identifier(const std::string& email, const std::string& as_identifier);
            bool get_as_identifier(const std::string& email, std::string& as_identifier);

            bool get_device_id_by_android_id(const std::string& android_id, std::string& device_id);
            bool get_device_id_by_advertising_id(const std::string& advertising_id, std::string& device_id);



            static std::string url_decode(const std::string& encoded);

            bool get_lnglv_token(const std::string& email, std::string& lnglv_token);
            bool update_lnglv_token(const std::string& email, const std::string& new_lnglv_token);

            bool get_land_token(const std::string& email, std::string& land_token);
            bool update_land_token(const std::string& email, const std::string& new_land_token);

            bool get_session_key(const std::string& email, std::string& session_key);
            bool update_session_key(const std::string& email, const std::string& new_session_key);

            bool get_anon_uid(const std::string& email, std::string& anon_uid);
            bool update_anon_uid(const std::string& email, const std::string& new_anon_uid);
            bool get_user_by_anon_uid(const std::string& anon_uid, std::string& email);

            bool find_user_email(const std::string& land_id, const std::string& user_id, const std::string& access_token, std::string& email);
            bool get_user_by_ip(const std::string& client_ip, std::string& email);
            bool get_users_by_ip(const std::string& client_ip, std::vector<std::string>& emails);
            bool is_mayhem_id_in_use(const std::string& mayhem_id);
            bool set_display_name(const std::string& email, const std::string& display_name);
            bool user_id_exists(const std::string& user_id);
            //bool sync_user_id(const std::string& email, const std::string& user_id, const std::string& access_token);
            //bool sync_user_id(const std::string& email, bool force_session = false);
            bool get_email_by_town_path(const std::string& town_path, std::string& email);

            bool begin_transaction();
            bool commit_transaction();
            bool rollback_transaction();
            bool is_transaction_active();

            bool create_public_users_table();
            bool register_public_user(const std::string& email, const std::string& password, const std::string& display_name);
            bool authenticate_public_user(const std::string& email, const std::string& password);
            bool verify_public_user_email(const std::string& email, const std::string& code);
            std::string generate_new_verification_code(const std::string& email);
            bool public_user_exists(const std::string& email);
            bool change_public_user_password(const std::string& email, const std::string& old_password, const std::string& new_password);
            bool get_public_user_info(const std::string& email, std::string& display_name, bool& is_verified, int64_t& registration_date, int64_t& last_login);
            bool update_public_user_display_name(const std::string& email, const std::string& display_name);

        private:
            Database();
            Database(const Database&) = delete;
            Database& operator=(const Database&) = delete;

            bool create_tables();
            bool prepare_and_bind(sqlite3_stmt** stmt, const char* query);
            bool execute_query(const char* query);

            std::string generate_salt();
            std::string hash_password(const std::string& password, const std::string& salt);
            std::string generate_verification_code();
            bool update_failed_login_attempts(const std::string& email, int attempts, int64_t timestamp);

            sqlite3* db_;
            std::mutex mutex_;
            bool transaction_active_ = false;
        };

        class UserDatabase {
        private:
            std::mutex mutex_;
            std::unordered_map<std::string, UserData> users_;
            std::unordered_set<std::string> used_user_ids_;

            std::unordered_map<std::string, std::string> email_to_userid_;
            std::unordered_map<std::string, std::string> email_to_token_;

        public:
            UserData* get_or_create_user(const std::string& access_token, const std::string& device_id);
            UserData* get_user(const std::string& access_token);
            std::string generate_unique_user_id();

            bool store_user_id(const std::string& email, const std::string& user_id, const std::string& access_token, const std::string& mayhem_id = "", const std::string& access_code = "", const std::string& user_cred = "", const std::string& town_name = "", const std::string& land_save_path = "", const std::string& device_id = "", const std::string& android_id = "", const std::string& vendor_id = "", const std::string& client_ip = "", const std::string& combined_id = "");
            bool store_user_id(const std::string& old_email, const std::string& new_email, const std::string& user_id, const std::string& mayhem_id = "");
            bool get_user_id(const std::string& email, std::string& user_id);
            bool get_access_token(const std::string& email, std::string& token);
        };

    }   
}   
