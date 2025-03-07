#include <std_include.hpp>
#include "database.hpp"
#include "debugging/serverlog.hpp"
#include <sqlite3.h>
#include <string>
#include <mutex>

namespace tsto {
namespace database {

namespace {
    // SQLite memory configuration
    constexpr int SQLITE_MEMORY_LIMIT = 2 * 1024 * 1024; // 2MB total memory limit
    constexpr int SQLITE_PAGE_SIZE = 4096;               // 4KB page size
    constexpr int SQLITE_CACHE_PAGES = 400;              // ~1.6MB cache
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
    const char* create_users_table = 
        "CREATE TABLE IF NOT EXISTS users ("
        "email TEXT PRIMARY KEY COLLATE NOCASE,"  // case-insensitive email
        "user_id TEXT NOT NULL,"                  // TEXT to handle large numbers
        "access_token TEXT NOT NULL,"             // access token
        "mayhem_id INTEGER,"                      // mayhem ID (muid)
        "access_code TEXT"                        // access code for auth flow
        ");";

    char* error_msg = nullptr;
    int rc = sqlite3_exec(db_, create_users_table, nullptr, nullptr, &error_msg);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to create users table: %s (code: %d)", 
            error_msg ? error_msg : "unknown error", rc);
        sqlite3_free(error_msg);
        return false;
    }


    const char* create_indices[] = {
        "CREATE INDEX IF NOT EXISTS idx_user_id ON users(user_id);",
        "CREATE INDEX IF NOT EXISTS idx_access_token ON users(access_token);",
        "CREATE INDEX IF NOT EXISTS idx_mayhem_id ON users(mayhem_id);",
        "CREATE INDEX IF NOT EXISTS idx_access_code ON users(access_code);"
    };

    for (const auto& create_index : create_indices) {
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

int64_t Database::get_next_mayhem_id() {
    if (!db_) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Cannot get next mayhem ID - database not initialized");
        return 47000; //starting MID
    }

    const char* sql = "SELECT MAX(mayhem_id) FROM users;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to prepare statement: %s (code: %d)", 
            sqlite3_errmsg(db_), rc);
        return 47000;
    }

    int64_t next_id = 47000; //starting MID
    
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            next_id = sqlite3_column_int64(stmt, 0) + 1;
        }
    }
    
    sqlite3_finalize(stmt);
    return next_id;
}

bool Database::store_user_id(const std::string& email, const std::string& user_id, 
                            const std::string& access_token, int64_t mayhem_id, 
                            const std::string& access_code) {
    if (!db_) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Cannot store user data - database not initialized");
        return false;
    }

    //if mayhem_id is 0, generate a new one
    if (mayhem_id == 0) {
        mayhem_id = get_next_mayhem_id();
    }

    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
        "Storing user data - Email: %s, User ID: %s, Access Token: %s, Mayhem ID: %lld, Access Code: %s", 
        email.c_str(), user_id.c_str(), access_token.c_str(), mayhem_id, access_code.c_str());

    const char* sql = "INSERT OR REPLACE INTO users (email, user_id, access_token, mayhem_id, access_code) VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to prepare statement: %s (code: %d)", 
            sqlite3_errmsg(db_), rc);
        return false;
    }

    sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, user_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, access_token.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, mayhem_id);
    sqlite3_bind_text(stmt, 5, access_code.c_str(), -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to store user data: %s (code: %d)", sqlite3_errmsg(db_), rc);
    } else {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Successfully stored user data for email: %s", email.c_str());
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
    } else {
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
    } else {
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
    } else if (rc != SQLITE_DONE) {
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
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Found access token for email %s", email.c_str());
        }
    } else if (rc != SQLITE_DONE) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to query access token: %s (code: %d)", sqlite3_errmsg(db_), rc);
    }

    sqlite3_finalize(stmt);
    return found;
}

bool Database::update_access_token(const std::string& email, const std::string& access_token) {
    if (!db_) return false;

    const char* sql = "UPDATE users SET access_token = ? WHERE email = ? COLLATE NOCASE;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to prepare statement: %s (code: %d)", 
            sqlite3_errmsg(db_), rc);
        return false;
    }

    rc = sqlite3_bind_text(stmt, 1, access_token.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to bind access token: %s (code: %d)", sqlite3_errmsg(db_), rc);
        sqlite3_finalize(stmt);
        return false;
    }

    rc = sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to bind email: %s (code: %d)", sqlite3_errmsg(db_), rc);
        sqlite3_finalize(stmt);
        return false;
    }

    rc = sqlite3_step(stmt);
    bool success = (rc == SQLITE_DONE);
    
    if (!success) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to update access token: %s (code: %d)", sqlite3_errmsg(db_), rc);
    } else {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Successfully updated access token for email: %s", email.c_str());
    }

    sqlite3_finalize(stmt);
    return success;
}

bool Database::get_email_by_token(const std::string& access_token, std::string& email) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT email FROM users WHERE access_token = ? COLLATE NOCASE";
    
    if (!prepare_and_bind(&stmt, query)) {
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 1, access_token.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        sqlite3_finalize(stmt);
        return false;
    }
    
    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (result) {
            email = result;
            found = true;
        }
    }
    
    sqlite3_finalize(stmt);
    return found;
}

bool Database::validate_access_token(const std::string& access_token, std::string& email) {
    if (access_token.empty()) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Cannot validate empty access token");
        return false;
    }
    
    std::string token_prefix = access_token.substr(0, 10);
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
        "Validating access token: %s...", token_prefix.c_str());
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    sqlite3_stmt* stmt = nullptr;
    const char* query = "SELECT email FROM users WHERE access_token = ? COLLATE NOCASE";
    
    if (sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr) != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to prepare statement for token validation: %s", sqlite3_errmsg(db_));
        return false;
    }
    
    if (sqlite3_bind_text(stmt, 1, access_token.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to bind access token for validation: %s", sqlite3_errmsg(db_));
        sqlite3_finalize(stmt);
        return false;
    }
    
    bool found = false;
    int result = sqlite3_step(stmt);
    
    if (result == SQLITE_ROW) {
        const char* email_result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (email_result) {
            email = email_result;
            found = true;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Access token is valid for email: %s", email.c_str());
        } else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Access token found but email is null");
        }
    } else if (result == SQLITE_DONE) {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Access token not found with direct comparison, trying case-insensitive lookup");
            
        sqlite3_finalize(stmt);
        
        const char* fallback_query = "SELECT email FROM users WHERE LOWER(access_token) = LOWER(?)";
        if (sqlite3_prepare_v2(db_, fallback_query, -1, &stmt, nullptr) != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare fallback statement for token validation: %s", sqlite3_errmsg(db_));
            return false;
        }
        
        if (sqlite3_bind_text(stmt, 1, access_token.c_str(), -1, SQLITE_STATIC) != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to bind access token for fallback validation: %s", sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            return false;
        }
        
        result = sqlite3_step(stmt);
        if (result == SQLITE_ROW) {
            const char* email_result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (email_result) {
                email = email_result;
                found = true;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "Access token is valid (case-insensitive) for email: %s", email.c_str());
            }
        } else {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Access token not found in database (tried both case-sensitive and insensitive): %s...", 
                token_prefix.c_str());
        }
    } else {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Error while validating access token: %s", sqlite3_errmsg(db_));
    }
    
    sqlite3_finalize(stmt);
    return found;
}

bool Database::get_mayhem_id(const std::string& email, int64_t& mayhem_id) {
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
        mayhem_id = sqlite3_column_int64(stmt, 0);
        found = true;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Found mayhem ID for email %s: %lld", email.c_str(), mayhem_id);
    } else if (rc != SQLITE_DONE) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "Failed to query mayhem ID: %s (code: %d)", sqlite3_errmsg(db_), rc);
    }

    sqlite3_finalize(stmt);
    return found;
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

} 
} 
