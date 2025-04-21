#include <std_include.hpp>
#include "pending_towns.hpp"
#include "debugging/serverlog.hpp"
#include "tsto/land/new_land.hpp"
#include <filesystem>
#include <random>

namespace tsto {
    namespace database {
        
        constexpr const char* PENDING_TOWNS_DB_FILE = "tsto_pending_towns.db";

        PendingTowns& PendingTowns::get_instance() {
            static PendingTowns instance;
            return instance;
        }

        PendingTowns::PendingTowns() : db_(nullptr) {
            initialize();
        }

        PendingTowns::~PendingTowns() {
            close();
        }

        bool PendingTowns::initialize() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
            }

            int rc = sqlite3_open(PENDING_TOWNS_DB_FILE, &db_);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to open database: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Using dedicated database file: %s", PENDING_TOWNS_DB_FILE);
            
            sqlite3_busy_timeout(db_, 5000);     
            
            sqlite3_extended_result_codes(db_, 1);
            
            char* error_message = nullptr;
            rc = sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &error_message);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to set journal mode: %s", error_message);
                sqlite3_free(error_message);
            }

            if (!create_tables()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to create tables");
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Database initialized successfully");
            return true;
        }

        void PendingTowns::close() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (db_) {
                sqlite3_close(db_);
                db_ = nullptr;
            }
        }

        bool PendingTowns::create_tables() {
            const char* create_table_query = 
                "CREATE TABLE IF NOT EXISTS pending_towns ("
                "id TEXT PRIMARY KEY,"
                "email TEXT NOT NULL,"
                "town_name TEXT NOT NULL,"
                "description TEXT,"
                "file_path TEXT NOT NULL,"
                "file_size INTEGER NOT NULL,"
                "submitted_at INTEGER NOT NULL,"
                "status TEXT NOT NULL DEFAULT 'pending',"
                "rejection_reason TEXT"
                ");";

            char* error_message = nullptr;
            int rc = sqlite3_exec(db_, create_table_query, nullptr, nullptr, &error_message);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to create table: %s", error_message);
                sqlite3_free(error_message);
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Table created or already exists");
            return true;
        }

        std::string PendingTowns::generate_unique_id() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 35);
            
            const char* chars = "0123456789abcdefghijklmnopqrstuvwxyz";
            std::string id;
            
            for (int i = 0; i < 16; i++) {
                id += chars[dis(gen)];
            }
            
            return id;
        }

        std::optional<std::string> PendingTowns::add_pending_town(
            const std::string& email,
            const std::string& town_name,
            const std::string& description,
            const std::string& file_path,
            size_t file_size
        ) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return std::nullopt;
            }

            std::string id = generate_unique_id();
            
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch()).count();
            
            const char* insert_query = 
                "INSERT INTO pending_towns (id, email, town_name, description, file_path, file_size, submitted_at, status) "
                "VALUES (?, ?, ?, ?, ?, ?, ?, 'pending');";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, insert_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, town_name.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, description.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 5, file_path.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 6, static_cast<sqlite3_int64>(file_size));
            sqlite3_bind_int64(stmt, 7, static_cast<sqlite3_int64>(timestamp));
            
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to insert pending town: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Added pending town with ID: %s, Email: %s, Town: %s", 
                id.c_str(), email.c_str(), town_name.c_str());
            
            return id;
        }

        std::vector<PendingTownData> PendingTowns::get_pending_towns() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            std::vector<PendingTownData> towns;
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return towns;
            }
            
            const char* select_query = 
                "SELECT id, email, town_name, description, file_path, file_size, submitted_at, status, rejection_reason "
                "FROM pending_towns "
                "WHERE status = 'pending' "
                "ORDER BY submitted_at DESC;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, select_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return towns;
            }
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                PendingTownData town;
                
                const unsigned char* id_text = sqlite3_column_text(stmt, 0);
                const unsigned char* email_text = sqlite3_column_text(stmt, 1);
                const unsigned char* town_name_text = sqlite3_column_text(stmt, 2);
                const unsigned char* description_text = sqlite3_column_text(stmt, 3);
                const unsigned char* file_path_text = sqlite3_column_text(stmt, 4);
                const unsigned char* timestamp_text = sqlite3_column_text(stmt, 6);
                const unsigned char* status_text = sqlite3_column_text(stmt, 7);
                const unsigned char* rejection_reason_text = sqlite3_column_text(stmt, 8);
                
                town.id = id_text ? reinterpret_cast<const char*>(id_text) : "";
                town.email = email_text ? reinterpret_cast<const char*>(email_text) : "";
                town.town_name = town_name_text ? reinterpret_cast<const char*>(town_name_text) : "";
                town.description = description_text ? reinterpret_cast<const char*>(description_text) : "";
                town.file_path = file_path_text ? reinterpret_cast<const char*>(file_path_text) : "";
                town.file_size = sqlite3_column_int64(stmt, 5);
                
                sqlite3_int64 timestamp_val = sqlite3_column_int64(stmt, 6);
                
                std::time_t time_t_val = static_cast<std::time_t>(timestamp_val);
                std::tm tm_info = {};
                if (localtime_s(&tm_info, &time_t_val) == 0) {
                    char time_str[100] = {0};
                    if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                        town.timestamp_str = time_str;
                    } else {
                        town.timestamp_str = "Unknown";
                    }
                    
                    town.submitted_at = std::chrono::system_clock::from_time_t(time_t_val);
                } else {
                    town.timestamp_str = "Unknown";
                    town.submitted_at = std::chrono::system_clock::now();
                }
                
                town.status = status_text ? reinterpret_cast<const char*>(status_text) : "pending";
                town.rejection_reason = rejection_reason_text ? reinterpret_cast<const char*>(rejection_reason_text) : "";
                
                towns.push_back(town);
            }
            
            sqlite3_finalize(stmt);
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Retrieved %zu pending towns", towns.size());
            
            return towns;
        }

        std::optional<PendingTownData> PendingTowns::get_pending_town(const std::string& id) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return std::nullopt;
            }
            
            const char* select_query = 
                "SELECT id, email, town_name, description, file_path, file_size, submitted_at, status, rejection_reason "
                "FROM pending_towns "
                "WHERE id = ?;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, select_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            
            PendingTownData town;
            bool found = false;
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* id_text = sqlite3_column_text(stmt, 0);
                const unsigned char* email_text = sqlite3_column_text(stmt, 1);
                const unsigned char* town_name_text = sqlite3_column_text(stmt, 2);
                const unsigned char* description_text = sqlite3_column_text(stmt, 3);
                const unsigned char* file_path_text = sqlite3_column_text(stmt, 4);
                const unsigned char* timestamp_text = sqlite3_column_text(stmt, 6);
                const unsigned char* status_text = sqlite3_column_text(stmt, 7);
                const unsigned char* rejection_reason_text = sqlite3_column_text(stmt, 8);
                
                town.id = id_text ? reinterpret_cast<const char*>(id_text) : "";
                town.email = email_text ? reinterpret_cast<const char*>(email_text) : "";
                town.town_name = town_name_text ? reinterpret_cast<const char*>(town_name_text) : "";
                town.description = description_text ? reinterpret_cast<const char*>(description_text) : "";
                town.file_path = file_path_text ? reinterpret_cast<const char*>(file_path_text) : "";
                town.file_size = sqlite3_column_int64(stmt, 5);
                
                sqlite3_int64 timestamp_val = sqlite3_column_int64(stmt, 6);
                
                std::time_t time_t_val = static_cast<std::time_t>(timestamp_val);
                std::tm tm_info = {};
                if (localtime_s(&tm_info, &time_t_val) == 0) {
                    char time_str[100] = {0};
                    if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                        town.timestamp_str = time_str;
                    } else {
                        town.timestamp_str = "Unknown";
                    }
                    
                    town.submitted_at = std::chrono::system_clock::from_time_t(time_t_val);
                } else {
                    town.timestamp_str = "Unknown";
                    town.submitted_at = std::chrono::system_clock::now();
                }
                
                town.status = status_text ? reinterpret_cast<const char*>(status_text) : "pending";
                town.rejection_reason = rejection_reason_text ? reinterpret_cast<const char*>(rejection_reason_text) : "";
                
                found = true;
            }
            
            sqlite3_finalize(stmt);
            
            if (!found) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] No pending town found with ID: %s", id.c_str());
                return std::nullopt;
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Retrieved pending town with ID: %s", id.c_str());
            
            return town;
        }

        bool PendingTowns::approve_town(const std::string& id, const std::string& target_email) {
            PendingTownData town;
            {
                std::lock_guard<std::recursive_mutex> lock(mutex_);
                
                if (!db_) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Database not initialized");
                    return false;
                }
                
                const char* select_query = 
                    "SELECT id, email, town_name, description, file_path, file_size, submitted_at, status, rejection_reason "
                    "FROM pending_towns WHERE id = ?;";
                
                sqlite3_stmt* stmt = nullptr;
                int rc = sqlite3_prepare_v2(db_, select_query, -1, &stmt, nullptr);
                
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                    return false;
                }
                
                sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
                
                rc = sqlite3_step(stmt);
                
                if (rc != SQLITE_ROW) {
                    sqlite3_finalize(stmt);
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Town not found with ID: %s", id.c_str());
                    return false;
                }
                
                town.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                town.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                town.town_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                town.description = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                town.file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
                town.file_size = sqlite3_column_int64(stmt, 5);
                
                sqlite3_int64 timestamp_val = sqlite3_column_int64(stmt, 6);
                
                std::time_t time_t_val = static_cast<std::time_t>(timestamp_val);
                std::tm tm_info = {};
                
                #ifdef _WIN32
                localtime_s(&tm_info, &time_t_val);
                #else
                localtime_r(&time_t_val, &tm_info);
                #endif
                
                char time_str[100] = {0};
                if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                    town.timestamp_str = time_str;
                } else {
                    town.timestamp_str = "Unknown";
                }
                
                town.submitted_at = std::chrono::system_clock::from_time_t(time_t_val);
                
                town.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
                
                if (sqlite3_column_text(stmt, 8)) {
                    town.rejection_reason = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 8));
                }
                
                sqlite3_finalize(stmt);
            }
            
            if (town.status != "pending") {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Town with ID %s is already %s", id.c_str(), town.status.c_str());
                return false;
            }
            
            std::string email_to_use = target_email.empty() ? town.email : target_email;
            
            if (!tsto::land::Land::import_town_file(town.file_path, email_to_use)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to import town file: %s", town.file_path.c_str());
                return false;
            }
            
            const int max_retries = 5;
            const int retry_delay_ms = 500;    
            bool update_success = false;
            
            for (int retry = 0; retry < max_retries; retry++) {
                try {
                    std::lock_guard<std::recursive_mutex> lock(mutex_);
                    
                    if (!db_) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                            "[PENDING_TOWNS] Database not initialized");
                        return false;
                    }
                    
                    const char* update_query = 
                        "UPDATE pending_towns SET status = 'approved' WHERE id = ?;";
                    
                    sqlite3_stmt* stmt = nullptr;
                    int rc = sqlite3_prepare_v2(db_, update_query, -1, &stmt, nullptr);
                    
                    if (rc != SQLITE_OK) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                            "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                        sqlite3_finalize(stmt);
                        continue;   
                    }
                    
                    sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
                    
                    rc = sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                    
                    if (rc != SQLITE_DONE) {
                        if (rc == SQLITE_BUSY || rc == SQLITE_LOCKED) {
                            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE, 
                                "[PENDING_TOWNS] Database is locked, retrying update (attempt %d of %d)...", 
                                retry + 1, max_retries);
                            
                            std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
                            continue;
                        } else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                                "[PENDING_TOWNS] Failed to update town status: %s (error code: %d)", 
                                sqlite3_errmsg(db_), rc);
                            return false;
                        }
                    }
                    
                    update_success = true;
                    break;
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Exception during database update: %s (attempt %d of %d)", 
                        ex.what(), retry + 1, max_retries);
                    
                    std::this_thread::sleep_for(std::chrono::milliseconds(retry_delay_ms));
                }
            }
            
            if (!update_success) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to update town status after %d attempts", max_retries);
                
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Note: Town file was successfully imported despite database update failure");
                
                return true;
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Approved town with ID: %s, Email: %s", id.c_str(), email_to_use.c_str());
            
            return true;
        }

        bool PendingTowns::reject_town(const std::string& id, const std::string& reason) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return false;
            }
            
            auto town_opt = get_pending_town(id);
            if (!town_opt) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Town not found with ID: %s", id.c_str());
                return false;
            }
            
            auto town = town_opt.value();
            
            if (town.status != "pending") {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Town with ID %s is already %s", id.c_str(), town.status.c_str());
                return false;
            }
            
            const char* update_query = 
                "UPDATE pending_towns SET status = 'rejected', rejection_reason = ? WHERE id = ?;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, update_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            sqlite3_bind_text(stmt, 1, reason.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, id.c_str(), -1, SQLITE_STATIC);
            
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to update town status: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            const char* delete_query = "DELETE FROM pending_towns WHERE id = ?;";
            
            stmt = nullptr;
            rc = sqlite3_prepare_v2(db_, delete_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare delete statement: %s", sqlite3_errmsg(db_));
            } else {
                sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
                
                rc = sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                
                if (rc != SQLITE_DONE) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Failed to delete rejected town record: %s", sqlite3_errmsg(db_));
                } else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Deleted rejected town record from database: %s", id.c_str());
                }
            }
            
            try {
                if (std::filesystem::exists(town.file_path)) {
                    std::filesystem::remove(town.file_path);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Deleted rejected town file: %s", town.file_path.c_str());
                }
            } catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to delete rejected town file: %s - %s", 
                    town.file_path.c_str(), ex.what());
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Rejected town with ID: %s, Reason: %s", 
                id.c_str(), reason.empty() ? "No reason provided" : reason.c_str());
            
            return true;
        }

        std::optional<PendingTownData> PendingTowns::get_and_approve_town(const std::string& id, const std::string& target_email) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return std::nullopt;
            }
            
            const char* query = 
                "SELECT id, email, town_name, description, file_path, file_size, submitted_at, status, rejection_reason "
                "FROM pending_towns WHERE id = ?;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            
            PendingTownData town;
            bool found = false;
            
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* id_text = sqlite3_column_text(stmt, 0);
                const unsigned char* email_text = sqlite3_column_text(stmt, 1);
                const unsigned char* town_name_text = sqlite3_column_text(stmt, 2);
                const unsigned char* description_text = sqlite3_column_text(stmt, 3);
                const unsigned char* file_path_text = sqlite3_column_text(stmt, 4);
                const unsigned char* timestamp_text = sqlite3_column_text(stmt, 6);
                const unsigned char* status_text = sqlite3_column_text(stmt, 7);
                const unsigned char* rejection_reason_text = sqlite3_column_text(stmt, 8);
                
                town.id = id_text ? reinterpret_cast<const char*>(id_text) : "";
                town.email = email_text ? reinterpret_cast<const char*>(email_text) : "";
                town.town_name = town_name_text ? reinterpret_cast<const char*>(town_name_text) : "";
                town.description = description_text ? reinterpret_cast<const char*>(description_text) : "";
                town.file_path = file_path_text ? reinterpret_cast<const char*>(file_path_text) : "";
                town.file_size = sqlite3_column_int64(stmt, 5);
                
                sqlite3_int64 timestamp_val = sqlite3_column_int64(stmt, 6);
                
                std::time_t time_t_val = static_cast<std::time_t>(timestamp_val);
                std::tm tm_info = {};
                if (localtime_s(&tm_info, &time_t_val) == 0) {
                    char time_str[100] = {0};
                    if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                        town.timestamp_str = time_str;
                    } else {
                        town.timestamp_str = "Unknown";
                    }
                    
                    town.submitted_at = std::chrono::system_clock::from_time_t(time_t_val);
                } else {
                    town.timestamp_str = "Unknown";
                    town.submitted_at = std::chrono::system_clock::now();
                }
                
                town.status = status_text ? reinterpret_cast<const char*>(status_text) : "pending";
                town.rejection_reason = rejection_reason_text ? reinterpret_cast<const char*>(rejection_reason_text) : "";
                
                found = true;
            }
            
            sqlite3_finalize(stmt);
            
            if (!found) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] No pending town found with ID: %s", id.c_str());
                return std::nullopt;
            }
            
            if (town.status != "pending") {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Town with ID %s is already %s", id.c_str(), town.status.c_str());
                return std::nullopt;
            }
            
            std::string email_to_use = target_email.empty() ? town.email : target_email;
            
            if (!tsto::land::Land::import_town_file(town.file_path, email_to_use)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to import town file: %s", town.file_path.c_str());
                return std::nullopt;
            }
            
            const char* update_query = 
                "UPDATE pending_towns SET status = 'approved' WHERE id = ?;";
            
            stmt = nullptr;
            rc = sqlite3_prepare_v2(db_, update_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_STATIC);
            
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to update town status: %s", sqlite3_errmsg(db_));
                return std::nullopt;
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Approved town with ID: %s, Email: %s", id.c_str(), email_to_use.c_str());
            
            return town;
        }

        std::vector<PendingTownData> PendingTowns::get_pending_towns_by_email(const std::string& email) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            std::vector<PendingTownData> towns;
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return towns;
            }
            
            const char* select_query = 
                "SELECT id, email, town_name, description, file_path, file_size, submitted_at, status, rejection_reason "
                "FROM pending_towns "
                "WHERE email = ? AND status = 'pending' "
                "ORDER BY submitted_at DESC;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, select_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return towns;
            }
            
            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                PendingTownData town;
                
                const unsigned char* id_text = sqlite3_column_text(stmt, 0);
                const unsigned char* email_text = sqlite3_column_text(stmt, 1);
                const unsigned char* town_name_text = sqlite3_column_text(stmt, 2);
                const unsigned char* description_text = sqlite3_column_text(stmt, 3);
                const unsigned char* file_path_text = sqlite3_column_text(stmt, 4);
                const unsigned char* timestamp_text = sqlite3_column_text(stmt, 6);
                const unsigned char* status_text = sqlite3_column_text(stmt, 7);
                const unsigned char* rejection_reason_text = sqlite3_column_text(stmt, 8);
                
                town.id = id_text ? reinterpret_cast<const char*>(id_text) : "";
                town.email = email_text ? reinterpret_cast<const char*>(email_text) : "";
                town.town_name = town_name_text ? reinterpret_cast<const char*>(town_name_text) : "";
                town.description = description_text ? reinterpret_cast<const char*>(description_text) : "";
                town.file_path = file_path_text ? reinterpret_cast<const char*>(file_path_text) : "";
                town.file_size = sqlite3_column_int64(stmt, 5);
                
                sqlite3_int64 timestamp_val = sqlite3_column_int64(stmt, 6);
                
                std::time_t time_t_val = static_cast<std::time_t>(timestamp_val);
                std::tm tm_info = {};
                if (localtime_s(&tm_info, &time_t_val) == 0) {
                    char time_str[100] = {0};
                    if (std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_info) > 0) {
                        town.timestamp_str = time_str;
                    } else {
                        town.timestamp_str = "Unknown";
                    }
                    
                    town.submitted_at = std::chrono::system_clock::from_time_t(time_t_val);
                } else {
                    town.timestamp_str = "Unknown";
                    town.submitted_at = std::chrono::system_clock::now();
                }
                
                town.status = status_text ? reinterpret_cast<const char*>(status_text) : "pending";
                town.rejection_reason = rejection_reason_text ? reinterpret_cast<const char*>(rejection_reason_text) : "";
                
                towns.push_back(town);
            }
            
            sqlite3_finalize(stmt);
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Retrieved %zu pending towns for email: %s", 
                towns.size(), email.c_str());
            
            return towns;
        }

        bool PendingTowns::cleanup_old_towns(int days_to_keep) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            
            if (!db_) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Database not initialized");
                return false;
            }
            
            auto now = std::chrono::system_clock::now();
            auto cutoff_time = now - std::chrono::hours(24 * days_to_keep);
            auto cutoff_timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                cutoff_time.time_since_epoch()).count();
            
            const char* select_query = 
                "SELECT file_path FROM pending_towns "
                "WHERE status IN ('approved', 'rejected') AND submitted_at < ?;";
            
            sqlite3_stmt* stmt = nullptr;
            int rc = sqlite3_prepare_v2(db_, select_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoff_timestamp));
            
            std::vector<std::string> files_to_delete;
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                files_to_delete.push_back(file_path);
            }
            
            sqlite3_finalize(stmt);
            
            for (const auto& file_path : files_to_delete) {
                try {
                    if (std::filesystem::exists(file_path)) {
                        std::filesystem::remove(file_path);
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                            "[PENDING_TOWNS] Deleted old town file: %s", file_path.c_str());
                    }
                } catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                        "[PENDING_TOWNS] Failed to delete old town file: %s - %s", 
                        file_path.c_str(), ex.what());
                }
            }
            
            const char* delete_query = 
                "DELETE FROM pending_towns "
                "WHERE status IN ('approved', 'rejected') AND submitted_at < ?;";
            
            rc = sqlite3_prepare_v2(db_, delete_query, -1, &stmt, nullptr);
            
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to prepare statement: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(cutoff_timestamp));
            
            rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE, 
                    "[PENDING_TOWNS] Failed to delete old town entries: %s", sqlite3_errmsg(db_));
                return false;
            }
            
            int deleted_count = sqlite3_changes(db_);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE, 
                "[PENDING_TOWNS] Cleaned up %d old town entries older than %d days", 
                deleted_count, days_to_keep);
            
            return true;
        }

    }   
}   
