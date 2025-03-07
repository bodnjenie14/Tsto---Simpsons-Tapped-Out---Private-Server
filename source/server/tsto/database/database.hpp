#pragma once
#include <string>
#include <sqlite3.h>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>

namespace tsto {
namespace database {

constexpr const char* DB_FILE = "tsto_users.db";

struct UserData {
    std::string email;
    std::string id;
    std::string access_token;
    std::string device_id;
    std::string user_user_id;
    std::string land_token;
    int64_t mayhem_id;  
    std::string access_code;  
    std::chrono::system_clock::time_point last_active;
};

class Database {
public:
    static Database& get_instance();
    ~Database();

    bool initialize();
    void close();
    int64_t get_next_user_id();
    int64_t get_next_mayhem_id();  
    bool store_user_id(const std::string& email, const std::string& user_id, const std::string& access_token, int64_t mayhem_id = 0, const std::string& access_code = "");
    bool get_user_id(const std::string& email, std::string& user_id);
    bool get_email_by_token(const std::string& access_token, std::string& email);
    bool get_access_token(const std::string& email, std::string& access_token);
    bool get_access_code(const std::string& email, std::string& access_code);
    bool get_email_by_access_code(const std::string& access_code, std::string& email);
    bool get_mayhem_id(const std::string& email, int64_t& mayhem_id);
    bool update_access_token(const std::string& email, const std::string& new_token);
    std::optional<UserData> find_user_by_token(const std::string& access_token);
    bool validate_access_token(const std::string& access_token, std::string& email);

private:
    Database();  
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool create_tables();
    sqlite3* db_{nullptr};
    std::mutex mutex_;
    bool execute_query(const char* query);
    bool prepare_and_bind(sqlite3_stmt** stmt, const char* query);
    static constexpr int SQLITE_PAGE_SIZE = 4096;
    static constexpr int SQLITE_CACHE_PAGES = 2000;
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
    
    bool store_user_id(const std::string& email, const std::string& user_id, const std::string& access_token, int64_t mayhem_id = 0, const std::string& access_code = "");
    bool get_user_id(const std::string& email, std::string& user_id);
    bool get_access_token(const std::string& email, std::string& token);
};

} // namespace database
} // namespace tsto
