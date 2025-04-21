#include <std_include.hpp>
#include "user_roles.hpp"
#include "debugging/serverlog.hpp"
#include <sqlite3.h>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>

namespace tsto {

    const std::string UserRole::DB_PATH = "auth_roles.db";

    std::string HashPassword(const std::string& password, const std::string& salt = "") {
        std::string useSalt = salt;
        if (useSalt.empty()) {
            const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            std::random_device rd;
            std::mt19937 generator(rd());
            std::uniform_int_distribution<> distribution(0, chars.size() - 1);

            useSalt.reserve(16);
            for (int i = 0; i < 16; ++i) {
                useSalt += chars[distribution(generator)];
            }
        }

        std::string salted = password + useSalt;

        std::stringstream ss;
        ss << useSalt << ":"; 

        unsigned int hash = 0;
        for (char c : salted) {
            hash = hash * 31 + c;
        }
        ss << std::hex << hash;

        return ss.str();
    }

    std::string ExtractSalt(const std::string& storedHash) {
        size_t pos = storedHash.find(':');
        if (pos != std::string::npos) {
            return storedHash.substr(0, pos);
        }
        return "";    
    }

    bool UserRole::Initialize() {
        try {
            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* create_table_sql =
                "CREATE TABLE IF NOT EXISTS users ("
                "username TEXT PRIMARY KEY,"
                "password TEXT NOT NULL,"
                "role TEXT NOT NULL,"
                "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                "updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                ");";

            char* err_msg = nullptr;
            rc = sqlite3_exec(db, create_table_sql, nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "SQL error: %s", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return false;
            }

            const char* create_sessions_table_sql =
                "CREATE TABLE IF NOT EXISTS sessions ("
                "token TEXT PRIMARY KEY,"
                "username TEXT NOT NULL,"
                "ip_address TEXT,"
                "user_agent TEXT,"
                "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                "expires_at TIMESTAMP,"
                "FOREIGN KEY (username) REFERENCES users(username)"
                ");";

            rc = sqlite3_exec(db, create_sessions_table_sql, nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "SQL error: %s", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return false;
            }

            const char* create_town_uploads_table_sql =
                "CREATE TABLE IF NOT EXISTS town_uploads ("
                "username TEXT PRIMARY KEY,"
                "total_uploads INTEGER DEFAULT 0,"
                "last_upload TIMESTAMP DEFAULT CURRENT_TIMESTAMP,"
                "FOREIGN KEY (username) REFERENCES users(username)"
                ");";

            rc = sqlite3_exec(db, create_town_uploads_table_sql, nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "SQL error: %s", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return false;
            }

            const char* count_sql = "SELECT COUNT(*) FROM users;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, count_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            rc = sqlite3_step(stmt);
            int count = 0;
            if (rc == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);

            if (count == 0) {
                const char* insert_admin_sql =
                    "INSERT INTO users (username, password, role) VALUES (?, ?, ?);";

                rc = sqlite3_prepare_v2(db, insert_admin_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to prepare statement: %s", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return false;
                }

                std::string admin_hash = HashPassword("admin");
                sqlite3_bind_text(stmt, 1, "admin", -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, admin_hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, "ADMIN", -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to insert admin user: %s", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return false;
                }
                sqlite3_finalize(stmt);

                rc = sqlite3_prepare_v2(db, insert_admin_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to prepare statement: %s", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return false;
                }

                std::string operator_hash = HashPassword("town123");
                sqlite3_bind_text(stmt, 1, "operator", -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, operator_hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, "TOWN_OPERATOR", -1, SQLITE_STATIC);

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to insert town operator user: %s", sqlite3_errmsg(db));
                    sqlite3_finalize(stmt);
                    sqlite3_close(db);
                    return false;
                }
                sqlite3_finalize(stmt);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "Created default users: admin and operator");
            }

            sqlite3_close(db);
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error initializing user roles database: %s", ex.what());
            return false;
        }
    }

    RoleType UserRole::GetRoleByUsername(const std::string& username) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during role retrieval");
                    return RoleType::UNKNOWN;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return RoleType::UNKNOWN;
            }

            const char* query_sql = "SELECT role FROM users WHERE username = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, query_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return RoleType::UNKNOWN;
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* role_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string role(role_str);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return StringToRoleType(role);
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return RoleType::UNKNOWN;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error getting role for user %s: %s", username.c_str(), ex.what());
            return RoleType::UNKNOWN;
        }
    }

    bool UserRole::AuthenticateUser(const std::string& username, const std::string& password) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during authentication");
                    return false;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* query_sql = "SELECT password FROM users WHERE username = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, query_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                const char* stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                std::string salt = ExtractSalt(stored_hash);
                std::string hash = HashPassword(password, salt);
                bool result = (hash == stored_hash);
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return result;
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error authenticating user %s: %s", username.c_str(), ex.what());
            return false;
        }
    }

    bool UserRole::CreateOrUpdateUser(const std::string& username, const std::string& password, RoleType role) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during user creation/update");
                    return false;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* check_sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, check_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            int count = 0;
            if (rc == SQLITE_ROW) {
                count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);

            std::string hash = HashPassword(password);

            if (count > 0) {
                const char* update_sql =
                    "UPDATE users SET password = ?, role = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?;";

                rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to prepare statement: %s", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return false;
                }

                sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, RoleTypeToString(role).c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, username.c_str(), -1, SQLITE_STATIC);
            }
            else {
                const char* insert_sql =
                    "INSERT INTO users (username, password, role) VALUES (?, ?, ?);";

                rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
                if (rc != SQLITE_OK) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to prepare statement: %s", sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return false;
                }

                sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, hash.c_str(), -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, RoleTypeToString(role).c_str(), -1, SQLITE_STATIC);
            }

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to create/update user: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error creating/updating user %s: %s", username.c_str(), ex.what());
            return false;
        }
    }

    bool UserRole::DeleteUser(const std::string& username) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during user deletion");
                    return false;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* delete_sql = "DELETE FROM users WHERE username = ?;";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, delete_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to delete user: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error deleting user %s: %s", username.c_str(), ex.what());
            return false;
        }
    }

    bool UserRole::ChangePassword(const std::string& username, const std::string& new_password) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during password change");
                    return false;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            std::string hash = HashPassword(new_password);

            const char* update_sql =
                "UPDATE users SET password = ?, updated_at = CURRENT_TIMESTAMP WHERE username = ?;";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, update_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, hash.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to update password: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error changing password for user %s: %s", username.c_str(), ex.what());
            return false;
        }
    }

    std::vector<std::tuple<std::string, RoleType>> UserRole::GetAllUsers() {
        std::vector<std::tuple<std::string, RoleType>> users;

        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during user retrieval");
                    return users;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return users;
            }

            const char* query_sql = "SELECT username, role FROM users ORDER BY username;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, query_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return users;
            }

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                const char* role_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

                RoleType role = StringToRoleType(std::string(role_str));
                users.emplace_back(std::string(username), role);
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error getting all users: %s", ex.what());
        }

        return users;
    }

    std::string UserRole::CreateSession(const std::string& username, const std::string& ip_address, const std::string& user_agent, int expiry_hours) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during session creation");
                    return "";
                }
            }

            const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            std::random_device rd;
            std::mt19937 generator(rd());
            std::uniform_int_distribution<> distribution(0, chars.size() - 1);

            std::string token;
            token.reserve(32);
            for (int i = 0; i < 32; ++i) {
                token += chars[distribution(generator)];
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return "";
            }

            const char* insert_sql =
                "INSERT INTO sessions (token, username, ip_address, user_agent, expires_at) "
                "VALUES (?, ?, ?, ?, datetime('now', '+' || ? || ' hours'));";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return "";
            }

            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, ip_address.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 4, user_agent.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 5, expiry_hours);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to create session: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return "";
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return token;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error creating session for user %s: %s", username.c_str(), ex.what());
            return "";
        }
    }

    bool UserRole::InvalidateSession(const std::string& token) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during session invalidation");
                    return false;
                }
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Invalidating session token: %s", token.c_str());

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* check_sql = "SELECT COUNT(*) FROM sessions WHERE token = ?;";
            sqlite3_stmt* check_stmt = nullptr;
            rc = sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare check statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(check_stmt, 1, token.c_str(), -1, SQLITE_STATIC);
            rc = sqlite3_step(check_stmt);
            int count = 0;
            if (rc == SQLITE_ROW) {
                count = sqlite3_column_int(check_stmt, 0);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "Found %d sessions with token: %s", count, token.c_str());
            }
            sqlite3_finalize(check_stmt);

            if (count == 0) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_INITIALIZER,
                    "No session found with token: %s", token.c_str());
                sqlite3_close(db);
                return false;
            }

            const char* delete_sql = "DELETE FROM sessions WHERE token = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, delete_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare delete statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

           
            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to delete session: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            int rows_affected = sqlite3_changes(db);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Session invalidation affected %d rows", rows_affected);

            const char* verify_sql = "SELECT COUNT(*) FROM sessions WHERE token = ?;";
            sqlite3_stmt* verify_stmt = nullptr;
            rc = sqlite3_prepare_v2(db, verify_sql, -1, &verify_stmt, nullptr);
            if (rc == SQLITE_OK) {
                sqlite3_bind_text(verify_stmt, 1, token.c_str(), -1, SQLITE_STATIC);
                rc = sqlite3_step(verify_stmt);
                if (rc == SQLITE_ROW) {
                    int remaining = sqlite3_column_int(verify_stmt, 0);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                        "After deletion, found %d sessions with token: %s", remaining, token.c_str());
                    if (remaining > 0) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                            "Session token still exists after deletion attempt!");
                    }
                }
                sqlite3_finalize(verify_stmt);
            }

            sqlite3_finalize(stmt);
            
            const char* pragma_sql = "PRAGMA wal_checkpoint;";
            sqlite3_exec(db, pragma_sql, nullptr, nullptr, nullptr);
            
            sqlite3_close(db);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Session successfully invalidated");
            
            return rows_affected > 0;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error invalidating session: %s", ex.what());
            return false;
        }
    }

    bool UserRole::ValidateSession(const std::string& token, std::string& username) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during session validation");
                    return false;
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                "Validating session token: %s", token.c_str());

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* query_sql =
                "SELECT username FROM sessions "
                "WHERE token = ? AND expires_at > datetime('now');";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, query_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, token.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                    "Session token valid for user: %s", username.c_str());
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return true;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                "Session token invalid or expired: %s", token.c_str());
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error validating session: %s", ex.what());
            return false;
        }
    }

    void UserRole::CleanupExpiredSessions() {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during session cleanup");
                    return;
                }
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return;
            }

            const char* delete_sql = "DELETE FROM sessions WHERE expires_at <= datetime('now');";

            char* err_msg = nullptr;
            rc = sqlite3_exec(db, delete_sql, nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to cleanup expired sessions: %s", err_msg);
                sqlite3_free(err_msg);
            }

            sqlite3_close(db);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error cleaning up expired sessions: %s", ex.what());
        }
    }

    bool UserRole::RenameUser(const std::string& old_username, const std::string& new_username) {
        try {
            static bool initialized = false;
            if (!initialized) {
                initialized = Initialize();
                if (!initialized) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                        "Failed to initialize user roles database during user rename");
                    return false;
                }
            }

            if (new_username.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot rename user to empty username");
                return false;
            }

            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            const char* check_old_sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, check_old_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, old_username.c_str(), -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            int old_count = 0;
            if (rc == SQLITE_ROW) {
                old_count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);

            if (old_count == 0) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot rename user: original username '%s' not found", old_username.c_str());
                sqlite3_close(db);
                return false;
            }

            const char* check_new_sql = "SELECT COUNT(*) FROM users WHERE username = ?;";
            rc = sqlite3_prepare_v2(db, check_new_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_username.c_str(), -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);
            int new_count = 0;
            if (rc == SQLITE_ROW) {
                new_count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);

            if (new_count > 0) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Cannot rename user: new username '%s' already exists", new_username.c_str());
                sqlite3_close(db);
                return false;
            }

            const char* get_user_sql = "SELECT password, role FROM users WHERE username = ?;";
            rc = sqlite3_prepare_v2(db, get_user_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, old_username.c_str(), -1, SQLITE_STATIC);
            rc = sqlite3_step(stmt);

            std::string password;
            std::string role;

            if (rc == SQLITE_ROW) {
                password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "Retrieved user data for '%s': role=%s", old_username.c_str(), role.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to get user data: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);

            const char* insert_sql = "INSERT INTO users (username, password, role) VALUES (?, ?, ?);";
            rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, password.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 3, role.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to insert new user: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Successfully created new user '%s'", new_username.c_str());

            const char* update_sessions_sql = "UPDATE sessions SET username = ? WHERE username = ?;";
            rc = sqlite3_prepare_v2(db, update_sessions_sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, new_username.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, old_username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to update sessions: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            int sessions_updated = sqlite3_changes(db);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Updated %d sessions from '%s' to '%s'",
                sessions_updated, old_username.c_str(), new_username.c_str());

            sqlite3_finalize(stmt);

            std::string delete_sql_str = "DELETE FROM users WHERE username = '" + old_username + "';";
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Executing direct SQL: %s", delete_sql_str.c_str());

            char* err_msg = nullptr;
            rc = sqlite3_exec(db, delete_sql_str.c_str(), nullptr, nullptr, &err_msg);

            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to delete old user with direct SQL: %s", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return false;
            }

            int rows_deleted = sqlite3_changes(db);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Direct SQL delete affected %d rows for user '%s'",
                rows_deleted, old_username.c_str());

            if (rows_deleted == 0) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to delete old user: no rows affected");
                sqlite3_close(db);
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Successfully renamed user from '%s' to '%s'", old_username.c_str(), new_username.c_str());

            sqlite3_close(db);
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Error renaming user from '%s' to '%s': %s",
                old_username.c_str(), new_username.c_str(), ex.what());
            return false;
        }
    }

    std::string UserRole::RoleTypeToString(RoleType role) {
        switch (role) {
        case RoleType::ADMIN:
            return "ADMIN";
        case RoleType::TOWN_OPERATOR:
            return "TOWN_OPERATOR";
        case RoleType::GAME_MODERATOR:
            return "GAME_MODERATOR";
        case RoleType::CONTENT_MANAGER:
            return "CONTENT_MANAGER";
        case RoleType::VIEWER:
            return "VIEWER";
        default:
            return "UNKNOWN";
        }
    }

    RoleType UserRole::StringToRoleType(const std::string& role_str) {
        std::string role_upper = role_str;
        std::transform(role_upper.begin(), role_upper.end(), role_upper.begin(), ::toupper);

        if (role_upper == "ADMIN") return RoleType::ADMIN;
        if (role_upper == "TOWN_OPERATOR") return RoleType::TOWN_OPERATOR;
        if (role_upper == "GAME_MODERATOR") return RoleType::GAME_MODERATOR;
        if (role_upper == "CONTENT_MANAGER") return RoleType::CONTENT_MANAGER;
        if (role_upper == "VIEWER") return RoleType::VIEWER;

        return RoleType::UNKNOWN;
    }

    bool UserRole::RecordTownUpload(const std::string& username) {
        try {
            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            std::string check_user_sql = "SELECT username FROM users WHERE username = ?;";
            sqlite3_stmt* check_stmt = nullptr;

            rc = sqlite3_prepare_v2(db, check_user_sql.c_str(), -1, &check_stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(check_stmt, 1, username.c_str(), -1, SQLITE_STATIC);

            bool user_exists = false;
            if (sqlite3_step(check_stmt) == SQLITE_ROW) {
                user_exists = true;
            }

            sqlite3_finalize(check_stmt);

            if (!user_exists) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] User does not exist: %s", username.c_str());
                sqlite3_close(db);
                return false;
            }

            std::string upsert_sql =
                "INSERT INTO town_uploads (username, total_uploads, last_upload) "
                "VALUES (?, 1, CURRENT_TIMESTAMP) "
                "ON CONFLICT(username) DO UPDATE SET "
                "total_uploads = total_uploads + 1, "
                "last_upload = CURRENT_TIMESTAMP;";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, upsert_sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Failed to record town upload: %s", sqlite3_errmsg(db));
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                return false;
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Successfully recorded town upload for user: %s", username.c_str());

            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Exception: %s", ex.what());
            return false;
        }
    }

    std::vector<std::tuple<std::string, int, std::string>> UserRole::GetTownUploadStats() {
        std::vector<std::tuple<std::string, int, std::string>> stats;

        try {
            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return stats;
            }

            std::string query_sql =
                "SELECT u.username, COALESCE(t.total_uploads, 0) as total_uploads, "
                "COALESCE(datetime(t.last_upload), 'Never') as last_upload "
                "FROM users u "
                "LEFT JOIN town_uploads t ON u.username = t.username "
                "ORDER BY total_uploads DESC, u.username ASC;";

            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, query_sql.c_str(), -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Failed to prepare statement: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return stats;
            }

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                std::string username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
                int total_uploads = sqlite3_column_int(stmt, 1);

                std::string last_upload;
                if (sqlite3_column_type(stmt, 2) != SQLITE_NULL) {
                    last_upload = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                }
                else {
                    last_upload = "Never";
                }

                stats.push_back(std::make_tuple(username, total_uploads, last_upload));
            }

            sqlite3_finalize(stmt);
            sqlite3_close(db);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Retrieved %d town upload statistics records", stats.size());

            return stats;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Exception: %s", ex.what());
            return stats;
        }
    }

    bool UserRole::ClearTownUploadStats() {
        try {
            sqlite3* db = nullptr;
            int rc = sqlite3_open(DB_PATH.c_str(), &db);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Cannot open database: %s", sqlite3_errmsg(db));
                sqlite3_close(db);
                return false;
            }

            std::string delete_sql = "DELETE FROM town_uploads;";

            char* err_msg = nullptr;
            rc = sqlite3_exec(db, delete_sql.c_str(), nullptr, nullptr, &err_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN UPLOAD] Failed to clear town upload statistics: %s", err_msg);
                sqlite3_free(err_msg);
                sqlite3_close(db);
                return false;
            }

            sqlite3_close(db);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Successfully cleared all town upload statistics");

            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[TOWN UPLOAD] Exception: %s", ex.what());
            return false;
        }
    }

}
