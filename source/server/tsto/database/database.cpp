#include <std_include.hpp>
#include "database.hpp"
#include "debugging/serverlog.hpp"
#include <sqlite3.h>
#include <string>
#include <mutex>
#include <vector>
#include <sstream>     
#include <iomanip>     
#include "cryptography.hpp"
#include <algorithm>       
#include "tsto/includes/session.hpp"

namespace tsto {
    namespace database {

        namespace {
            constexpr int SQLITE_MEMORY_LIMIT = 2 * 1024 * 1024;     
            constexpr int SQLITE_PAGE_SIZE = 4096;                  
            constexpr int SQLITE_CACHE_PAGES = 400;                
        }

        Database& Database::get_instance() {
            static Database instance;
            return instance;
        }

        Database::Database() {
            int rc = sqlite3_initialize();
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to initialize SQLite: %d", rc);
                return;
            }

            sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 1);
            sqlite3_config(SQLITE_CONFIG_HEAP, SQLITE_MEMORY_LIMIT, 64, 8);
            sqlite3_soft_heap_limit(SQLITE_MEMORY_LIMIT);

            if (!initialize()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to initialize database during construction");
            }
        }

        Database::~Database() {
            if (db_) {
                sqlite3_close_v2(db_);
                db_ = nullptr;
            }
            sqlite3_shutdown();
        }

        bool Database::initialize() {
            if (db_) {
                sqlite3_close_v2(db_);
                db_ = nullptr;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Initializing database...");

            int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
            int rc = sqlite3_open_v2(DB_FILE, &db_, flags, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to open database: %s (code: %d)",
                    db_ ? sqlite3_errmsg(db_) : "unknown error", rc);
                if (db_) {
                    sqlite3_close_v2(db_);
                    db_ = nullptr;
                }
                return false;
            }

            std::string page_size = "PRAGMA page_size=" + std::to_string(SQLITE_PAGE_SIZE);
            std::string cache_size = "PRAGMA cache_size=" + std::to_string(SQLITE_CACHE_PAGES);

            const char* pragmas[] = {
                page_size.c_str(),
                cache_size.c_str(),
                "PRAGMA journal_mode=MEMORY",
                "PRAGMA temp_store=MEMORY",
                "PRAGMA synchronous=OFF",
                "PRAGMA locking_mode=EXCLUSIVE"
            };

            for (const auto& pragma : pragmas) {
                char* error_msg = nullptr;
                rc = sqlite3_exec(db_, pragma, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to set pragma: %s - %s (code: %d)",
                        pragma, error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    sqlite3_close_v2(db_);
                    db_ = nullptr;
                    return false;
                }
            }

            bool tables_created = create_tables();
            if (!tables_created) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to create database tables");
                sqlite3_close_v2(db_);
                db_ = nullptr;
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Database initialized successfully");
            return true;
        }

        bool Database::create_tables() {
            const char* create_users_table = R"(
                CREATE TABLE IF NOT EXISTS users (
                    email TEXT PRIMARY KEY,
                    user_id TEXT,
                    access_token TEXT,
                    mayhem_id TEXT,
                    access_code TEXT,
                    user_cred TEXT,
                    display_name TEXT,
                    town_name TEXT,
                    land_save_path TEXT,
                    device_id TEXT,
                    android_id TEXT,
                    vendor_id TEXT,
                    client_ip TEXT,
                    combined_id TEXT,
                    advertising_id TEXT,
                    platform_id TEXT,
                    manufacturer TEXT,
                    model TEXT,
                    session_key TEXT,
                    land_token TEXT,
                    anon_uid TEXT,
                    as_identifier TEXT
                )
            )";

            char* error_msg = nullptr;
            int rc = sqlite3_exec(db_, create_users_table, nullptr, nullptr, &error_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to create users table: %s (code: %d)",
                    error_msg ? error_msg : "unknown error", rc);
                sqlite3_free(error_msg);
                return false;
            }

            const char* check_town_name = "PRAGMA table_info(users);";
            bool has_town_name = false;
            bool has_land_save_path = false;
            bool has_device_id = false;
            bool has_android_id = false;
            bool has_vendor_id = false;
            bool has_client_ip = false;
            bool has_combined_id = false;
            bool has_lnglv_token = false;
            bool has_advertising_id = false;
            bool has_platform_id = false;
            bool has_session_key = false;
            bool has_land_token = false;
            bool has_anon_uid = false;
            bool has_as_identifier = false;

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db_, check_town_name, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement to check columns: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* column_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                if (column_name) {
                    if (strcmp(column_name, "town_name") == 0) {
                        has_town_name = true;
                    }
                    else if (strcmp(column_name, "land_save_path") == 0) {
                        has_land_save_path = true;
                    }
                    else if (strcmp(column_name, "device_id") == 0) {
                        has_device_id = true;
                    }
                    else if (strcmp(column_name, "android_id") == 0) {
                        has_android_id = true;
                    }
                    else if (strcmp(column_name, "vendor_id") == 0) {
                        has_vendor_id = true;
                    }
                    else if (strcmp(column_name, "client_ip") == 0) {
                        has_client_ip = true;
                    }
                    else if (strcmp(column_name, "combined_id") == 0) {
                        has_combined_id = true;
                    }
                    else if (strcmp(column_name, "lnglv_token") == 0) {
                        has_lnglv_token = true;
                    }
                    else if (strcmp(column_name, "advertising_id") == 0) {
                        has_advertising_id = true;
                    }
                    else if (strcmp(column_name, "platform_id") == 0) {
                        has_platform_id = true;
                    }
                    else if (strcmp(column_name, "session_key") == 0) {
                        has_session_key = true;
                    }
                    else if (strcmp(column_name, "land_token") == 0) {
                        has_land_token = true;
                    }
                    else if (strcmp(column_name, "anon_uid") == 0) {
                        has_anon_uid = true;
                    }
                    else if (strcmp(column_name, "as_identifier") == 0) {
                        has_as_identifier = true;
                    }
                }
            }

            sqlite3_finalize(stmt);

            if (!has_town_name) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding town_name column to users table");
                const char* add_town_name = "ALTER TABLE users ADD COLUMN town_name TEXT;";
                rc = sqlite3_exec(db_, add_town_name, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add town_name column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_land_save_path) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding land_save_path column to users table");
                const char* add_land_save_path = "ALTER TABLE users ADD COLUMN land_save_path TEXT;";
                rc = sqlite3_exec(db_, add_land_save_path, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add land_save_path column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_device_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding device_id column to users table");
                const char* add_device_id = "ALTER TABLE users ADD COLUMN device_id TEXT;";
                rc = sqlite3_exec(db_, add_device_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add device_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_android_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding android_id column to users table");
                const char* add_android_id = "ALTER TABLE users ADD COLUMN android_id TEXT;";
                rc = sqlite3_exec(db_, add_android_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add android_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_vendor_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding vendor_id column to users table");
                const char* add_vendor_id = "ALTER TABLE users ADD COLUMN vendor_id TEXT;";
                rc = sqlite3_exec(db_, add_vendor_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add vendor_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_client_ip) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding client_ip column to users table");
                const char* add_client_ip = "ALTER TABLE users ADD COLUMN client_ip TEXT;";
                rc = sqlite3_exec(db_, add_client_ip, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add client_ip column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_combined_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding combined_id column to users table");
                const char* add_combined_id = "ALTER TABLE users ADD COLUMN combined_id TEXT;";
                rc = sqlite3_exec(db_, add_combined_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add combined_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_advertising_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding advertising_id column to users table");
                const char* add_advertising_id = "ALTER TABLE users ADD COLUMN advertising_id TEXT;";
                rc = sqlite3_exec(db_, add_advertising_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add advertising_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_platform_id) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding platform_id column to users table");
                const char* add_platform_id = "ALTER TABLE users ADD COLUMN platform_id TEXT;";
                rc = sqlite3_exec(db_, add_platform_id, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add platform_id column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_session_key) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding session_key column to users table");
                const char* add_session_key = "ALTER TABLE users ADD COLUMN session_key TEXT;";
                rc = sqlite3_exec(db_, add_session_key, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add session_key column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            if (!has_land_token) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding land_token column to users table");
                const char* add_land_token = "ALTER TABLE users ADD COLUMN land_token TEXT;";
                rc = sqlite3_exec(db_, add_land_token, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add land_token column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }
            
            if (!has_anon_uid) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding anon_uid column to users table");
                const char* add_anon_uid = "ALTER TABLE users ADD COLUMN anon_uid TEXT;";
                rc = sqlite3_exec(db_, add_anon_uid, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add anon_uid column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }
            
            if (!has_as_identifier) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding as_identifier column to users table");
                const char* add_as_identifier = "ALTER TABLE users ADD COLUMN as_identifier TEXT;";
                rc = sqlite3_exec(db_, add_as_identifier, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add as_identifier column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
                
                const char* create_as_identifier_index = "CREATE INDEX IF NOT EXISTS idx_as_identifier ON users(as_identifier);";
                rc = sqlite3_exec(db_, create_as_identifier_index, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to create index on as_identifier column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Successfully created index on as_identifier column");
                }
            }

            if (!has_lnglv_token) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding lnglv_token column to users table");
                const char* add_lnglv_token = "ALTER TABLE users ADD COLUMN lnglv_token TEXT;";
                rc = sqlite3_exec(db_, add_lnglv_token, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add lnglv_token column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }
            
            if (!has_as_identifier) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Adding as_identifier column to users table");
                const char* add_as_identifier = "ALTER TABLE users ADD COLUMN as_identifier TEXT;";
                rc = sqlite3_exec(db_, add_as_identifier, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to add as_identifier column: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            const char* create_friend_requests_table =
                "CREATE TABLE IF NOT EXISTS friend_requests ("
                "from_user_id TEXT NOT NULL,"       
                "to_user_id TEXT NOT NULL,"        
                "status TEXT NOT NULL,"              
                "timestamp INTEGER NOT NULL"      
                ");";

            rc = sqlite3_exec(db_, create_friend_requests_table, nullptr, nullptr, &error_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to create friend requests table: %s (code: %d)",
                    error_msg ? error_msg : "unknown error", rc);
                sqlite3_free(error_msg);
                return false;
            }

            const char* create_user_id_index = "CREATE INDEX IF NOT EXISTS idx_users_user_id ON users(user_id);";
            const char* create_token_index = "CREATE INDEX IF NOT EXISTS idx_users_access_token ON users(access_token);";
            const char* create_mayhem_id_index = "CREATE UNIQUE INDEX IF NOT EXISTS idx_users_mayhem_id ON users(mayhem_id) WHERE mayhem_id IS NOT NULL AND mayhem_id != '';";    
            const char* create_code_index = "CREATE INDEX IF NOT EXISTS idx_users_access_code ON users(access_code);";
            const char* create_device_id_index = "CREATE INDEX IF NOT EXISTS idx_users_device_id ON users(device_id);";
            const char* create_android_id_index = "CREATE INDEX IF NOT EXISTS idx_users_android_id ON users(android_id);";
            const char* create_vendor_id_index = "CREATE INDEX IF NOT EXISTS idx_users_vendor_id ON users(vendor_id);";
            const char* create_combined_id_index = "CREATE INDEX IF NOT EXISTS idx_users_combined_id ON users(combined_id);";
            const char* create_advertising_id_index = "CREATE INDEX IF NOT EXISTS idx_users_advertising_id ON users(advertising_id);";
            const char* create_platform_id_index = "CREATE INDEX IF NOT EXISTS idx_users_platform_id ON users(platform_id);";
            const char* create_anon_uid_index = "CREATE INDEX IF NOT EXISTS idx_users_anon_uid ON users(anon_uid);";
            const char* create_as_identifier_index = "CREATE INDEX IF NOT EXISTS idx_users_as_identifier ON users(as_identifier);";

            const char* indexes[] = {
                create_user_id_index,
                create_token_index,
                create_mayhem_id_index,
                create_code_index,
                create_device_id_index,
                create_android_id_index,
                create_vendor_id_index,
                create_combined_id_index,
                create_advertising_id_index,
                create_platform_id_index,
                create_anon_uid_index,
                create_as_identifier_index
            };

            for (const auto& create_index : indexes) {
                rc = sqlite3_exec(db_, create_index, nullptr, nullptr, &error_msg);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to create index: %s (code: %d)",
                        error_msg ? error_msg : "unknown error", rc);
                    sqlite3_free(error_msg);
                    return false;
                }
            }

            return true;
        }

        std::string Database::get_next_user_id() {
            std::string user_id_str = "0000000000001";

            std::string last_user_id;
            if (get_last_user_id(last_user_id) && !last_user_id.empty()) {
                user_id_str = last_user_id;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Found last user ID in database: %s", user_id_str.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] No existing user IDs found, starting with default");
            }

            if (user_id_str.length() < 13) {
                user_id_str = std::string(13 - user_id_str.length(), '0') + user_id_str;
            }
            else if (user_id_str.length() > 13) {
                user_id_str = user_id_str.substr(user_id_str.length() - 13);
            }
            
            bool is_unique = false;
            int max_attempts = 100;          
            int attempts = 0;
            
            while (!is_unique && attempts < max_attempts) {
                int carry = 1;
                for (int i = user_id_str.length() - 1; i >= 0; --i) {
                    int digit = (user_id_str[i] - '0') + carry;
                    carry = digit / 10;
                    digit = digit % 10;
                    user_id_str[i] = '0' + digit;

                    if (carry == 0) break;
                }

                if (carry > 0) {
                    user_id_str = "1" + user_id_str.substr(1);
                }

                if (user_id_str.length() != 13) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] User ID length error! Expected 13 but got %zu digits. Fixing...",
                        user_id_str.length());

                    if (user_id_str.length() < 13) {
                        user_id_str = std::string(13 - user_id_str.length(), '0') + user_id_str;
                    }
                    else if (user_id_str.length() > 13) {
                        user_id_str = user_id_str.substr(user_id_str.length() - 13);
                    }
                }
                
                is_unique = !user_id_exists(user_id_str);
                
                if (!is_unique) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] User ID %s is already in use, generating next one", 
                        user_id_str.c_str());
                    attempts++;
                }
            }
            
            if (!is_unique) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to find unique user ID after %d attempts, generating random ID", 
                    max_attempts);
                    
                std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                std::string random_part = timestamp.substr(timestamp.length() - std::min(timestamp.length(), size_t(13)));
                
                if (random_part.length() < 13) {
                    random_part = std::string(13 - random_part.length(), '0') + random_part;
                } else if (random_part.length() > 13) {
                    random_part = random_part.substr(random_part.length() - 13);
                }
                
                user_id_str = random_part;
                
                if (user_id_exists(user_id_str)) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, 9);
                    
                    for (size_t i = 5; i < 13; i++) {
                        user_id_str[i] = '0' + dis(gen);
                    }
                    
                    if (user_id_exists(user_id_str)) {
                        for (size_t i = 0; i < 13; i++) {
                            user_id_str[i] = '0' + dis(gen);
                        }
                    }
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Generated next user ID: %s (length: %zu)", user_id_str.c_str(), user_id_str.length());

            return user_id_str;
        }

        std::string Database::get_next_mayhem_id() {
            std::string mayhem_id_str = "00000000000000000000000000000000000001";

            std::string last_mayhem_id;
            if (get_last_mayhem_id(last_mayhem_id) && !last_mayhem_id.empty()) {
                mayhem_id_str = last_mayhem_id;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Found last mayhem ID in database: %s", mayhem_id_str.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] No existing mayhem IDs found, starting with default");
            }

            if (mayhem_id_str.length() < 38) {
                mayhem_id_str = std::string(38 - mayhem_id_str.length(), '0') + mayhem_id_str;
            }
            else if (mayhem_id_str.length() > 38) {
                mayhem_id_str = mayhem_id_str.substr(mayhem_id_str.length() - 38);
            }

            bool is_unique = false;
            int max_attempts = 100;          
            int attempts = 0;
            
            while (!is_unique && attempts < max_attempts) {
                int carry = 1;
                for (int i = mayhem_id_str.length() - 1; i >= 0; --i) {
                    int digit = (mayhem_id_str[i] - '0') + carry;
                    carry = digit / 10;
                    digit = digit % 10;
                    mayhem_id_str[i] = '0' + digit;

                    if (carry == 0) break;
                }

                if (carry > 0) {
                    mayhem_id_str = "1" + mayhem_id_str.substr(1, 37);
                }

                if (mayhem_id_str.length() != 38) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] Mayhem ID length error! Expected 38 but got %zu digits. Fixing...",
                        mayhem_id_str.length());

                    if (mayhem_id_str.length() < 38) {
                        mayhem_id_str = std::string(38 - mayhem_id_str.length(), '0') + mayhem_id_str;
                    }
                    else if (mayhem_id_str.length() > 38) {
                        mayhem_id_str = mayhem_id_str.substr(mayhem_id_str.length() - 38);
                    }
                }
                
                is_unique = !is_mayhem_id_in_use(mayhem_id_str);
                
                if (!is_unique) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] Mayhem ID %s is already in use, generating next one", 
                        mayhem_id_str.c_str());
                    attempts++;
                }
            }
            
            if (!is_unique) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to find unique mayhem ID after %d attempts, generating random ID", 
                    max_attempts);
                    
                std::string timestamp = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                std::string random_part = timestamp.substr(0, std::min(timestamp.length(), size_t(20)));
                mayhem_id_str = random_part + std::string(38 - random_part.length(), '0');
                
                if (is_mayhem_id_in_use(mayhem_id_str)) {
                    std::random_device rd;
                    std::mt19937 gen(rd());
                    std::uniform_int_distribution<> dis(0, 9);
                    
                    for (size_t i = random_part.length(); i < 38; i++) {
                        mayhem_id_str[i] = '0' + dis(gen);
                    }
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Generated next mayhem ID: %s (length: %zu)", mayhem_id_str.c_str(), mayhem_id_str.length());

            return mayhem_id_str;
        }

        bool Database::store_user_id(const std::string& email, const std::string& user_id,
            const std::string& access_token, const std::string& mayhem_id,
            const std::string& access_code, const std::string& user_cred,
            const std::string& display_name, const std::string& town_name, const std::string& land_save_path, const std::string& device_id, const std::string& android_id, const std::string& vendor_id, const std::string& client_ip, const std::string& combined_id, const std::string& advertising_id, const std::string& platform_id, const std::string& manufacturer, const std::string& model, const std::string& session_key, const std::string& land_token, const std::string& anon_uid, const std::string& as_identifier) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            std::string formatted_mayhem_id = mayhem_id;
            if (mayhem_id.empty()) {
                formatted_mayhem_id = get_next_mayhem_id();
            }

            if (!formatted_mayhem_id.empty() && formatted_mayhem_id.length() != 38 && formatted_mayhem_id.find_first_not_of("0123456789") == std::string::npos) {
                formatted_mayhem_id = std::string(38 - formatted_mayhem_id.length(), '0') + formatted_mayhem_id;

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Padded numeric mayhem ID %s to 38-digit format: %s",
                    mayhem_id.c_str(), formatted_mayhem_id.c_str());
            }

            std::string formatted_user_id = user_id;
            if (!user_id.empty() && user_id.length() != 13 && user_id.find_first_not_of("0123456789") == std::string::npos) {
                if (user_id.length() < 13) {
                    formatted_user_id = std::string(13 - user_id.length(), '0') + user_id;
                }
                else {
                    formatted_user_id = user_id.substr(user_id.length() - 13);
                }

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Formatted numeric user ID %s to 13-digit format: %s",
                    user_id.c_str(), formatted_user_id.c_str());
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Storing user data - Email: %s, User ID: %s, Access Token: %s, Mayhem ID: %s, Access Code: %s, Has Credential: %s, Display Name: %s, Town Name: %s, Land Save Path: %s, Device ID: %s, Android ID: %s, Vendor ID: %s, Client IP: %s, Combined ID: %s, Advertising ID: %s, Platform ID: %s, Manufacturer: %s, Model: %s, Session Key: %s, Land Token: %s, Anon UID: %s, AS Identifier: %s",
                email.c_str(), formatted_user_id.c_str(), access_token.c_str(), formatted_mayhem_id.c_str(), access_code.c_str(),
                user_cred.empty() ? "No" : "Yes", display_name.c_str(), town_name.c_str(), land_save_path.c_str(), device_id.c_str(), android_id.c_str(), vendor_id.c_str(), client_ip.c_str(), combined_id.c_str(), advertising_id.c_str(), platform_id.c_str(), manufacturer.c_str(), model.c_str(), session_key.c_str(), land_token.c_str(), anon_uid.c_str(), as_identifier.c_str());

            const char* query = "INSERT OR REPLACE INTO users (email, user_id, access_token, mayhem_id, access_code, user_cred, display_name, town_name, land_save_path, device_id, android_id, vendor_id, client_ip, combined_id, advertising_id, platform_id, manufacturer, model, session_key, land_token, anon_uid, as_identifier) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, formatted_user_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, access_token.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, formatted_mayhem_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, access_code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 6, user_cred.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 7, display_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 8, town_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 9, land_save_path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 10, device_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 11, android_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 12, vendor_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 13, client_ip.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 14, combined_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 15, advertising_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 16, platform_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 17, manufacturer.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 18, model.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 19, session_key.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 20, land_token.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 21, anon_uid.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 22, as_identifier.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to store user data: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::store_user_id(const std::string& old_email, const std::string& new_email, const std::string& user_id, const std::string& mayhem_id) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot store user data - database not initialized");
                return false;
            }

            if (old_email.empty() || new_email.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot store user data - email is empty");
                return false;
            }

            std::string access_token;
            if (!get_access_token(old_email, access_token)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to get access token for old email: %s", old_email.c_str());
                return false;
            }

            if (access_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Access token for old email is empty: %s", old_email.c_str());
                return false;
            }

            std::string access_code;
            get_access_code(old_email, access_code);            

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Converting user from %s to %s with user_id: %s, mayhem_id: %s",
                old_email.c_str(), new_email.c_str(), user_id.c_str(), mayhem_id.c_str());

            bool success = store_user_id(new_email, user_id, access_token, mayhem_id, access_code, "", "", "", "", "", "", "", "", "", "", "");
            if (!success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to store user data for new email: %s", new_email.c_str());
                return false;
            }

            const char* sql = "DELETE FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for deleting old user: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, old_email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool delete_success = (rc == SQLITE_DONE);

            if (!delete_success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to delete old user data: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Successfully deleted old user data for email: %s", old_email.c_str());
            }

            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::get_access_code(const std::string& email, std::string& access_code) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get access code - database not initialized");
                return false;
            }

            const char* sql = "SELECT access_code FROM users WHERE email = ?;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = false;

            if (rc == SQLITE_ROW) {
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                    access_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    success = true;
                }
            }

            sqlite3_finalize(stmt);

            if (!success) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No access code found for email: %s", email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Found access code for email: %s: %s", email.c_str(), access_code.c_str());
            }

            return success;
        }

        bool Database::get_email_by_access_code(const std::string& access_code, std::string& email) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get email by access code - database not initialized");
                return false;
            }

            if (access_code.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Empty access code provided, rejecting");
                return false;
            }

            const char* sql = "SELECT email FROM users WHERE access_code = ?;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, access_code.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = false;

            if (rc == SQLITE_ROW) {
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                    email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    success = true;
                }
            }

            sqlite3_finalize(stmt);

            if (!success) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No email found for access code: %s", access_code.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Found email for access code: %s: %s", access_code.c_str(), email.c_str());
            }

            return success;
        }

        bool Database::get_user_id(const std::string& email, std::string& user_id) {
            if (!db_) return false;

            const char* sql = "SELECT user_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            bool found = false;
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) {
                    user_id = reinterpret_cast<const char*>(text);
                    found = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found user ID for email %s: %s", email.c_str(), user_id.c_str());
                }
            }
            else if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to query user ID: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }

            sqlite3_finalize(stmt);
            return found;
        }

        bool Database::get_access_token(const std::string& email, std::string& access_token) {
            if (!db_) return false;

            const char* sql = "SELECT access_token FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            bool found = false;
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) {
                    access_token = reinterpret_cast<const char*>(text);
                    found = true;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Found access token for email %s", email.c_str());
                }
            }
            else if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to query access token: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }

            sqlite3_finalize(stmt);
            return found;
        }

        bool Database::update_access_token(const std::string& email, const std::string& new_token) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update access token - database not initialized");
                return false;
            }

            if (email.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update access token - email is empty");
                return false;
            }

            std::lock_guard<std::mutex> lock(mutex_);

            const char* sql = "UPDATE users SET access_token = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for updating access token: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (!success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update access token: %s", sqlite3_errmsg(db_));
            }
            else {
                if (sqlite3_changes(db_) > 0) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Successfully updated access token for email: %s", email.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                        "No rows affected when updating access token for email: %s", email.c_str());
                    success = false;
                }
            }

            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::update_access_code(const std::string& email, const std::string& new_code) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update access code - database not initialized");
                return false;
            }

            const char* sql = "UPDATE users SET access_code = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for updating access code: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (!success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update access code: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Successfully updated access code for email: %s", email.c_str());
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_mayhem_id(const std::string& email, const std::string& mayhem_id) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update mayhem ID - database not initialized");
                return false;
            }

            const char* sql = "UPDATE users SET mayhem_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for updating mayhem ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, mayhem_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = (rc == SQLITE_DONE);

            if (!success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update mayhem ID: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Successfully updated mayhem ID for email %s: %s", email.c_str(), mayhem_id.c_str());
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::get_email_by_token(const std::string& token, std::string& email) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get email by token - database not initialized");
                return false;
            }

            std::string token_prefix = token.length() > 10 ? token.substr(0, 10) + "..." : token;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Looking up email for token: %s", token_prefix.c_str());

            const char* sql = "SELECT email FROM users WHERE access_token = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool success = false;

            if (rc == SQLITE_ROW) {
                if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                    email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    success = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found email for token: %s", email.c_str());
                }
            }

            sqlite3_finalize(stmt);

            if (!success) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No email found for token: %s", token_prefix.c_str());
            }

            return success;
        }

        bool Database::validate_access_token(const std::string& token) {
            std::string email;
            return validate_access_token(token, email);
        }

        bool Database::validate_access_token(const std::string& access_token, std::string& email) {
            if (access_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot validate empty access token");
                return false;
            }

            std::string token_prefix = access_token.length() > 10 ? access_token.substr(0, 10) + "..." : access_token;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Validating access token: %s", token_prefix.c_str());

            const int max_attempts = 2;
            for (int attempt = 1; attempt <= max_attempts; attempt++) {
                std::lock_guard<std::mutex> lock(mutex_);

                if (attempt > 1) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Retry attempt %d for token validation", attempt);
                }

                std::string decoded_token;
                std::string token_user_id;
                std::string printable_user_id;
                bool extracted_user_id = false;

                try {
                    std::string base64_token = access_token;
                    std::replace(base64_token.begin(), base64_token.end(), '-', '+');
                    std::replace(base64_token.begin(), base64_token.end(), '_', '/');

                    switch (base64_token.length() % 4) {
                    case 0: break;    
                    case 2: base64_token += "=="; break;
                    case 3: base64_token += "="; break;
                    default:    
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                            "Invalid base64 token length: %zu", base64_token.length());
                    }

                    decoded_token = utils::cryptography::base64::decode(base64_token);
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Decoded token: %s", decoded_token.substr(0, 20).c_str());

                    size_t mpdon_pos = decoded_token.find(":MPDON");
                    if (mpdon_pos != std::string::npos) {
                        size_t last_colon = decoded_token.rfind(':', mpdon_pos - 1);
                        if (last_colon != std::string::npos && last_colon < mpdon_pos) {
                            token_user_id = decoded_token.substr(last_colon + 1, mpdon_pos - last_colon - 1);
                            extracted_user_id = true;

                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Extracted user ID from new format token: %s", token_user_id.c_str());
                        }
                    }

                    if (!extracted_user_id) {
                        if (decoded_token.find("AT0:2.0:3.0:86400:") == 0) {
                            std::vector<std::string> token_parts;
                            std::string token_copy = decoded_token;
                            size_t pos = 0;
                            std::string delimiter = ":";
                            std::string part;

                            while ((pos = token_copy.find(delimiter)) != std::string::npos) {
                                part = token_copy.substr(0, pos);
                                token_parts.push_back(part);
                                token_copy.erase(0, pos + delimiter.length());
                            }

                            if (!token_copy.empty()) {
                                token_parts.push_back(token_copy);
                            }

                            if (token_parts.size() >= 6) {
                                token_user_id = token_parts[5];
                                extracted_user_id = true;

                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                    "Extracted user ID from old format token parts: %s", token_user_id.c_str());
                            }
                        }
                        else {
                            std::vector<std::string> token_parts;
                            std::string token_copy = access_token;
                            size_t pos = 0;
                            std::string delimiter = ":";
                            std::string part;

                            while ((pos = token_copy.find(delimiter)) != std::string::npos) {
                                part = token_copy.substr(0, pos);
                                token_parts.push_back(part);
                                token_copy.erase(0, pos + delimiter.length());
                            }

                            if (!token_copy.empty()) {
                                token_parts.push_back(token_copy);
                            }

                            if (token_parts.size() >= 6) {
                                token_user_id = token_parts[5];
                                extracted_user_id = true;

                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                    "Extracted user ID from token parts: %s", token_user_id.c_str());
                            }
                        }
                    }

                    printable_user_id.clear();
                    for (char c : token_user_id) {
                        if (c >= 32 && c <= 126) {    
                            printable_user_id += c;
                        }
                        else {
                            char hex[5];
                            snprintf(hex, sizeof(hex), "\\x%02X", static_cast<unsigned char>(c));
                            printable_user_id += hex;
                        }
                    }

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Extracted user ID from token (printable form): %s", printable_user_id.c_str());
                }
                catch (const std::exception& e) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Failed to decode token: %s", e.what());
                }

                sqlite3_stmt* stmt = nullptr;
                const char* query = "SELECT email, user_id FROM users WHERE access_token = ? COLLATE NOCASE";

                if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to prepare statement for token validation (attempt %d): %s",
                        attempt, sqlite3_errmsg(db_));

                    if (attempt < max_attempts) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    return false;
                }

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Looking for exact token match: %s", access_token.c_str());

                sqlite3_bind_text(stmt, 1, access_token.c_str(), -1, SQLITE_STATIC);

                bool found_match = false;
                std::string best_email;
                std::string best_user_id;

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    std::string row_email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    std::string row_user_id = "";

                    if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) {
                        row_user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                    }

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found token match - Email: %s, User ID: %s",
                        row_email.c_str(), row_user_id.c_str());

                    if (!row_user_id.empty()) {
                        if (extracted_user_id) {
                            if (token_user_id.find_first_not_of("0123456789") == std::string::npos) {
                                std::string padded_token_id = std::string(38 - token_user_id.length(), '0') + token_user_id;
                                if (padded_token_id == row_user_id) {
                                    best_email = row_email;
                                    best_user_id = row_user_id;
                                    found_match = true;
                                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                        "Found perfect match with padded token ID");
                                    break;
                                }
                            }

                            if (row_user_id.find(token_user_id) != std::string::npos) {
                                best_email = row_email;
                                best_user_id = row_user_id;
                                found_match = true;
                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                    "Found match where DB ID contains token ID");
                                break;
                            }
                        }

                        if (!found_match) {
                            best_email = row_email;
                            best_user_id = row_user_id;
                            found_match = true;
                        }
                    }
                    else if (!found_match) {
                        best_email = row_email;
                        best_user_id = row_user_id;
                        found_match = true;
                    }
                }

                if (found_match) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Found best token match - Email: %s, User ID: %s",
                        best_email.c_str(), best_user_id.c_str());
                }
                else if (extracted_user_id && !token_user_id.empty()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "No exact token match found, trying to find user by ID: %s", token_user_id.c_str());

                    std::string padded_user_id = token_user_id;
                    if (token_user_id.find_first_not_of("0123456789") == std::string::npos && token_user_id.length() != 38) {
                        padded_user_id = std::string(38 - token_user_id.length(), '0') + token_user_id;
                    }

                    sqlite3_finalize(stmt);     

                    const char* id_query = "SELECT email, user_id, access_token FROM users WHERE user_id = ? COLLATE NOCASE";
                    if (sqlite3_prepare_v2(db_, id_query, -1, &stmt, nullptr) == SQLITE_OK) {
                        sqlite3_bind_text(stmt, 1, padded_user_id.c_str(), -1, SQLITE_STATIC);

                        if (sqlite3_step(stmt) == SQLITE_ROW) {
                            best_email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                            best_user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                            std::string db_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));

                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                "Found user by ID - Email: %s, User ID: %s, Token: %s",
                                best_email.c_str(), best_user_id.c_str(), db_token.substr(0, 10).c_str());

                            const char* update_query = "UPDATE users SET access_token = ? WHERE user_id = ? COLLATE NOCASE";
                            sqlite3_stmt* update_stmt = nullptr;

                            if (sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr) == SQLITE_OK) {
                                sqlite3_bind_text(update_stmt, 1, access_token.c_str(), -1, SQLITE_STATIC);
                                sqlite3_bind_text(update_stmt, 2, padded_user_id.c_str(), -1, SQLITE_STATIC);

                                if (sqlite3_step(update_stmt) == SQLITE_DONE) {
                                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                        "Updated token for user ID: %s", padded_user_id.c_str());
                                    found_match = true;
                                }

                                sqlite3_finalize(update_stmt);
                            }
                        }
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "No matching token found in database and could not extract user ID");
                }

                int result = found_match ? SQLITE_ROW : SQLITE_DONE;

                if (result == SQLITE_ROW) {
                    email = best_email;
                    std::string db_user_id = best_user_id;

                    if (extracted_user_id) {
                        std::string token_numeric_id;
                        std::string db_numeric_id;

                        db_numeric_id = db_user_id;         

                        for (char c : token_user_id) {
                            if (c >= '0' && c <= '9') {
                                token_numeric_id += c;
                            }
                        }

                        if (token_numeric_id.empty() || token_numeric_id.length() < 5) {
                            token_numeric_id.clear();

                            std::string possible_id;
                            for (char c : token_user_id) {
                                if ((c >= '0' && c <= '9') || c == ':') {
                                    possible_id += c;
                                }
                            }

                            if (!possible_id.empty()) {
                                token_numeric_id = possible_id;
                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                    "Extracted possible ID from binary data: %s", token_numeric_id.c_str());
                            }
                        }

                        if (token_numeric_id.empty() || token_numeric_id.length() < 5) {
                            token_numeric_id.clear();

                            for (size_t i = 0; i < token_user_id.size(); ++i) {
                                unsigned char byte = static_cast<unsigned char>(token_user_id[i]);
                                char hex[3];
                                snprintf(hex, sizeof(hex), "%02X", byte);
                                token_numeric_id += hex;
                            }

                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Using hex representation of token user ID: %s", token_numeric_id.c_str());
                        }

                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                            "Comparing IDs - Token: %s, DB: %s", token_numeric_id.c_str(), db_numeric_id.c_str());

                        bool ids_match = false;

                        if (token_numeric_id.find_first_not_of("0123456789") == std::string::npos) {
                            std::string padded_token_id = std::string(38 - token_numeric_id.length(), '0') + token_numeric_id;

                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Padded token ID for comparison: %s", padded_token_id.c_str());

                            if (padded_token_id == db_numeric_id) {
                                ids_match = true;
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                    "Padded token ID matches DB ID");
                            }
                        }

                        if (!ids_match) {
                            if (token_numeric_id.length() >= 10 && db_numeric_id.find(token_numeric_id) != std::string::npos) {
                                ids_match = true;
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                    "DB ID contains token ID");
                            }
                            else if (db_numeric_id.length() >= 10 && token_numeric_id.find(db_numeric_id) != std::string::npos) {
                                ids_match = true;
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                    "Token ID contains DB ID");
                            }
                            else if (token_numeric_id == db_numeric_id) {
                                ids_match = true;
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                    "Token ID equals DB ID");
                            }
                            else if (token_numeric_id.length() >= 10 && db_numeric_id.length() >= 10) {
                                size_t compare_length = std::min<size_t>(10, std::min<size_t>(token_numeric_id.length(), db_numeric_id.length()));
                                std::string token_suffix = token_numeric_id.substr(token_numeric_id.length() - compare_length);
                                std::string db_suffix = db_numeric_id.substr(db_numeric_id.length() - compare_length);

                                if (token_suffix == db_suffix) {
                                    ids_match = true;
                                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                        "Last %zu digits match between token and DB IDs", compare_length);
                                }
                            }
                        }

                        if (ids_match) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                                "User ID match found between token and database");
                        }
                        else {
                            sqlite3_finalize(stmt);

                            if (attempt < max_attempts) {
                                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                break;            
                            }
                            return false;
                        }
                    }

                    sqlite3_finalize(stmt);

                    if (attempt > 1) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                            "Token validation succeeded on retry attempt %d", attempt);
                    }

                    return true;
                }

                sqlite3_finalize(stmt);

                const char* prefix_query = "SELECT email, user_id FROM users WHERE access_token IS NOT NULL AND access_token != '' COLLATE NOCASE";

                if (sqlite3_prepare_v2(db_, prefix_query, -1, &stmt, nullptr) != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to prepare statement for token prefix validation (attempt %d): %s",
                        attempt, sqlite3_errmsg(db_));

                    if (attempt < max_attempts) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                        continue;
                    }
                    return false;
                }

                std::string matched_email;
                std::string matched_user_id;

                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    const char* db_email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    const char* db_user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

                    if (db_email && db_user_id) {
                        std::string stored_token(db_email);

                        if (stored_token.length() >= access_token.length() &&
                            _strnicmp(stored_token.c_str(), access_token.c_str(), access_token.length()) == 0) {

                            if (extracted_user_id && token_user_id != db_user_id) {
                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                    "Skipping token with user ID mismatch: token contains '%s' but database has '%s'",
                                    printable_user_id.c_str(), db_user_id);
                                continue;        
                            }

                            if (found_match) {
                                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                                    "Ambiguous token prefix matches multiple users: %s and %s",
                                    matched_email.c_str(), db_email);
                                sqlite3_finalize(stmt);

                                if (attempt < max_attempts) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                                    break;            
                                }
                                return false;
                            }

                            matched_email = db_email;
                            matched_user_id = db_user_id;
                            found_match = true;

                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Found matching token prefix for email: %s (user ID: %s)",
                                matched_email.c_str(), matched_user_id.c_str());
                        }
                    }
                }

                sqlite3_finalize(stmt);

                if (found_match) {
                    email = matched_email;

                    if (attempt > 1) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                            "Token prefix validation succeeded on retry attempt %d", attempt);
                    }

                    return true;
                }

                if (attempt < max_attempts) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "No token match found on attempt %d, retrying...", attempt);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    continue;
                }

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No token match found for: %s after %d attempts", token_prefix.c_str(), max_attempts);

                const char* debug_query = "SELECT email, user_id, access_token FROM users WHERE access_token IS NOT NULL AND access_token != ''";
                if (sqlite3_prepare_v2(db_, debug_query, -1, &stmt, nullptr) == SQLITE_OK) {
                    int count = 0;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Sample tokens in database:");

                    while (sqlite3_step(stmt) == SQLITE_ROW && count < 5) {
                        const char* db_email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                        const char* db_user_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                        const char* db_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                        if (db_email && db_token && db_user_id) {
                            std::string token_sample = strlen(db_token) > 10 ?
                                std::string(db_token, 10) + "..." : db_token;
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "  Email: %s, User ID: %s, Token: %s", db_email, db_user_id, token_sample.c_str());
                            count++;
                        }
                    }
                }
                sqlite3_finalize(stmt);
            }

            return false;
        }

        bool Database::get_mayhem_id(const std::string& email, std::string& mayhem_id) {
            if (!db_) return false;

            const char* sql = "SELECT mayhem_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            bool found = false;
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) {
                    mayhem_id = reinterpret_cast<const char*>(text);
                    found = true;

                    if (!mayhem_id.empty() && mayhem_id.length() != 38) {
                        if (mayhem_id.find('e') != std::string::npos || mayhem_id.find('E') != std::string::npos) {
                            mayhem_id = "20000000000000000000000000000000000001";
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Converted scientific notation mayhem ID to default 38-digit format");
                        }
                        else if (mayhem_id.find_first_not_of("0123456789") == std::string::npos) {
                            mayhem_id = std::string(38 - mayhem_id.length(), '0') + mayhem_id;
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Padded numeric mayhem ID to 38-digit format");
                        }
                    }

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found mayhem ID for email %s: %s", email.c_str(), mayhem_id.c_str());
                }
            }
            else if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to query mayhem ID: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }

            sqlite3_finalize(stmt);
            return found;
        }

        bool Database::get_user_cred(const std::string& email, std::string& user_cred) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get user credential: database not initialized");
                return false;
            }

            bool user_exists = false;
            std::string user_id;
            {
                const char* check_query = "SELECT user_id FROM users WHERE email = ? COLLATE NOCASE;";
                sqlite3_stmt* check_stmt = nullptr;
                if (sqlite3_prepare_v2(db_, check_query, -1, &check_stmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(check_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
                    if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                        const char* id = reinterpret_cast<const char*>(sqlite3_column_text(check_stmt, 0));
                        if (id) {
                            user_id = id;
                            user_exists = true;
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                                "Found user with ID %s for email %s", id, email.c_str());
                        }
                    }
                    sqlite3_finalize(check_stmt);
                }
            }

            if (!user_exists) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "User not found for email %s when retrieving credential", email.c_str());
                return false;
            }

            const char* query = "SELECT user_cred FROM users WHERE email = ? COLLATE NOCASE;";
            
            for (int attempt = 0; attempt < 3; attempt++) {
                sqlite3_stmt* stmt = nullptr;
                int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to prepare statement: %s (code: %d)",
                        sqlite3_errmsg(db_), rc);
                    continue;   
                }

                sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc == SQLITE_ROW) {
                    const char* cred = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                    if (cred) {
                        user_cred = cred;
                        sqlite3_finalize(stmt);
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                            "Successfully retrieved user credential for email %s (attempt %d)", 
                            email.c_str(), attempt + 1);
                        return true;
                    }
                }

                sqlite3_finalize(stmt);
                
                if (attempt < 2) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10 * (attempt + 1)));
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "User credential not found for email %s after multiple attempts", email.c_str());
            return false;
        }

        bool Database::update_user_cred(const std::string& email, const std::string& new_cred) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update user credential: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET user_cred = ? WHERE email = ? COLLATE NOCASE;";

            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_cred.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update user credential: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Updated user credential for email %s", email.c_str());
            return true;
        }

        bool Database::prepare_and_bind(sqlite3_stmt** stmt, const char* query) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            if (sqlite3_prepare_v2(db_, query, -1, stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            return true;
        }

        bool Database::execute_query(const char* query) {
            char* error_message = nullptr;
            int result = sqlite3_exec(db_, query, nullptr, nullptr, &error_message);

            if (result != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] SQL error: %s", error_message);
                sqlite3_free(error_message);
                return false;
            }

            return true;
        }

        bool Database::get_user_by_id(const std::string& user_id, std::string& access_token) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = "SELECT access_token FROM users WHERE user_id = ? COLLATE NOCASE;";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

            bool found = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (token) {
                    access_token = token;
                    found = true;

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] Found access token for user ID: %s", user_id.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return found;
        }

        bool Database::update_access_token_by_userid(const std::string& user_id, const std::string& new_token) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = "UPDATE users SET access_token = ? WHERE user_id = ? COLLATE NOCASE;";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_token.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);

            bool success = false;
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                int changes = sqlite3_changes(db_);
                success = (changes > 0);

                logger::write(success ? logger::LOG_LEVEL_INFO : logger::LOG_LEVEL_WARN,
                    logger::LOG_LABEL_DATABASE,
                    "[DATABASE] %s access token for user ID: %s (changes: %d)",
                    success ? "Updated" : "Failed to update", user_id.c_str(), changes);
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Error updating access token: %s", sqlite3_errmsg(db_));
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::get_display_name(const std::string& email, std::string& display_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get display name: database not initialized");
                return false;
            }

            const char* query = "SELECT display_name FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    display_name = result;
                }
                else {
                    display_name = "";
                }
                sqlite3_finalize(stmt);
                return true;
            }
            else if (rc == SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No display name found for email: %s", email.c_str());
                display_name = "";
                sqlite3_finalize(stmt);
                return false;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to get display name: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }
        }

        bool Database::update_display_name(const std::string& email, const std::string& display_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update display name: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET display_name = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, display_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update display name: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Updated display name for email %s", email.c_str());
            return true;
        }

        bool Database::clear_access_token(const std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot clear access token: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET access_token = '' WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to clear access token: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Cleared access token for email %s", email.c_str());
            return true;
        }

        bool Database::logout_user(const std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot logout user: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET access_token = '', access_code = '', mayhem_id = '', vendor_id = '', android_id = '', device_id = '', advertising_id = '', platform_id = '', combined_id = '' WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to logout user: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "User logged out successfully (cleared tokens, mayhem_id, device IDs, advertising ID, platform ID, and combined ID): %s", email.c_str());
            return true;
        }

        bool Database::get_session_key(const std::string& email, std::string& session_key) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get session key: database not initialized");
                return false;
            }

            const char* query = "SELECT session_key FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    session_key = result;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found session key for email %s: %s", email.c_str(), session_key.c_str());
                }
                else {
                    session_key = "";
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "No session key found for email %s", email.c_str());
                }
                sqlite3_finalize(stmt);
                return true;
            }
            else if (rc == SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No user found with email %s", email.c_str());
                session_key = "";
                sqlite3_finalize(stmt);
                return false;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to get session key: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }
        }

        bool Database::update_session_key(const std::string& email, const std::string& new_session_key) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update session key: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET session_key = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_session_key.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update session key: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Updated session key for email %s", email.c_str());
            return true;
        }

        bool Database::find_user_email(const std::string& land_id, const std::string& user_id, const std::string& access_token, std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot find user email: database not initialized");
                return false;
            }

            bool user_found = false;

            if (!land_id.empty()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[FIND USER] Trying to find user by land_id (as mayhem_id): %s", land_id.c_str());
                if (get_email_by_mayhem_id(land_id, email)) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[FIND USER] Found user by land_id (as mayhem_id): %s -> %s", land_id.c_str(), email.c_str());
                    user_found = true;
                }
            }

            if (!user_found && !user_id.empty()) {
                std::string user_id_email;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[FIND USER] Trying to find user by user_id: %s", user_id.c_str());
                if (get_email_by_user_id(user_id, user_id_email)) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[FIND USER] Found user by user_id: %s -> %s", user_id.c_str(), user_id_email.c_str());
                    email = user_id_email;
                    user_found = true;
                }
                else {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[FIND USER] Trying to find user by user_id (as mayhem_id): %s", user_id.c_str());
                    if (get_email_by_mayhem_id(user_id, email)) {
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                            "[FIND USER] Found user by user_id (as mayhem_id): %s -> %s", user_id.c_str(), email.c_str());
                        user_found = true;
                    }
                }
            }

            if (!user_found && !access_token.empty()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[FIND USER] Trying to find user by access_token: %s", access_token.substr(0, 10).c_str());
                if (get_email_by_token(access_token, email)) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[FIND USER] Found user by access_token: %s -> %s", access_token.substr(0, 10).c_str(), email.c_str());
                    user_found = true;
                }
            }

            if (user_found) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[FIND USER] Successfully found user email: %s", email.c_str());
                return true;
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "[FIND USER] Failed to find user with given identifiers");
                return false;
            }
        }

        bool Database::get_all_emails(std::vector<std::string>& emails) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get all emails - database not initialized");
                return false;
            }

            const char* sql = "SELECT email FROM users;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            emails.clear();
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) {
                    emails.push_back(reinterpret_cast<const char*>(text));
                }
            }

            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Error while retrieving emails: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Retrieved %zu emails from database", emails.size());
            return true;
        }

        bool Database::get_town_name(const std::string& email, std::string& town_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get town name: database not initialized");
                return false;
            }

            const char* query = "SELECT town_name FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            bool success = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    town_name = result;
                    success = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Retrieved town name for %s: %s", email.c_str(), town_name.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Town name for %s is NULL", email.c_str());
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No town name found for %s", email.c_str());
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_town_name(const std::string& email, const std::string& new_town_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update town name: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET town_name = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_town_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update town name: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Updated town name for %s to %s", email.c_str(), new_town_name.c_str());
            return true;
        }

        bool Database::get_land_save_path(const std::string& email, std::string& land_save_path) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get land save path: database not initialized");
                return false;
            }

            const char* query = "SELECT land_save_path FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            bool success = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    land_save_path = result;
                    success = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Retrieved land save path for %s: %s", email.c_str(), land_save_path.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Land save path for %s is NULL", email.c_str());
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No land save path found for %s", email.c_str());
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_land_save_path(const std::string& email, const std::string& new_land_save_path) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot update land save path: database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET land_save_path = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_land_save_path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update land save path: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Updated land save path for %s to %s", email.c_str(), new_land_save_path.c_str());
            return true;
        }

        bool Database::get_email_by_user_id(const std::string& user_id, std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get email by user_id: database not initialized");
                return false;
            }

            const char* query = "SELECT email FROM users WHERE user_id = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    email = result;
                }
                else {
                    email = "";
                }
                sqlite3_finalize(stmt);
                return true;
            }
            else if (rc == SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No email found for user_id: %s", user_id.c_str());
                email = "";
                sqlite3_finalize(stmt);
                return false;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to get email: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }
        }

        bool Database::get_email_by_mayhem_id(const std::string& mayhem_id_str, std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get email by mayhem_id: database not initialized");
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Looking up email for mayhem_id: %s", mayhem_id_str.c_str());

            std::string formatted_mayhem_id = mayhem_id_str;
            std::string standard_format = "20000000000000000000000000000000000001";

            if (mayhem_id_str.find('e') != std::string::npos || mayhem_id_str.find('E') != std::string::npos) {
                formatted_mayhem_id = standard_format;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Converted scientific notation mayhem ID to default 38-digit format: %s",
                    formatted_mayhem_id.c_str());
            }
            else if (!mayhem_id_str.empty() && mayhem_id_str.length() != 38 &&
                mayhem_id_str.find_first_not_of("0123456789") == std::string::npos) {
                formatted_mayhem_id = std::string(38 - mayhem_id_str.length(), '0') + mayhem_id_str;

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Padded numeric mayhem ID %s to 38-digit format: %s",
                    mayhem_id_str.c_str(), formatted_mayhem_id.c_str());
            }

            const char* query = "SELECT email FROM users WHERE mayhem_id = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, formatted_mayhem_id.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    email = result;
                }
                else {
                    email = "";
                }
                sqlite3_finalize(stmt);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Found email for mayhem_id: %s -> %s", mayhem_id_str.c_str(), email.c_str());
                return true;
            }
            else if (rc == SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No email found for mayhem_id: %s", mayhem_id_str.c_str());
                email = "";
                sqlite3_finalize(stmt);
                return false;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to get email: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }
        }

        bool Database::get_friend_requests(const std::string& user_id, std::vector<std::tuple<std::string, std::string, std::string, int64_t>>& requests) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                SELECT fr.from_user_id, u.display_name, u.display_name, fr.timestamp
                FROM friend_requests fr
                JOIN users u ON fr.from_user_id = u.user_id
                WHERE fr.to_user_id = ? AND fr.status = 'pending'
                ORDER BY fr.timestamp DESC
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* friend_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                int64_t timestamp = sqlite3_column_int64(stmt, 3);

                if (friend_id && display_name && nickname) {
                    requests.emplace_back(friend_id, display_name, nickname, timestamp);
                }
            }

            sqlite3_finalize(stmt);
            return true;
        }

        bool Database::get_outbound_friend_requests(const std::string& user_id, std::vector<std::tuple<std::string, std::string, std::string, int64_t>>& requests) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                SELECT fr.to_user_id, u.display_name, u.display_name, fr.timestamp
                FROM friend_requests fr
                JOIN users u ON fr.to_user_id = u.user_id
                WHERE fr.from_user_id = ? AND fr.status = 'pending'
                ORDER BY fr.timestamp DESC
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* friend_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                const char* nickname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                int64_t timestamp = sqlite3_column_int64(stmt, 3);

                if (friend_id && display_name && nickname) {
                    requests.emplace_back(friend_id, display_name, nickname, timestamp);
                }
            }

            sqlite3_finalize(stmt);
            return true;
        }

        bool Database::send_friend_request(const std::string& from_user_id, const std::string& to_user_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* check_stmt = nullptr;
            const char* check_sql = R"(
                SELECT status FROM friend_requests 
                WHERE (from_user_id = ? AND to_user_id = ?) 
                OR (from_user_id = ? AND to_user_id = ?)
            )";

            if (sqlite3_prepare_v2(db_, check_sql, -1, &check_stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare check statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(check_stmt, 1, from_user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(check_stmt, 2, to_user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(check_stmt, 3, to_user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(check_stmt, 4, from_user_id.c_str(), -1, SQLITE_TRANSIENT);

            if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                const char* status = reinterpret_cast<const char*>(sqlite3_column_text(check_stmt, 0));
                if (status && strcmp(status, "accepted") == 0) {
                    sqlite3_finalize(check_stmt);
                    return true;   
                }
            }
            sqlite3_finalize(check_stmt);

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                INSERT OR REPLACE INTO friend_requests (from_user_id, to_user_id, status, timestamp)
                VALUES (?, ?, 'pending', ?)
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            sqlite3_bind_text(stmt, 1, from_user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, to_user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int64(stmt, 3, timestamp);

            bool success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::accept_friend_request(const std::string& user_id, const std::string& friend_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                UPDATE friend_requests 
                SET status = 'accepted'
                WHERE to_user_id = ? AND from_user_id = ? AND status = 'pending'
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, friend_id.c_str(), -1, SQLITE_TRANSIENT);

            bool success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::reject_friend_request(const std::string& user_id, const std::string& friend_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                UPDATE friend_requests 
                SET status = 'rejected'
                WHERE to_user_id = ? AND from_user_id = ? AND status = 'pending'
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, friend_id.c_str(), -1, SQLITE_TRANSIENT);

            bool success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::remove_friend(const std::string& user_id, const std::string& friend_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                DELETE FROM friend_requests 
                WHERE (from_user_id = ? AND to_user_id = ?) 
                OR (from_user_id = ? AND to_user_id = ?)
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, friend_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, friend_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, user_id.c_str(), -1, SQLITE_TRANSIENT);

            bool success = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);

            return success;
        }

        bool Database::are_friends(const std::string& user_id1, const std::string& user_id2) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = R"(
                SELECT 1 FROM friend_requests 
                WHERE ((from_user_id = ? AND to_user_id = ?) 
                OR (from_user_id = ? AND to_user_id = ?))
                AND status = 'accepted'
                LIMIT 1
            )";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, user_id1.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, user_id2.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, user_id2.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, user_id1.c_str(), -1, SQLITE_TRANSIENT);

            bool are_friends = sqlite3_step(stmt) == SQLITE_ROW;
            sqlite3_finalize(stmt);

            return are_friends;
        }

        bool Database::delete_user(const std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete user, database not initialized");
                return false;
            }
            
            std::string decoded_email = url_decode(email);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Deleting user with email: %s (decoded: %s)", email.c_str(), decoded_email.c_str());

            const char* update_query = "UPDATE users SET android_id = NULL, vendor_id = NULL, client_ip = NULL, combined_id = NULL, advertising_id = NULL, platform_id = NULL WHERE email = ? COLLATE NOCASE";
            sqlite3_stmt* update_stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare update statement to clear device fields: %s", sqlite3_errmsg(db_));
                return false;
            }

            rc = sqlite3_bind_text(update_stmt, 1, decoded_email.c_str(), -1, SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email parameter in update: %s", sqlite3_errmsg(db_));
                sqlite3_finalize(update_stmt);
                return false;
            }

            rc = sqlite3_step(update_stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to clear device fields: %s", sqlite3_errmsg(db_));
                sqlite3_finalize(update_stmt);
                return false;
            }

            sqlite3_finalize(update_stmt);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Successfully cleared device fields for user: %s", email.c_str());

            const char* query = "DELETE FROM users WHERE email = ? COLLATE NOCASE";
            sqlite3_stmt* stmt = nullptr;

            rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare delete_user statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, decoded_email.c_str(), -1, SQLITE_STATIC);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email parameter in delete_user: %s", sqlite3_errmsg(db_));
                sqlite3_finalize(stmt);
                return false;
            }

            rc = sqlite3_step(stmt);
            bool query_success = (rc == SQLITE_DONE);

            if (!query_success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to delete user: %s", sqlite3_errmsg(db_));
                sqlite3_finalize(stmt);
                
                return false;
            }

            int changes = sqlite3_changes(db_);
            bool success = (changes > 0);

            if (success) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Successfully deleted user with email: %s (%d record(s) affected)",
                    decoded_email.c_str(), changes);
                
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "No records found to delete for email: %s", decoded_email.c_str());
                
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::delete_anonymous_user(const std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete anonymous user - database not initialized");
                return false;
            }

            if (email.substr(0, 10) != "anonymous_") {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete non-anonymous user: %s", email.c_str());
                return false;
            }

            const char* sql = "DELETE FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for deleting anonymous user: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

            rc = sqlite3_step(stmt);
            bool delete_success = (rc == SQLITE_DONE);

            if (!delete_success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to delete anonymous user: %s (code: %d)", email.c_str(), rc);
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Successfully deleted anonymous user: %s", email.c_str());
            }

            sqlite3_finalize(stmt);
            return delete_success;
        }

        std::string Database::url_decode(const std::string& encoded) {
            std::string result;
            result.reserve(encoded.length());

            for (size_t i = 0; i < encoded.length(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.length()) {
                    int value = 0;
                    std::istringstream is(encoded.substr(i + 1, 2));
                    if (is >> std::hex >> value) {
                        result += static_cast<char>(value);
                        i += 2;
                    }
                    else {
                        result += encoded[i];
                    }
                }
                else if (encoded[i] == '+') {
                    result += ' ';
                }
                else {
                    result += encoded[i];
                }
            }

            return result;
        }

        bool Database::delete_anonymous_users_by_token(const std::string& access_token) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete anonymous users by token - database not initialized");
                return false;
            }

            if (access_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete anonymous users - access token is empty");
                return false;
            }

            const char* find_sql = "SELECT email FROM users WHERE email LIKE 'anonymous_%' AND access_token = ? COLLATE NOCASE;";
            sqlite3_stmt* find_stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, find_sql, -1, &find_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for finding anonymous users: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(find_stmt, 1, access_token.c_str(), -1, SQLITE_TRANSIENT);

            std::vector<std::string> emails_to_delete;

            while ((rc = sqlite3_step(find_stmt)) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(find_stmt, 0));
                if (email) {
                    emails_to_delete.push_back(email);
                }
            }

            sqlite3_finalize(find_stmt);

            if (emails_to_delete.empty()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No anonymous users found with the given access token");
                return true;       
            }

            const char* delete_sql = "DELETE FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* delete_stmt = nullptr;

            rc = sqlite3_prepare_v2(db_, delete_sql, -1, &delete_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for deleting anonymous users: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            bool all_success = true;

            for (const auto& email : emails_to_delete) {
                sqlite3_reset(delete_stmt);
                sqlite3_bind_text(delete_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

                rc = sqlite3_step(delete_stmt);
                bool delete_success = (rc == SQLITE_DONE);

                if (!delete_success) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to delete anonymous user: %s (code: %d)", email.c_str(), rc);
                    all_success = false;
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Successfully deleted anonymous user: %s", email.c_str());
                }
            }

            sqlite3_finalize(delete_stmt);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Deleted %zu anonymous users with the given access token", emails_to_delete.size());

            return all_success;
        }

        bool Database::delete_anonymous_users_by_user_id(const std::string& user_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete anonymous users by user ID - database not initialized");
                return false;
            }

            if (user_id.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot delete anonymous users - user ID is empty");
                return false;
            }

            const char* find_sql = "SELECT email FROM users WHERE email LIKE 'anonymous_%' AND user_id = ? COLLATE NOCASE;";
            sqlite3_stmt* find_stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, find_sql, -1, &find_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for finding anonymous users by user ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(find_stmt, 1, user_id.c_str(), -1, SQLITE_TRANSIENT);

            std::vector<std::string> emails_to_delete;

            while ((rc = sqlite3_step(find_stmt)) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(find_stmt, 0));
                if (email) {
                    emails_to_delete.push_back(email);
                }
            }

            sqlite3_finalize(find_stmt);

            if (emails_to_delete.empty()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No anonymous users found with the given user ID");
                return true;       
            }

            const char* delete_sql = "DELETE FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* delete_stmt = nullptr;

            rc = sqlite3_prepare_v2(db_, delete_sql, -1, &delete_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement for deleting anonymous users: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            bool all_success = true;

            for (const auto& email : emails_to_delete) {
                sqlite3_reset(delete_stmt);
                sqlite3_bind_text(delete_stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);

                rc = sqlite3_step(delete_stmt);
                bool delete_success = (rc == SQLITE_DONE);

                if (!delete_success) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "Failed to delete anonymous user: %s (code: %d)", email.c_str(), rc);
                    all_success = false;
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "Successfully deleted anonymous user: %s", email.c_str());
                }
            }

            sqlite3_finalize(delete_stmt);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Deleted %zu anonymous users with the given user ID", emails_to_delete.size());

            return all_success;
        }

        bool Database::get_device_id(const std::string& email, std::string& device_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT device_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    device_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_device_id(const std::string& email, const std::string& device_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET device_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update device ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::get_android_id(const std::string& email, std::string& android_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT android_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    android_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_android_id(const std::string& email, const std::string& android_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET android_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, android_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update android ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::get_vendor_id(const std::string& email, std::string& vendor_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT vendor_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    vendor_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_vendor_id(const std::string& email, const std::string& vendor_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET vendor_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, vendor_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update vendor ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::check_android_id_exists(const std::string& android_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check android_id: database not initialized");
                return false;
            }

            if (android_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE android_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, android_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing android_id %s for email %s", android_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::check_vendor_id_exists(const std::string& vendor_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check vendor_id: database not initialized");
                return false;
            }

            if (vendor_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE vendor_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, vendor_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing vendor_id %s for email %s", vendor_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::check_android_id_ip_exists(const std::string& combined_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check android_id_ip: database not initialized");
                return false;
            }

            if (combined_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE combined_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, combined_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing combined_id %s for email %s", combined_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::check_vendor_id_ip_exists(const std::string& combined_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check vendor_id_ip: database not initialized");
                return false;
            }

            if (combined_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE combined_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, combined_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing combined_id %s for email %s", combined_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::validate_token_and_userid(const std::string& token, const std::string& user_id) {
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot validate token and user ID - database not initialized");
                return false;
            }

            std::string token_prefix = token.length() > 10 ? token.substr(0, 10) + "..." : token;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Validating token: %s and user ID: %s", token_prefix.c_str(), user_id.c_str());

            std::string email;
            if (!get_email_by_token(token, email)) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No email found for token: %s", token_prefix.c_str());
                return false;
            }

            std::string db_user_id;
            if (!get_user_id(email, db_user_id)) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No user_id found for email: %s", email.c_str());
                return false;
            }

            bool match = (user_id == db_user_id);
            if (match) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Token and user ID match validated for user: %s", email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "Token and user ID mismatch! Token belongs to %s with ID %s, but provided ID was %s",
                    email.c_str(), db_user_id.c_str(), user_id.c_str());
            }

            return match;
        }

        bool Database::get_client_ip(const std::string& email, std::string& client_ip) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT client_ip FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    client_ip = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_client_ip(const std::string& email, const std::string& client_ip) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET client_ip = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, client_ip.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update client IP: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::get_combined_id(const std::string& email, std::string& combined_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT combined_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    combined_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_combined_id(const std::string& email, const std::string& combined_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET combined_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, combined_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update combined ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::get_advertising_id(const std::string& email, std::string& advertising_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT advertising_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    advertising_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_advertising_id(const std::string& email, const std::string& advertising_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET advertising_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, advertising_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update advertising ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::get_platform_id(const std::string& email, std::string& platform_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT platform_id FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    platform_id = result;
                    success = true;
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::update_platform_id(const std::string& email, const std::string& platform_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "UPDATE users SET platform_id = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, platform_id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update platform ID: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            return true;
        }

        bool Database::check_advertising_id_exists(const std::string& advertising_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check advertising_id: database not initialized");
                return false;
            }

            if (advertising_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE advertising_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, advertising_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing advertising_id %s for email %s", advertising_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::check_combined_id_exists(const std::string& combined_id, std::string& existing_email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot check combined_id: database not initialized");
                return false;
            }

            if (combined_id.empty()) {
                return false;
            }

            const char* query = "SELECT email FROM users WHERE combined_id = ? LIMIT 1";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, combined_id.c_str(), -1, SQLITE_STATIC);

            bool exists = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email) {
                    existing_email = email;
                    exists = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found existing combined_id %s for email %s", combined_id.c_str(), existing_email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return exists;
        }

        bool Database::get_user_by_device_id(const std::string& device_id, std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Database not initialized");
                return false;
            }

            sqlite3_stmt* stmt = nullptr;
            const char* sql = "SELECT email FROM users WHERE device_id = ? COLLATE NOCASE;";

            if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, device_id.c_str(), -1, SQLITE_TRANSIENT);

            bool found = false;
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const char* email_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (email_value) {
                    email = email_value;
                    found = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "[DATABASE] Found email %s for device ID: %s", email.c_str(), device_id.c_str());
                }
            }

            sqlite3_finalize(stmt);

            if (!found) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] No email found for device ID: %s", device_id.c_str());
            }

            return found;
        }

        bool Database::get_user_by_ip(const std::string& client_ip, std::string& email) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT email FROM users WHERE client_ip = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, client_ip.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            bool success = false;
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    email = result;
                    success = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found user by IP: %s -> %s", client_ip.c_str(), email.c_str());
                }
            }

            sqlite3_finalize(stmt);
            return success;
        }

        bool Database::get_users_by_ip(const std::string& client_ip, std::vector<std::string>& emails) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Cannot get users by IP: database not initialized");
                return false;
            }

            const char* query = "SELECT email FROM users WHERE client_ip = ?;";
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            sqlite3_bind_text(stmt, 1, client_ip.c_str(), -1, SQLITE_STATIC);

            emails.clear();
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    emails.push_back(result);
                }
            }

            sqlite3_finalize(stmt);

            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Error while retrieving users by IP: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Found %zu users with IP %s", emails.size(), client_ip.c_str());
            return !emails.empty();
        }

        //bool Database::sync_user_id(const std::string& email, bool force_session) {
        //    std::lock_guard<std::mutex> lock(mutex_);

        //}
        bool Database::set_display_name(const std::string& email, const std::string& display_name) {
            std::lock_guard<std::mutex> lock(mutex_);
            try {
                sqlite3_stmt* stmt = nullptr;
                const char* sql = "UPDATE users SET display_name = ? WHERE email = ?";

                if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "[SET DISPLAY NAME] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                    return false;
                }

                sqlite3_bind_text(stmt, 1, display_name.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

                int result = sqlite3_step(stmt);
                sqlite3_finalize(stmt);

                if (result != SQLITE_DONE) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "[SET DISPLAY NAME] Failed to update display name: %s", sqlite3_errmsg(db_));
                    return false;
                }

                return true;
            }
            catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[SET DISPLAY NAME] Exception: %s", e.what());
                return false;
            }
        }

        bool Database::user_id_exists(const std::string& user_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            try {
                sqlite3_stmt* stmt = nullptr;
                const char* sql = "SELECT COUNT(*) FROM users WHERE user_id = ?";

                if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                        "[USER ID EXISTS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                    return false;
                }

                sqlite3_bind_text(stmt, 1, user_id.c_str(), -1, SQLITE_STATIC);

                int count = 0;
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    count = sqlite3_column_int(stmt, 0);
                }

                sqlite3_finalize(stmt);
                return count > 0;
            }
            catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "[USER ID EXISTS] Exception: %s", e.what());
                return false;
            }
        }

        bool Database::get_last_user_id(std::string& user_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT user_id FROM users ORDER BY rowid DESC LIMIT 1;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare get_last_user_id query: %s", sqlite3_errmsg(db_));
                return false;
            }

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    user_id = result;
                    sqlite3_finalize(stmt);
                    return true;
                }
            }

            sqlite3_finalize(stmt);
            return false;
        }

        bool Database::is_mayhem_id_in_use(const std::string& mayhem_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT COUNT(*) FROM users WHERE mayhem_id = ?;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare is_mayhem_id_in_use query: %s", sqlite3_errmsg(db_));
                return false;
            }

            sqlite3_bind_text(stmt, 1, mayhem_id.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                int count = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                
                if (count > 0) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Mayhem ID %s is already in use by %d users", mayhem_id.c_str(), count);
                    return true;
                } else {
                    return false;
                }
            }

            sqlite3_finalize(stmt);
            return false;           
        }

        bool Database::get_last_mayhem_id(std::string& mayhem_id) {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Database not initialized");
                return false;
            }

            const char* query = "SELECT mayhem_id FROM users ORDER BY rowid DESC LIMIT 1;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare get_last_mayhem_id query: %s", sqlite3_errmsg(db_));
                return false;
            }

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                if (result) {
                    mayhem_id = result;
                    sqlite3_finalize(stmt);
                    return true;
                }
            }

            sqlite3_finalize(stmt);
            return false;
        }

        bool Database::get_lnglv_token(const std::string& email, std::string& lnglv_token) {
            if (!db_) return false;

            const char* sql = "SELECT lnglv_token FROM users WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            bool found = false;
            rc = sqlite3_step(stmt);

            if (rc == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) {
                    lnglv_token = reinterpret_cast<const char*>(text);
                    found = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                        "Found lnglv_token for email %s", email.c_str());
                }
            }
            else if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to query lnglv_token: %s (code: %d)", sqlite3_errmsg(db_), rc);
            }

            sqlite3_finalize(stmt);
            return found;
        }

        bool Database::update_lnglv_token(const std::string& email, const std::string& new_lnglv_token) {
            if (!db_) return false;

            std::string access_code;
            get_access_code(email, access_code);

            const char* sql = "UPDATE users SET lnglv_token = ?, access_code = ? WHERE email = ? COLLATE NOCASE;";
            sqlite3_stmt* stmt = nullptr;

            int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to prepare statement: %s (code: %d)",
                    sqlite3_errmsg(db_), rc);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 1, new_lnglv_token.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind new lnglv_token: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 2, access_code.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind access_code: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            rc = sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to update lnglv_token: %s (code: %d)", sqlite3_errmsg(db_), rc);
                sqlite3_finalize(stmt);
                return false;
            }

            int changes = sqlite3_changes(db_);
            sqlite3_finalize(stmt);

            if (changes > 0) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Updated lnglv_token for email %s while preserving access_code: %s",
                    email.c_str(), access_code.c_str());
                return true;
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "No rows updated when setting lnglv_token for email %s (access_code: %s)",
                    email.c_str(), access_code.c_str());
                return false;
            }
        }

    }

    bool tsto::database::Database::get_manufacturer(const std::string& email, std::string& manufacturer) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot get manufacturer: database not initialized");
            return false;
        }

        const char* query = "SELECT manufacturer FROM users WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

        bool success = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (result) {
                manufacturer = result;
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Retrieved manufacturer for %s: %s", email.c_str(), manufacturer.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Manufacturer for %s is NULL", email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "No manufacturer found for %s", email.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::update_manufacturer(const std::string& email, const std::string& manufacturer) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot update manufacturer: database not initialized");
            return false;
        }

        const char* query = "UPDATE users SET manufacturer = ? WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, manufacturer.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update manufacturer: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Updated manufacturer for %s to %s", email.c_str(), manufacturer.c_str());
        return true;
    }

    bool tsto::database::Database::get_model(const std::string& email, std::string& model) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot get model: database not initialized");
            return false;
        }

        const char* query = "SELECT model FROM users WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

        bool success = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (result) {
                model = result;
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Retrieved model for %s: %s", email.c_str(), model.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Model for %s is NULL", email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "No model found for %s", email.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::update_model(const std::string& email, const std::string& model) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot update model: database not initialized");
            return false;
        }

        const char* query = "UPDATE users SET model = ? WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, model.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update model: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Updated model for %s to %s", email.c_str(), model.c_str());
        return true;
    }

    bool tsto::database::Database::get_email_by_town_path(const std::string& town_path, std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot get email by town path: database not initialized");
            return false;
        }

        const char* query = "SELECT email FROM users WHERE land_save_path = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, town_path.c_str(), -1, SQLITE_STATIC);

        bool success = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (result) {
                email = result;
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Found email %s for town path: %s", email.c_str(), town_path.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Email for town path %s is NULL", town_path.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "No email found for town path: %s", town_path.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::get_emails_by_device_id(const std::string& device_id, std::vector<std::string>& emails) {
        std::lock_guard<std::mutex> lock(mutex_);
        emails.clear();

        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Database not initialized");
            return false;
        }

        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT email FROM users WHERE device_id = ?;";

        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s", sqlite3_errmsg(db_));
            return false;
        }

        sqlite3_bind_blob(stmt, 1, device_id.data(), static_cast<int>(device_id.size()), SQLITE_TRANSIENT);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (email) {
                emails.push_back(email);
            }
        }

        sqlite3_finalize(stmt);
        return true;
    }

    bool tsto::database::Database::get_land_token(const std::string& email, std::string& land_token) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot get land_token: database not initialized");
            return false;
        }

        const char* query = "SELECT land_token FROM users WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

        bool success = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (result) {
                land_token = result;
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Found land_token %s for email: %s", land_token.c_str(), email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] land_token for email %s is NULL", email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "[DATABASE] No land_token found for email: %s", email.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::update_land_token(const std::string& email, const std::string& new_land_token) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot update land_token: database not initialized");
            return false;
        }

        const char* query = "UPDATE users SET land_token = ? WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, new_land_token.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to update land_token: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "[DATABASE] Updated land_token for %s to %s", email.c_str(), new_land_token.c_str());
        return true;
    }

    bool tsto::database::Database::get_anon_uid(const std::string& email, std::string& anon_uid) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot get anon_uid: database not initialized");
            return false;
        }

        const char* query = "SELECT anon_uid FROM users WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        bool success = false;

        if (rc == SQLITE_ROW) {
            const unsigned char* uid_text = sqlite3_column_text(stmt, 0);
            if (uid_text) {
                anon_uid = reinterpret_cast<const char*>(uid_text);
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Found anon_uid %s for email: %s", anon_uid.c_str(), email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] anon_uid for email %s is NULL", email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "[DATABASE] No anon_uid found for email: %s", email.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::update_anon_uid(const std::string& email, const std::string& new_anon_uid) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot update anon_uid: database not initialized");
            return false;
        }

        const char* query = "UPDATE users SET anon_uid = ? WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, new_anon_uid.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to update anon_uid: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "[DATABASE] Updated anon_uid for %s to %s", email.c_str(), new_anon_uid.c_str());
        return true;
    }

    bool tsto::database::Database::get_user_by_anon_uid(const std::string& anon_uid, std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot get user by anon_uid: database not initialized");
            return false;
        }

        const char* query = "SELECT email FROM users WHERE anon_uid = ? COLLATE NOCASE;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, anon_uid.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        bool success = false;

        if (rc == SQLITE_ROW) {
            const unsigned char* email_text = sqlite3_column_text(stmt, 0);
            if (email_text) {
                email = reinterpret_cast<const char*>(email_text);
                success = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Found user %s for anon_uid: %s", email.c_str(), anon_uid.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[DATABASE] Email for anon_uid %s is NULL", anon_uid.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "[DATABASE] No user found for anon_uid: %s", anon_uid.c_str());
        }

        sqlite3_finalize(stmt);
        return success;
    }

    bool tsto::database::Database::begin_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot begin transaction: database not initialized");
            return false;
        }

        if (transaction_active_) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Transaction already active, cannot begin a new one");
            return false;
        }

        const char* query = "BEGIN TRANSACTION;";
        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, query, nullptr, nullptr, &error_msg);

        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to begin transaction: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        transaction_active_ = true;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "[DATABASE] Transaction started");
        return true;
    }

    bool tsto::database::Database::commit_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot commit transaction: database not initialized");
            return false;
        }

        if (!transaction_active_) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "[DATABASE] No active transaction to commit");
            return false;
        }

        const char* query = "COMMIT;";
        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, query, nullptr, nullptr, &error_msg);

        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to commit transaction: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        transaction_active_ = false;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "[DATABASE] Transaction committed");
        return true;
    }

    bool tsto::database::Database::rollback_transaction() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Cannot rollback transaction: database not initialized");
            return false;
        }

        if (!transaction_active_) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "[DATABASE] No active transaction to rollback");
            return false;
        }

        const char* query = "ROLLBACK;";
        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, query, nullptr, nullptr, &error_msg);

        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[DATABASE] Failed to rollback transaction: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        transaction_active_ = false;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "[DATABASE] Transaction rolled back");
        return true;
    }

    bool tsto::database::Database::is_transaction_active() {
        std::lock_guard<std::mutex> lock(mutex_);
        return transaction_active_;
    }

    bool tsto::database::Database::get_user_by_as_identifier(const std::string& as_identifier, std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Database not initialized");
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Looking up user by AS identifier: %s", as_identifier.c_str());

        const char* query = "SELECT email FROM users WHERE as_identifier = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, as_identifier.c_str(), -1, SQLITE_STATIC);

        bool found = false;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* email_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (email_str) {
                email = email_str;
                found = true;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Found user with AS identifier %s: %s", as_identifier.c_str(), email.c_str());
            }
        }

        sqlite3_finalize(stmt);
        
        if (!found) {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "No user found with AS identifier: %s", as_identifier.c_str());
        }
        
        return found;
    }

    bool tsto::database::Database::update_as_identifier(const std::string& email, const std::string& as_identifier) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Database not initialized");
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Updating AS identifier for user %s: %s", email.c_str(), as_identifier.c_str());

        const char* query = "UPDATE users SET as_identifier = ? WHERE email = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, as_identifier.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update AS identifier for user %s: %s (code: %d)",
                email.c_str(), sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Successfully updated AS identifier for user %s", email.c_str());
        return true;
    }
    
    bool tsto::database::Database::get_as_identifier(const std::string& email, std::string& as_identifier) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Database not initialized");
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Getting AS identifier for user %s", email.c_str());

        const char* query = "SELECT as_identifier FROM users WHERE email = ?;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (id) {
                as_identifier = id;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Got AS identifier for user %s: %s", email.c_str(), as_identifier.c_str());
            } else {
                as_identifier = "";
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "No AS identifier found for user %s", email.c_str());
            }
            sqlite3_finalize(stmt);
            return true;
        }

        sqlite3_finalize(stmt);
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to get AS identifier for user %s: %s (code: %d)",
            email.c_str(), sqlite3_errmsg(db_), rc);
        return false;
    }
}