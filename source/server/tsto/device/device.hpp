#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace tsto::device {

    struct DeviceInfo {
        std::string device_id;              
        std::string android_id;            
        std::string vendor_id;              
        std::string advertising_id;         
        std::string platform;              
        std::string platform_id;           
        std::string as_identifier;             
        std::string combined_id;            
        uint64_t anon_uid = 0;                
        std::chrono::system_clock::time_point expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
    };

    class DeviceIdCache {
    public:
        static DeviceIdCache& get_instance() {
            static DeviceIdCache instance;
            return instance;
        }

        void store_device_id(const std::string& ip_address, const std::string& device_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(ip_address);
            if (it != cache_.end()) {
                it->second.device_id = device_id;
                it->second.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
            } else {
                DeviceInfo info;
                info.device_id = device_id;
                info.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
                cache_[ip_address] = info;
            }
        }

        void store_device_info(const std::string& ip_address, const DeviceInfo& info) {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_[ip_address] = info;
        }

        std::string get_device_id(const std::string& ip_address) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto it = cache_.find(ip_address);
            if (it != cache_.end() && it->second.expiry > std::chrono::system_clock::now() && !it->second.device_id.empty()) {
                auto& db = tsto::database::Database::get_instance();
                std::string existing_email;
                
                if (db.get_user_by_device_id(it->second.device_id, existing_email)) {
                    std::string android_id = it->second.android_id;
                    std::string platform_id = it->second.platform_id;
                    std::string new_device_id = generate_unique_device_id(android_id, platform_id, ip_address);
                    
                    it->second.device_id = new_device_id;
                    it->second.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
                    
                    return new_device_id;
                }
                
                return it->second.device_id;
            }
            
            std::string android_id = (it != cache_.end()) ? it->second.android_id : "";
            std::string platform_id = (it != cache_.end()) ? it->second.platform_id : "";
            
            std::string new_device_id = generate_unique_device_id(android_id, platform_id, ip_address);
            
            if (it != cache_.end()) {
                it->second.device_id = new_device_id;
                it->second.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
            } else {
                DeviceInfo info;
                info.device_id = new_device_id;
                info.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
                cache_[ip_address] = info;
            }
            
            return new_device_id;
        }

        DeviceInfo get_device_info(const std::string& ip_address) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(ip_address);
            if (it != cache_.end() && it->second.expiry > std::chrono::system_clock::now()) {
                return it->second;
            }
            return DeviceInfo();     
        }
        
        void store_anon_uid(const std::string& ip_address, uint64_t uid) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(ip_address);
            if (it != cache_.end()) {
                it->second.anon_uid = uid;
                it->second.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
            } else {
                DeviceInfo info;
                info.anon_uid = uid;
                info.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
                cache_[ip_address] = info;
            }
        }
        
        uint64_t get_anon_uid(const std::string& ip_address) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(ip_address);
            if (it != cache_.end() && it->second.expiry > std::chrono::system_clock::now()) {
                return it->second.anon_uid;
            }
            return 0;      
        }

        void update_device_info(const std::string& ip_address, 
                              const std::string& android_id,
                              const std::string& vendor_id,
                              const std::string& advertising_id,
                              const std::string& platform,
                              const std::string& platform_id) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = cache_.find(ip_address);
            if (it != cache_.end()) {
                if (!android_id.empty()) it->second.android_id = android_id;
                if (!vendor_id.empty()) it->second.vendor_id = vendor_id;
                if (!advertising_id.empty()) it->second.advertising_id = advertising_id;
                if (!platform.empty()) it->second.platform = platform;
                if (!platform_id.empty()) it->second.platform_id = platform_id;
                it->second.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
            } else if (!android_id.empty() || !vendor_id.empty() || !advertising_id.empty()) {
                DeviceInfo info;
                info.android_id = android_id;
                info.vendor_id = vendor_id;
                info.advertising_id = advertising_id;
                info.platform = platform;
                info.platform_id = platform_id;
                info.expiry = std::chrono::system_clock::now() + std::chrono::hours(24);
                cache_[ip_address] = info;
            }
        }

        void cleanup() {
            std::lock_guard<std::mutex> lock(mutex_);
            auto now = std::chrono::system_clock::now();
            for (auto it = cache_.begin(); it != cache_.end();) {
                if (it->second.expiry < now) {
                    it = cache_.erase(it);
                }
                else {
                    ++it;
                }
            }
        }

    private:
        std::string generate_unique_device_id(const std::string& android_id, const std::string& platform_id, const std::string& ip_address) {
            auto now = std::chrono::system_clock::now();
            auto now_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
            
            std::string device_id = utils::cryptography::sha256::compute(now_str, true);
            
            return device_id;
        }
        
        std::unordered_map<std::string, DeviceInfo> cache_;
        std::mutex mutex_;
    };

    class Device {
    public:
        static void handle_device_registration(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_get_anon_uid(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_get_device_id(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_validate_device_id(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);

        static std::string generateUniqueDeviceId(const std::string& androidId, const std::string& hwId, const std::string& clientIp);
    };
}