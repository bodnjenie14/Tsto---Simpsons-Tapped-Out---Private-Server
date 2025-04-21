#include <std_include.hpp>
#include "public_users_db.hpp"
#include "debugging/serverlog.hpp"
#include "database.hpp"       
#include <sqlite3.h>
#include <string>
#include <mutex>
#include <vector>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <regex>
#include "cryptography.hpp"

namespace tsto::database {

    namespace {
        constexpr int SQLITE_MEMORY_LIMIT = 2 * 1024 * 1024;     
        constexpr int SQLITE_PAGE_SIZE = 4096;                  
        constexpr int SQLITE_CACHE_PAGES = 400;                
    }

    PublicUsersDB& PublicUsersDB::get_instance() {
        static PublicUsersDB instance;
        return instance;
    }

    PublicUsersDB::PublicUsersDB() : db_(nullptr) {
        int rc = sqlite3_initialize();
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to initialize SQLite for public users DB: %d", rc);
            return;
        }

        sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 1);
        sqlite3_config(SQLITE_CONFIG_HEAP, SQLITE_MEMORY_LIMIT, 64, 8);
        sqlite3_soft_heap_limit(SQLITE_MEMORY_LIMIT);

        if (!initialize()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to initialize public users database during construction");
        }
    }

    PublicUsersDB::~PublicUsersDB() {
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }
        sqlite3_shutdown();
    }

    bool PublicUsersDB::initialize() {
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Initializing public users database...");

        int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        int rc = sqlite3_open_v2(DB_FILE, &db_, flags, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to open public users database: %s (code: %d)",
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
            "PRAGMA journal_mode=WAL",
            "PRAGMA temp_store=MEMORY",
            "PRAGMA synchronous=NORMAL",
            "PRAGMA foreign_keys=ON"
        };

        for (const auto& pragma : pragmas) {
            char* error_msg = nullptr;
            rc = sqlite3_exec(db_, pragma, nullptr, nullptr, &error_msg);
            if (rc != SQLITE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to set pragma for public users DB: %s - %s (code: %d)",
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
                "Failed to create public users database tables");
            sqlite3_close_v2(db_);
            db_ = nullptr;
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Public users database initialized successfully");
        return true;
    }

    void PublicUsersDB::close() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (db_) {
            sqlite3_close_v2(db_);
            db_ = nullptr;
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Public users database closed");
        }
    }

    bool PublicUsersDB::create_tables() {
        const char* create_users_table = R"(
            CREATE TABLE IF NOT EXISTS users (
                email TEXT PRIMARY KEY,
                password_hash TEXT NOT NULL,
                password_salt TEXT NOT NULL,
                display_name TEXT,
                registration_date INTEGER,
                last_login INTEGER,
                verification_code TEXT,
                verification_code_expiry INTEGER,
                is_verified INTEGER DEFAULT 0,
                failed_login_attempts INTEGER DEFAULT 0,
                last_failed_login INTEGER DEFAULT 0,
                account_locked INTEGER DEFAULT 0,
                account_locked_until INTEGER DEFAULT 0,
                reset_token TEXT,
                reset_token_expiry INTEGER,
                tsto_email TEXT,
                user_cred TEXT
            )
        )";

        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, create_users_table, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to create public users table: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        const char* create_tsto_email_index = R"(
            CREATE INDEX IF NOT EXISTS idx_tsto_email ON users(tsto_email);
        )";

        rc = sqlite3_exec(db_, create_tsto_email_index, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to create tsto_email index: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        const char* create_verification_codes_table = R"(
            CREATE TABLE IF NOT EXISTS verification_codes (
                email TEXT PRIMARY KEY,
                code TEXT NOT NULL,
                expiry INTEGER NOT NULL,
                FOREIGN KEY (email) REFERENCES users(email) ON DELETE CASCADE
            )
        )";

        rc = sqlite3_exec(db_, create_verification_codes_table, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to create verification_codes table: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Public users tables created or already exist");
        return true;
    }

    std::string PublicUsersDB::generate_salt() {
        const size_t SALT_LENGTH = 32;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);

        std::vector<unsigned char> salt(SALT_LENGTH);
        for (size_t i = 0; i < SALT_LENGTH; ++i) {
            salt[i] = static_cast<unsigned char>(dis(gen));
        }

        std::string raw_salt(salt.begin(), salt.end());
        std::string encoded_salt = utils::cryptography::base64::encode(raw_salt);
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Generated salt - Raw length: %zu, Encoded length: %zu", 
            raw_salt.length(), encoded_salt.length());
            
        if (encoded_salt.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to generate valid salt: encoded salt is empty");
            return "DefaultSalt123456789"; 
        }
        
        return encoded_salt;
    }

    std::string PublicUsersDB::hash_password(const std::string& password, const std::string& salt) {
        if (password.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot hash an empty password");
            throw std::invalid_argument("Password cannot be empty");
        }

        if (salt.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot hash with an empty salt");
            throw std::invalid_argument("Salt cannot be empty");
        }

        std::string combined = password + salt;

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Hashing password - Password length: %zu, Salt: %s, Combined length: %zu",
            password.length(), salt.c_str(), combined.length());

        try {
            std::string binary_hash = utils::cryptography::sha256::compute(combined);
            
            if (binary_hash.empty() || binary_hash.length() != 32) {     
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to compute SHA-256 hash, length: %zu", binary_hash.length());
                throw std::runtime_error("SHA-256 hash computation failed");
            }
            
            std::string hex_hash = utils::cryptography::sha256::compute(combined, true);
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Generated hash - Binary length: %zu, Hex length: %zu",
                binary_hash.length(), hex_hash.length());
                
            if (hex_hash.empty() || hex_hash.length() != 64) {      
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to encode password hash to hex, length: %zu", hex_hash.length());
                throw std::runtime_error("Hex encoding failed");
            }

            return hex_hash;
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Exception during password hashing: %s", e.what());
            throw;    
        }
    }

    std::string PublicUsersDB::generate_random_code() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);

        return std::to_string(dis(gen));
    }
    
    bool PublicUsersDB::update_password_hash(const std::string& email, const std::string& hash, const std::string& salt) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot update password hash: database not initialized");
            return false;
        }
        
        const char* update_query = R"(
            UPDATE users SET 
                password_hash = ?, 
                password_salt = ?
            WHERE email = ? COLLATE NOCASE;
        )";
        
        sqlite3_stmt* update_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }
        
        sqlite3_bind_text(update_stmt, 1, hash.c_str(), hash.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt, 2, salt.c_str(), salt.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt, 3, email.c_str(), email.length(), SQLITE_TRANSIENT);
        
        rc = sqlite3_step(update_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update password hash: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password hash update affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "Password hash update statement succeeded but no rows were affected for email: %s", 
                email.c_str());
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        sqlite3_finalize(update_stmt);
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Successfully migrated password hash for user: %s", email.c_str());
        return true;
    }
    
    void PublicUsersDB::migrate_password_hash_async(const std::string& email, const std::string& hash, const std::string& salt) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Starting async password hash migration for user: %s", email.c_str());
                
            PublicUsersDB& db = PublicUsersDB::get_instance();
            
            if (db.update_password_hash(email, hash, salt)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Async password hash migration successful for user: %s", email.c_str());
            } else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Async password hash migration failed for user: %s", email.c_str());
            }
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Exception during async password hash migration: %s", e.what());
        }
    }

    bool PublicUsersDB::register_user(const std::string& email, const std::string& password, const std::string& display_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot register public user: database not initialized");
            return false;
        }

        std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
        if (!std::regex_match(email, email_regex)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid email format: %s", email.c_str());
            return false;
        }

        if (password.length() < 8) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Password too short for user: %s", email.c_str());
            return false;
        }

        const char* check_query = "SELECT email FROM users WHERE email = ? COLLATE NOCASE;";
        sqlite3_stmt* check_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, check_query, -1, &check_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(check_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(check_stmt);

        if (rc == SQLITE_ROW) {
            sqlite3_finalize(check_stmt);
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "User already exists: %s", email.c_str());
            return false;
        }

        sqlite3_finalize(check_stmt);

        std::string salt = generate_salt();
        std::string password_hash = hash_password(password, salt);
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "User registration - Generated salt: '%s' (length: %zu)", salt.c_str(), salt.length());
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "User registration - Generated hash: '%s' (length: %zu)", password_hash.c_str(), password_hash.length());
            
        if (salt.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to generate salt for user: %s", email.c_str());
            return false;
        }

        int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        std::string verification_code = generate_random_code();

        int64_t verification_expiry = current_time + (VERIFICATION_CODE_EXPIRY_HOURS * 3600);

        const char* insert_query = R"(
            INSERT INTO users 
            (email, password_hash, password_salt, display_name, registration_date, verification_code, verification_code_expiry)
            VALUES (?, ?, ?, ?, ?, ?, ?);
        )";

        sqlite3_stmt* insert_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, insert_query, -1, &insert_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(insert_stmt, 1, email.c_str(), email.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(insert_stmt, 2, password_hash.c_str(), password_hash.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(insert_stmt, 3, salt.c_str(), salt.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(insert_stmt, 4, display_name.c_str(), display_name.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(insert_stmt, 5, current_time);
        sqlite3_bind_text(insert_stmt, 6, verification_code.c_str(), verification_code.length(), SQLITE_TRANSIENT);
        sqlite3_bind_int64(insert_stmt, 7, verification_expiry);
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Executing user registration - Email: '%s', Hash length: %zu, Salt length: %zu",
            email.c_str(), password_hash.length(), salt.length());

        rc = sqlite3_step(insert_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to register public user: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(insert_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "User registration affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "User registration statement succeeded but no rows were affected for email: %s", 
                email.c_str());
        }
        
        sqlite3_finalize(insert_stmt);
        
        const char* verify_query = R"(
            SELECT password_hash, password_salt FROM users
            WHERE email = ? COLLATE NOCASE;
        )";
        
        sqlite3_stmt* verify_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, verify_query, -1, &verify_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare verification statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return true;        
        }
        
        sqlite3_bind_text(verify_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(verify_stmt);
        
        if (rc == SQLITE_ROW) {
            std::string stored_hash = "";
            std::string stored_salt = "";
            
            if (sqlite3_column_text(verify_stmt, 0) != nullptr) {
                stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(verify_stmt, 0));
            }
            
            if (sqlite3_column_text(verify_stmt, 1) != nullptr) {
                stored_salt = reinterpret_cast<const char*>(sqlite3_column_text(verify_stmt, 1));
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Verification - Stored hash: '%s', Stored salt: '%s'",
                stored_hash.c_str(), stored_salt.c_str());
                
            if (stored_hash != password_hash || stored_salt != salt) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "User registration verification failed - hash or salt mismatch for user: %s",
                    email.c_str());
            }
        } else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to verify user registration: user not found after insert");
        }
        
        sqlite3_finalize(verify_stmt);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Registered new public user: %s with verification code: %s",
            email.c_str(), verification_code.c_str());
        return true;
    }

    bool PublicUsersDB::authenticate_user(const std::string& email, const std::string& password) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot authenticate public user: database not initialized");
            return false;
        }

        int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        const char* query = R"(
            SELECT password_hash, password_salt, is_verified, failed_login_attempts, account_locked, account_locked_until 
            FROM users 
            WHERE email = ? COLLATE NOCASE;
        )";

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

        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "User not found: %s", email.c_str());
            return false;
        }

        std::string stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        std::string salt = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        int is_verified = sqlite3_column_int(stmt, 2);
        int failed_attempts = sqlite3_column_int(stmt, 3);
        int account_locked = sqlite3_column_int(stmt, 4);
        int64_t locked_until = sqlite3_column_int64(stmt, 5);
        sqlite3_finalize(stmt);

        if (account_locked && current_time < locked_until) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Account locked for user: %s until %lld (current time: %lld)",
                email.c_str(), locked_until, current_time);
            return false;
        }

        if (!is_verified) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Email not verified for user: %s", email.c_str());
            return false;
        }

        std::string computed_hash;
        try {
            computed_hash = hash_password(password, salt);
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Error hashing password during authentication for user %s: %s", 
                email.c_str(), e.what());
            return false;
        }

        bool is_old_hash_format = (stored_hash.length() < 64 && computed_hash != stored_hash);
        
        if (is_old_hash_format) {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Detected possible old-style hash for user: %s, attempting legacy authentication", email.c_str());
                
            std::string combined = password + salt;
            
            std::string binary_hash = utils::cryptography::sha256::compute(combined, false);
            std::string old_style_hash = utils::cryptography::base64::encode(binary_hash);
            
            if (old_style_hash != stored_hash) {
                update_failed_login_attempts(email, failed_attempts + 1, current_time);

                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Invalid password for user: %s (attempt %d) - legacy auth failed", 
                    email.c_str(), failed_attempts + 1);
                return false;
            }
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Migrating user %s from old hash format to new hash format", email.c_str());
                
            std::string email_to_update = email;
            std::string new_hash = computed_hash;
            std::string current_salt = salt;
            
        } else if (computed_hash != stored_hash) {
            update_failed_login_attempts(email, failed_attempts + 1, current_time);

            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid password for user: %s (attempt %d)", email.c_str(), failed_attempts + 1);
            return false;
        }

        const char* update_query = R"(
            UPDATE users 
            SET failed_login_attempts = 0, last_login = ?, account_locked = 0, account_locked_until = 0
            WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* update_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_int64(update_stmt, 1, current_time);
        sqlite3_bind_text(update_stmt, 2, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(update_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update password: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password update affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "Password update statement succeeded but no rows were affected for email: %s", 
                email.c_str());
        }
        
        sqlite3_finalize(update_stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update login info for public user: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Authenticated public user: %s", email.c_str());
            
        if (is_old_hash_format) {
            std::thread migration_thread([email, computed_hash, salt]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                migrate_password_hash_async(email, computed_hash, salt);
            });
            
            migration_thread.detach();
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Scheduled async password hash migration for user: %s", email.c_str());
        }
        
        return true;
    }

    bool PublicUsersDB::update_failed_login_attempts(const std::string& email, int attempts, int64_t timestamp) {
        bool should_lock = attempts >= MAX_FAILED_ATTEMPTS;
        int64_t locked_until = 0;

        if (should_lock) {
            locked_until = timestamp + (ACCOUNT_LOCK_MINUTES * 60);
        }

        const char* update_query = R"(
            UPDATE users 
            SET failed_login_attempts = ?, last_failed_login = ?, account_locked = ?, account_locked_until = ?
            WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* update_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_int(update_stmt, 1, attempts);
        sqlite3_bind_int64(update_stmt, 2, timestamp);
        sqlite3_bind_int(update_stmt, 3, should_lock ? 1 : 0);
        sqlite3_bind_int64(update_stmt, 4, locked_until);
        sqlite3_bind_text(update_stmt, 5, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(update_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update password: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password update affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "Password update statement succeeded but no rows were affected for email: %s", 
                email.c_str());
        }
        
        sqlite3_finalize(update_stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update failed login attempts for user: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        if (should_lock) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                "Locked account for user: %s until %lld due to too many failed login attempts",
                email.c_str(), locked_until);
        }

        return true;
    }

    bool PublicUsersDB::get_user_info(const std::string& email, std::string& display_name, bool& is_verified,
        int64_t& registration_date, int64_t& last_login) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Database not initialized");
            return false;
        }

        const char* query = R"(
            SELECT display_name, is_verified, registration_date, last_login
            FROM users
            WHERE email = ? COLLATE NOCASE;
        )";

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
            const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            display_name = name ? name : "";

            is_verified = sqlite3_column_int(stmt, 1) == 1;

            registration_date = sqlite3_column_int64(stmt, 2);
            last_login = sqlite3_column_int64(stmt, 3);

            sqlite3_finalize(stmt);
            return true;
        }

        sqlite3_finalize(stmt);
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
            "User not found: %s", email.c_str());
        return false;
    }

    bool PublicUsersDB::store_access_token(const std::string& email, const std::string& token, int64_t expiry) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot store access token: database not initialized");
            return false;
        }

        const char* create_tokens_table = R"(
            CREATE TABLE IF NOT EXISTS user_tokens (
                email TEXT PRIMARY KEY,
                token TEXT NOT NULL,
                expiry INTEGER NOT NULL,
                FOREIGN KEY (email) REFERENCES users(email)
            )
        )";

        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, create_tokens_table, nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to create tokens table: %s (code: %d)",
                error_msg ? error_msg : "unknown error", rc);
            sqlite3_free(error_msg);
            return false;
        }

        const char* query = R"(
            INSERT OR REPLACE INTO user_tokens (email, token, expiry)
            VALUES (?, ?, ?);
        )";

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, token.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int64(stmt, 3, expiry);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to store access token: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Stored access token for user: %s", email.c_str());
        return true;
    }

    bool PublicUsersDB::validate_access_token(const std::string& email, const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot validate access token: database not initialized");
            return false;
        }

        int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        const char* query = R"(
            SELECT token, expiry FROM user_tokens
            WHERE email = ? COLLATE NOCASE;
        )";

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

        if (rc != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "No token found for user: %s", email.c_str());
            return false;
        }

        std::string stored_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        int64_t expiry = sqlite3_column_int64(stmt, 1);
        sqlite3_finalize(stmt);

        if (current_time > expiry) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Token expired for user: %s (expired at %lld, current time: %lld)",
                email.c_str(), expiry, current_time);
            return false;
        }

        if (token != stored_token) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid token for user: %s", email.c_str());
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Validated access token for user: %s", email.c_str());
        return true;
    }

    bool PublicUsersDB::user_exists(const std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot check if user exists: database not initialized");
            return false;
        }

        const char* query = R"(
            SELECT COUNT(*) FROM users
            WHERE email = ? COLLATE NOCASE;
        )";

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

        bool exists = false;
        if (rc == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }

        sqlite3_finalize(stmt);
        return exists;
    }

    bool PublicUsersDB::reset_password(const std::string& email, const std::string& code, const std::string& new_password) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot reset password: database not initialized");
            return false;
        }

        const char* check_query = R"(
            SELECT user_cred FROM users
            WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* check_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, check_query, -1, &check_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(check_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(check_stmt);

        bool verification_success = false;

        if (rc == SQLITE_ROW && sqlite3_column_text(check_stmt, 0) != nullptr) {
            std::string stored_code = reinterpret_cast<const char*>(sqlite3_column_text(check_stmt, 0));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Password reset attempt - Stored code: '%s', Entered code: '%s'", stored_code.c_str(), code.c_str());

            if (stored_code == code) {
                verification_success = true;
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "No verification code found for user: %s", email.c_str());
        }

        sqlite3_finalize(check_stmt);

        if (!verification_success) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid or expired verification code for user: %s", email.c_str());
            return false;
        }

        std::string salt = generate_salt();
        
        if (salt.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to generate salt for password reset: %s", email.c_str());
            return false;
        }
        
        std::string hashed_password;
        try {
            hashed_password = hash_password(new_password, salt);
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Error hashing password during reset for user %s: %s", 
                email.c_str(), e.what());
            return false;
        }
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password reset - Generated new salt: '%s' (length: %zu)", salt.c_str(), salt.length());
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password reset - Generated new hash: '%s' (length: %zu)", hashed_password.c_str(), hashed_password.length());

        const char* update_query = R"(
            UPDATE users SET 
                password_hash = ?, 
                password_salt = ?, 
                user_cred = NULL,
                failed_login_attempts = 0,
                account_locked = 0,
                account_locked_until = 0
            WHERE email = ? COLLATE NOCASE;
        )";
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Reset password SQL query: %s", update_query);

        sqlite3_stmt* update_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(update_stmt, 1, hashed_password.c_str(), hashed_password.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt, 2, salt.c_str(), salt.length(), SQLITE_TRANSIENT);
        sqlite3_bind_text(update_stmt, 3, email.c_str(), email.length(), SQLITE_TRANSIENT);
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Executing password update - Email: '%s', Hash length: %zu, Salt length: %zu",
            email.c_str(), hashed_password.length(), salt.length());

        rc = sqlite3_step(update_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update password: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password update affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "Password update statement succeeded but no rows were affected for email: %s", 
                email.c_str());
        }
        
        sqlite3_finalize(update_stmt);

        const char* verify_query = R"(
            SELECT password_hash, password_salt FROM users
            WHERE email = ? COLLATE NOCASE;
        )";
        
        sqlite3_stmt* verify_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, verify_query, -1, &verify_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare verification statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }
        
        sqlite3_bind_text(verify_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(verify_stmt);
        
        if (rc == SQLITE_ROW) {
            std::string stored_hash = "";
            std::string stored_salt = "";
            
            if (sqlite3_column_text(verify_stmt, 0) != nullptr) {
                stored_hash = reinterpret_cast<const char*>(sqlite3_column_text(verify_stmt, 0));
            }
            
            if (sqlite3_column_text(verify_stmt, 1) != nullptr) {
                stored_salt = reinterpret_cast<const char*>(sqlite3_column_text(verify_stmt, 1));
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Verification - Stored hash: '%s', Stored salt: '%s'",
                stored_hash.c_str(), stored_salt.c_str());
                
            if (stored_hash != hashed_password || stored_salt != salt) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "Password update verification failed - hash or salt mismatch for user: %s",
                    email.c_str());
            }
        } else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to verify password update: user not found after update");
        }
        
        sqlite3_finalize(verify_stmt);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Password reset successful for user: %s", email.c_str());
        return true;
    }

    bool PublicUsersDB::verify_email(const std::string& email, const std::string& code) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot verify email: database not initialized");
            return false;
        }

        const char* check_query = R"(
            SELECT user_cred FROM users
            WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* check_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, check_query, -1, &check_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(check_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(check_stmt);

        bool verification_success = false;

        if (rc == SQLITE_ROW && sqlite3_column_text(check_stmt, 0) != nullptr) {
            std::string stored_code = reinterpret_cast<const char*>(sqlite3_column_text(check_stmt, 0));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Verification attempt - Stored code: '%s', Entered code: '%s'", stored_code.c_str(), code.c_str());

            if (stored_code == code) {
                verification_success = true;
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "No verification code found for user: %s", email.c_str());
        }

        sqlite3_finalize(check_stmt);

        if (!verification_success) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid or expired verification code for user: %s", email.c_str());
            return false;
        }

        const char* update_query = R"(
            UPDATE users SET is_verified = 1 WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* update_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, update_query, -1, &update_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(update_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(update_stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update password: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            sqlite3_finalize(update_stmt);
            return false;
        }
        
        int changes = sqlite3_changes(db_);
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
            "Password update affected %d rows", changes);
            
        if (changes == 0) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                "Password update statement succeeded but no rows were affected for email: %s", 
                email.c_str());
        }
        
        sqlite3_finalize(update_stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update user verification status: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        const char* clear_query = R"(
            UPDATE users SET user_cred = NULL WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* clear_stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, clear_query, -1, &clear_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(clear_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(clear_stmt);
        sqlite3_finalize(clear_stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to clear verification code: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Email verified for user: %s", email.c_str());
        return true;
    }

    std::string PublicUsersDB::generate_verification_code(const std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot generate verification code: database not initialized");
            return "";
        }

        std::string code = generate_random_code();

        int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        int64_t expiry_time = current_time + (24 * 60 * 60);     

        const char* check_query = R"(
            SELECT code FROM verification_codes
            WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* check_stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, check_query, -1, &check_stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return "";
        }

        sqlite3_bind_text(check_stmt, 1, email.c_str(), -1, SQLITE_STATIC);
        rc = sqlite3_step(check_stmt);
        bool code_exists = (rc == SQLITE_ROW);
        sqlite3_finalize(check_stmt);

        const char* query = code_exists ?
            R"(
                UPDATE verification_codes
                SET code = ?, expiry = ?
                WHERE email = ? COLLATE NOCASE;
            )" :
            R"(
                INSERT INTO verification_codes (email, code, expiry)
                VALUES (?, ?, ?);
            )";

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return "";
        }

        if (code_exists) {
            sqlite3_bind_text(stmt, 1, code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 2, expiry_time);
            sqlite3_bind_text(stmt, 3, email.c_str(), -1, SQLITE_STATIC);
        }
        else {
            sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_text(stmt, 2, code.c_str(), -1, SQLITE_STATIC);
            sqlite3_bind_int64(stmt, 3, expiry_time);
        }

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to store verification code: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return "";
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Generated verification code for user: %s", email.c_str());
        return code;
    }


    bool PublicUsersDB::update_verification_status(const std::string& email, bool is_verified) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot update verification status: database not initialized");
            return false;
        }

        const char* query = R"(
            UPDATE users SET is_verified = ? WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_int(stmt, 1, is_verified ? 1 : 0);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update user verification status: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Updated verification status for user %s to %s", email.c_str(), is_verified ? "verified" : "unverified");
        return true;
    }

    bool PublicUsersDB::update_user_cred(const std::string& email, const std::string& cred) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot update user credential: database not initialized");
            return false;
        }

        const char* query = R"(
            UPDATE users SET user_cred = ? WHERE email = ? COLLATE NOCASE;
        )";

        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare statement: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        sqlite3_bind_text(stmt, 1, cred.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);

        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update user credential: %s (code: %d)",
                sqlite3_errmsg(db_), rc);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Updated credential for user %s", email.c_str());
        return true;
    }

    bool PublicUsersDB::update_display_name(const std::string& email, const std::string& display_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto& main_db = Database::get_instance();        
        bool main_db_result = main_db.update_display_name(email, display_name);
        
        if (!main_db_result) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to update display name in main game DB for user %s", 
                email.c_str());
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Updated display name for user %s to '%s' in main game database", 
            email.c_str(), display_name.c_str());
            
        return true;
    }

    bool PublicUsersDB::get_all_emails(std::vector<std::string>& emails) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot get all emails: database not initialized");
            return false;
        }
        
        const char* query = "SELECT email FROM users";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare get_all_emails query: %s", sqlite3_errmsg(db_));
            return false;
        }
        
        emails.clear();
        
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (email) {
                emails.push_back(std::string(email));
            }
        }
        
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Error while retrieving emails: %s", sqlite3_errmsg(db_));
            return false;
        }
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Retrieved %zu emails from public users database", emails.size());
        
        return true;
    }

    bool PublicUsersDB::search_users(const std::string& field, const std::string& term, std::vector<std::string>& matching_emails) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot search users: database not initialized");
            return false;
        }
        
        std::vector<std::string> valid_fields = {"email", "display_name", "is_verified", "registration_date", "last_login"};
        if (std::find(valid_fields.begin(), valid_fields.end(), field) == valid_fields.end()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Invalid search field: %s", field.c_str());
            return false;
        }
        
        std::string query = "SELECT email FROM users WHERE " + field + " LIKE ?";
        sqlite3_stmt* stmt = nullptr;
        
        int rc = sqlite3_prepare_v2(db_, query.c_str(), -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare search_users query: %s", sqlite3_errmsg(db_));
            return false;
        }
        
        std::string search_term = "%" + term + "%";
        rc = sqlite3_bind_text(stmt, 1, search_term.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to bind search term: %s", sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            return false;
        }
        
        matching_emails.clear();
        
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (email) {
                matching_emails.push_back(std::string(email));
            }
        }
        
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Error while searching users: %s", sqlite3_errmsg(db_));
            return false;
        }
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Found %zu matching users for search %s='%s'", matching_emails.size(), field.c_str(), term.c_str());
        
        return true;
    }

    bool PublicUsersDB::delete_user(const std::string& email) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!db_) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot delete user: database not initialized");
            return false;
        }
        
        if (!user_exists(email)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Cannot delete user: user %s does not exist", email.c_str());
            return false;
        }
        
        char* error_msg = nullptr;
        int rc = sqlite3_exec(db_, "BEGIN TRANSACTION", nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to begin transaction for delete_user: %s", error_msg ? error_msg : "unknown error");
            sqlite3_free(error_msg);
            return false;
        }
        
        const char* query = "DELETE FROM users WHERE email = ?";
        sqlite3_stmt* stmt = nullptr;
        
        rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare delete_user query: %s", sqlite3_errmsg(db_));
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to bind email for delete_user: %s", sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        if (rc != SQLITE_DONE) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Error while deleting user: %s", sqlite3_errmsg(db_));
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        const char* token_query = "DELETE FROM tokens WHERE email = ?";
        
        rc = sqlite3_prepare_v2(db_, token_query, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to prepare delete tokens query: %s", sqlite3_errmsg(db_));
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        rc = sqlite3_bind_text(stmt, 1, email.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to bind email for delete tokens: %s", sqlite3_errmsg(db_));
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        rc = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        
        rc = sqlite3_exec(db_, "COMMIT", nullptr, nullptr, &error_msg);
        if (rc != SQLITE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "Failed to commit transaction for delete_user: %s", error_msg ? error_msg : "unknown error");
            sqlite3_free(error_msg);
            sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "Successfully deleted user %s from public users database", email.c_str());
        
        return true;
    }
}
