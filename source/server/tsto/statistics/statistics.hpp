#pragma once
#include <std_include.hpp>
#include <mutex>
#include <unordered_map>
#include <string>
#include <chrono>
#include <vector>
#include <unordered_set>
#include <thread>
#include <atomic>

namespace tsto::statistics {

    struct UploadStats {
        int total_uploads;
        int total_approvals;
        std::chrono::system_clock::time_point last_upload;
    };

    struct PublicUploadStats {
        int total_submissions;
        int approved_submissions;
        int rejected_submissions;
        std::chrono::system_clock::time_point last_submission;
    };

    struct ClientInfo {
        std::chrono::system_clock::time_point last_activity;
    };

    class Statistics {
    public:
        static Statistics& get_instance();

        void increment_active_connections();
        void decrement_active_connections();
        int get_active_connections() const;

        void register_client_ip(const std::string& ip);
        void unregister_client_ip(const std::string& ip);
        int get_unique_clients() const;

        void update_client_activity(const std::string& ip);
        void cleanup_inactive_clients();
        void start_cleanup_thread();
        void stop_cleanup_thread();

        void record_town_upload(const std::string& username);
        void record_town_approval(const std::string& username);
        std::unordered_map<std::string, UploadStats> get_upload_stats() const;
        void clear_upload_stats();

        void record_public_town_submission(const std::string& device_id);
        void record_public_town_approval(const std::string& device_id);
        void record_public_town_rejection(const std::string& device_id);
        std::unordered_map<std::string, PublicUploadStats> get_public_upload_stats() const;
        void clear_public_upload_stats();

        std::string to_json() const;

    private:
        Statistics();
        ~Statistics();
        Statistics(const Statistics&) = delete;
        Statistics& operator=(const Statistics&) = delete;

        std::mutex mutex_;
        int active_connections_;
        std::unordered_map<std::string, UploadStats> upload_stats_;
        std::unordered_map<std::string, PublicUploadStats> public_upload_stats_;
        std::unordered_map<std::string, ClientInfo> active_clients_;

        std::thread cleanup_thread_;
        std::atomic<bool> cleanup_thread_running_;
        static constexpr int CLIENT_TIMEOUT_SECONDS = 300;    
        static constexpr int CLEANUP_INTERVAL_SECONDS = 30;     
    };

}
