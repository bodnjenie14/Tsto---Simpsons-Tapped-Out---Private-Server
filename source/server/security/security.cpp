#include <std_include.hpp>
#include "security.hpp"
#include "debugging/serverlog.hpp"
#include "tsto/auth/user_roles.hpp"
#include <algorithm>
#include <regex>

namespace tsto::security {

    bool LoginSecurity::can_attempt_login(const std::string& ip_address) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        if (now - last_cleanup_time_ > std::chrono::minutes(CLEANUP_INTERVAL_MINUTES)) {
            cleanup_old_entries();
            last_cleanup_time_ = now;
        }
        
        if (ip_failures_.find(ip_address) != ip_failures_.end()) {
            const auto& attempts = ip_failures_[ip_address];
            
            int recent_failures = 0;
            auto ban_threshold = now - std::chrono::minutes(IP_BAN_DURATION_MINUTES);
            
            for (const auto& attempt : attempts) {
                if (attempt.timestamp > ban_threshold) {
                    recent_failures++;
                }
            }
            
            if (recent_failures >= MAX_FAILED_ATTEMPTS_PER_IP) {
                auto most_recent = std::max_element(attempts.begin(), attempts.end(),
                    [](const FailedAttempt& a, const FailedAttempt& b) {
                        return a.timestamp < b.timestamp;
                    });
                
                auto ban_expiry = most_recent->timestamp + std::chrono::minutes(IP_BAN_DURATION_MINUTES);
                auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(ban_expiry - now).count();
                
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                    "[SECURITY] IP %s is temporarily banned due to too many failed login attempts. Ban expires in %d minutes",
                    ip_address.c_str(), minutes_left);
                
                return false;
            }
        }
        
        return true;
    }

    void LoginSecurity::record_failed_attempt(const std::string& ip_address, const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        FailedAttempt attempt;
        attempt.timestamp = std::chrono::system_clock::now();
        attempt.username = username;
        
        ip_failures_[ip_address].push_back(attempt);
        
        if (!username.empty()) {
            if (account_lockouts_.find(username) == account_lockouts_.end()) {
                account_lockouts_[username] = {attempt.timestamp, 1};
            } else {
                account_lockouts_[username].failed_attempts++;
                account_lockouts_[username].lockout_time = attempt.timestamp;
            }
            
            int failures = account_lockouts_[username].failed_attempts;
            
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Failed login attempt for user '%s' from IP %s (%d/%d attempts)",
                username.c_str(), ip_address.c_str(), failures, MAX_FAILED_ATTEMPTS_PER_ACCOUNT);
            
            if (failures >= MAX_FAILED_ATTEMPTS_PER_ACCOUNT) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                    "[SECURITY] Account '%s' has been temporarily locked due to too many failed attempts",
                    username.c_str());
            }
        } else {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Failed login attempt with empty username from IP %s",
                ip_address.c_str());
        }
    }

    void LoginSecurity::record_successful_login(const std::string& ip_address, const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!username.empty() && account_lockouts_.find(username) != account_lockouts_.end()) {
            account_lockouts_.erase(username);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Successful login for user '%s' from IP %s, reset failed attempt counter",
                username.c_str(), ip_address.c_str());
        }
        
    }

    bool LoginSecurity::is_account_locked(const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (username.empty() || account_lockouts_.find(username) == account_lockouts_.end()) {
            return false;
        }
        
        const auto& lockout = account_lockouts_[username];
        
        if (lockout.failed_attempts < MAX_FAILED_ATTEMPTS_PER_ACCOUNT) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        
        int lockout_minutes = ACCOUNT_LOCKOUT_DURATION_MINUTES;
        

        auto role = tsto::RoleType::ADMIN;
        if (role == tsto::RoleType::ADMIN) {
            // Admin accounts get a much shorter lockout period (2 minutes)
            lockout_minutes = 2;
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Admin account '%s' has special shorter lockout period (%d minutes)",
                username.c_str(), lockout_minutes);
        }
        
        auto lockout_expiry = lockout.lockout_time + std::chrono::minutes(lockout_minutes);
        
        if (now > lockout_expiry) {
            account_lockouts_.erase(username);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Account '%s' lockout period expired, account unlocked automatically",
                username.c_str());
                
            return false;
        }
        
        auto minutes_left = std::chrono::duration_cast<std::chrono::minutes>(lockout_expiry - now).count();
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
            "[SECURITY] Account '%s' is locked for %d more minutes",
            username.c_str(), minutes_left);
        
        return true;
    }

    bool LoginSecurity::unlock_account(const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (username.empty() || account_lockouts_.find(username) == account_lockouts_.end()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Attempted to unlock account '%s' but it was not locked",
                username.c_str());
            return false;
        }
        
        account_lockouts_.erase(username);
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
            "[SECURITY] Account '%s' has been manually unlocked by an administrator",
            username.c_str());
        
        return true;
    }
    
    std::vector<std::string> LoginSecurity::get_locked_accounts() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> locked_accounts;
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [username, lockout] : account_lockouts_) {
            if (lockout.failed_attempts >= MAX_FAILED_ATTEMPTS_PER_ACCOUNT) {
                auto lockout_expiry = lockout.lockout_time + std::chrono::minutes(ACCOUNT_LOCKOUT_DURATION_MINUTES);
                if (now <= lockout_expiry) {
                    locked_accounts.push_back(username);
                }
            }
        }
        
        return locked_accounts;
    }

    void LoginSecurity::cleanup_old_entries() {
        auto now = std::chrono::system_clock::now();
        auto ip_threshold = now - std::chrono::minutes(IP_BAN_DURATION_MINUTES);
        auto account_threshold = now - std::chrono::minutes(ACCOUNT_LOCKOUT_DURATION_MINUTES);
        
        for (auto it = ip_failures_.begin(); it != ip_failures_.end();) {
            auto& attempts = it->second;
            
            attempts.erase(
                std::remove_if(attempts.begin(), attempts.end(),
                    [&ip_threshold](const FailedAttempt& attempt) {
                        return attempt.timestamp < ip_threshold;
                    }),
                attempts.end()
            );
            
            if (attempts.empty()) {
                it = ip_failures_.erase(it);
            } else {
                ++it;
            }
        }
        
        for (auto it = account_lockouts_.begin(); it != account_lockouts_.end();) {
            if (it->second.lockout_time < account_threshold) {
                it = account_lockouts_.erase(it);
            } else {
                ++it;
            }
        }
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SECURITY,
            "[SECURITY] Cleaned up old security entries. Tracking %zu IPs and %zu accounts",
            ip_failures_.size(), account_lockouts_.size());
    }

    bool is_suspicious_input(const std::string& input) {
        static const std::vector<std::regex> sql_patterns = {
            std::regex(".*\\b(SELECT|INSERT|UPDATE|DELETE|DROP|ALTER)\\b.*", std::regex::icase),
            std::regex(".*['\"]\\s*OR\\s*['\"].*", std::regex::icase),
            std::regex(".*\\bOR\\b\\s+\\d+\\s*=\\s*\\d+.*", std::regex::icase),
            std::regex(".*\\bAND\\b\\s+\\d+\\s*=\\s*\\d+.*", std::regex::icase),
            std::regex(".*--.*"),
            std::regex(".*\\bUNION\\b.*", std::regex::icase),
            std::regex(".*[;].*")
        };
        
        static const std::vector<std::regex> xss_patterns = {
            std::regex(".*<script.*>.*", std::regex::icase),
            std::regex(".*<.*onload.*=.*", std::regex::icase),
            std::regex(".*<.*onerror.*=.*", std::regex::icase),
            std::regex(".*javascript:.*", std::regex::icase)
        };
        
        for (const auto& pattern : sql_patterns) {
            if (std::regex_match(input, pattern)) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                    "[SECURITY] Detected potential SQL injection attempt: %s", input.c_str());
                return true;
            }
        }
        
        for (const auto& pattern : xss_patterns) {
            if (std::regex_match(input, pattern)) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SECURITY,
                    "[SECURITY] Detected potential XSS attempt: %s", input.c_str());
                return true;
            }
        }
        
        return false;
    }

    std::string sanitize_input(const std::string& input) {
        std::string result = input;
        
        std::regex special_chars("[<>&\"']");
        result = std::regex_replace(result, special_chars, "");
        
        const size_t MAX_LENGTH = 128;
        if (result.length() > MAX_LENGTH) {
            result = result.substr(0, MAX_LENGTH);
        }
        
        return result;
    }
}