#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <vector>
#include <ctime>

namespace tsto::security {

    class LoginSecurity {
    public:
        static LoginSecurity& get_instance() {
            static LoginSecurity instance;
            return instance;
        }

        bool can_attempt_login(const std::string& ip_address);
        void record_failed_attempt(const std::string& ip_address, const std::string& username);
        void record_successful_login(const std::string& ip_address, const std::string& username);
        bool is_account_locked(const std::string& username);
        bool unlock_account(const std::string& username);
        std::vector<std::string> get_locked_accounts();
        void cleanup_old_entries();

    private:
        LoginSecurity() = default;
        ~LoginSecurity() = default;
        LoginSecurity(const LoginSecurity&) = delete;
        LoginSecurity& operator=(const LoginSecurity&) = delete;

        //security settings
        static constexpr int MAX_FAILED_ATTEMPTS_PER_IP = 10;       // Max failed attempts per IP before temporary ban
        static constexpr int MAX_FAILED_ATTEMPTS_PER_ACCOUNT = 5;   // Max failed attempts per account before lockout
        static constexpr int IP_BAN_DURATION_MINUTES = 30;          // Duration of IP ban in minutes
        static constexpr int ACCOUNT_LOCKOUT_DURATION_MINUTES = 15; // Duration of account lockout in minutes
        static constexpr int CLEANUP_INTERVAL_MINUTES = 60;         // How often to clean up old entries

        struct FailedAttempt {
            std::chrono::system_clock::time_point timestamp;
            std::string username;
        };

        struct AccountLockout {
            std::chrono::system_clock::time_point lockout_time;
            int failed_attempts;
        };

        std::unordered_map<std::string, std::vector<FailedAttempt>> ip_failures_;
        std::unordered_map<std::string, AccountLockout> account_lockouts_;
        
        std::mutex mutex_;
        
        std::chrono::system_clock::time_point last_cleanup_time_;
    };

    bool is_suspicious_input(const std::string& input);
    std::string sanitize_input(const std::string& input);
}