#include <std_include.hpp>
#include "statistics.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "debugging/serverlog.hpp"

namespace tsto::statistics {

    Statistics& Statistics::get_instance() {
        static Statistics instance;
        return instance;
    }

    Statistics::Statistics() : active_connections_(0), cleanup_thread_running_(false) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[STATISTICS] Statistics module initialized");
        start_cleanup_thread();
    }

    Statistics::~Statistics() {
        stop_cleanup_thread();
    }

    void Statistics::increment_active_connections() {
        std::lock_guard<std::mutex> lock(mutex_);
        active_connections_++;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
            "[STATISTICS] Active connections: %d", active_connections_);
    }

    void Statistics::decrement_active_connections() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (active_connections_ > 0) {
            active_connections_--;
        }
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
            "[STATISTICS] Active connections: %d", active_connections_);
    }

    int Statistics::get_active_connections() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return active_connections_;
    }

    void Statistics::register_client_ip(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        active_clients_[ip].last_activity = std::chrono::system_clock::now();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
            "[STATISTICS] Registered client IP: %s, Total unique clients: %d",
            ip.c_str(), static_cast<int>(active_clients_.size()));
    }

    void Statistics::unregister_client_ip(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_clients_.find(ip);
        if (it != active_clients_.end()) {
            active_clients_.erase(it);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[STATISTICS] Unregistered client IP: %s, Total unique clients: %d",
                ip.c_str(), static_cast<int>(active_clients_.size()));
        }
    }

    int Statistics::get_unique_clients() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return static_cast<int>(active_clients_.size());
    }

    void Statistics::update_client_activity(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = active_clients_.find(ip);
        if (it != active_clients_.end()) {
            it->second.last_activity = std::chrono::system_clock::now();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[STATISTICS] Updated activity for client IP: %s", ip.c_str());
        }
        else {
            active_clients_[ip].last_activity = std::chrono::system_clock::now();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[STATISTICS] Registered client IP: %s, Total unique clients: %d",
                ip.c_str(), static_cast<int>(active_clients_.size()));
        }
    }

    void Statistics::cleanup_inactive_clients() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto now = std::chrono::system_clock::now();
        auto timeout = std::chrono::seconds(CLIENT_TIMEOUT_SECONDS);

        std::vector<std::string> ips_to_remove;

        for (const auto& [ip, info] : active_clients_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - info.last_activity);
            if (elapsed > timeout) {
                ips_to_remove.push_back(ip);
            }
        }

        for (const auto& ip : ips_to_remove) {
            active_clients_.erase(ip);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[STATISTICS] Removed inactive client IP: %s (timeout after %d seconds / 5 minutes)",
                ip.c_str(), CLIENT_TIMEOUT_SECONDS);
        }

        if (!ips_to_remove.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[STATISTICS] Cleaned up %d inactive clients, %d active clients remaining",
                static_cast<int>(ips_to_remove.size()), static_cast<int>(active_clients_.size()));
        }
    }

    void Statistics::start_cleanup_thread() {
        if (!cleanup_thread_running_) {
            cleanup_thread_running_ = true;
            cleanup_thread_ = std::thread([this]() {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[STATISTICS] Started client cleanup thread");

                while (cleanup_thread_running_) {
                    std::this_thread::sleep_for(std::chrono::seconds(CLEANUP_INTERVAL_SECONDS));

                    try {
                        cleanup_inactive_clients();
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[STATISTICS] Error in cleanup thread: %s", ex.what());
                    }
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[STATISTICS] Stopped client cleanup thread");
                });
        }
    }

    void Statistics::stop_cleanup_thread() {
        if (cleanup_thread_running_) {
            cleanup_thread_running_ = false;
            if (cleanup_thread_.joinable()) {
                cleanup_thread_.join();
            }
        }
    }

    void Statistics::record_town_upload(const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (upload_stats_.find(username) != upload_stats_.end()) {
            upload_stats_[username].total_uploads++;
        }
        else {
            UploadStats stats;
            stats.total_uploads = 1;
            stats.total_approvals = 0;
            upload_stats_[username] = stats;
        }

        upload_stats_[username].last_upload = std::chrono::system_clock::now();
    }

    void Statistics::record_town_approval(const std::string& username) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (upload_stats_.find(username) != upload_stats_.end()) {
            upload_stats_[username].total_approvals++;
        }
        else {
            UploadStats stats;
            stats.total_uploads = 0;
            stats.total_approvals = 1;
            upload_stats_[username] = stats;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[STATISTICS] Town approval recorded for admin: %s, Total approvals: %d",
            username.c_str(), upload_stats_[username].total_approvals);
    }

    std::unordered_map<std::string, UploadStats> Statistics::get_upload_stats() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return upload_stats_;
    }

    void Statistics::clear_upload_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        upload_stats_.clear();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[STATISTICS] Upload stats cleared");
    }

    void Statistics::record_public_town_submission(const std::string& device_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto& stats = public_upload_stats_[device_id];
        stats.total_submissions++;
        stats.last_submission = std::chrono::system_clock::now();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[STATISTICS] Public town submission recorded for device ID: %s, Total submissions: %d",
            device_id.c_str(), stats.total_submissions);
    }

    void Statistics::record_public_town_approval(const std::string& device_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = public_upload_stats_.find(device_id);
        if (it != public_upload_stats_.end()) {
            it->second.approved_submissions++;
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[STATISTICS] Public town approval recorded for device ID: %s, Total approvals: %d",
                device_id.c_str(), it->second.approved_submissions);
        }
        else {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                "[STATISTICS] Attempted to record approval for unknown device ID: %s", device_id.c_str());
        }
    }

    void Statistics::record_public_town_rejection(const std::string& device_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = public_upload_stats_.find(device_id);
        if (it != public_upload_stats_.end()) {
            it->second.rejected_submissions++;
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[STATISTICS] Public town rejection recorded for device ID: %s, Total rejections: %d",
                device_id.c_str(), it->second.rejected_submissions);
        }
        else {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                "[STATISTICS] Attempted to record rejection for unknown device ID: %s", device_id.c_str());
        }
    }

    std::unordered_map<std::string, PublicUploadStats> Statistics::get_public_upload_stats() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
        return public_upload_stats_;
    }

    void Statistics::clear_public_upload_stats() {
        std::lock_guard<std::mutex> lock(mutex_);
        public_upload_stats_.clear();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[STATISTICS] Public upload stats cleared");
    }

    std::string Statistics::to_json() const {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));

        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        doc.AddMember("active_connections", active_connections_, allocator);

        doc.AddMember("unique_clients", static_cast<int>(active_clients_.size()), allocator);

        rapidjson::Value uploads(rapidjson::kArrayType);
        for (const auto& [username, stats] : upload_stats_) {
            rapidjson::Value upload(rapidjson::kObjectType);
            upload.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);
            upload.AddMember("total_uploads", stats.total_uploads, allocator);
            upload.AddMember("total_approvals", stats.total_approvals, allocator);

            auto time_t = std::chrono::system_clock::to_time_t(stats.last_upload);
            std::tm tm = *std::localtime(&time_t);
            char time_str[30];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);
            upload.AddMember("last_upload", rapidjson::Value(time_str, allocator), allocator);

            uploads.PushBack(upload, allocator);
        }
        doc.AddMember("uploads", uploads, allocator);

        rapidjson::Value public_uploads(rapidjson::kArrayType);
        for (const auto& [device_id, stats] : public_upload_stats_) {
            rapidjson::Value public_upload(rapidjson::kObjectType);
            public_upload.AddMember("device_id", rapidjson::Value(device_id.c_str(), allocator), allocator);
            public_upload.AddMember("total_submissions", stats.total_submissions, allocator);
            public_upload.AddMember("approved_submissions", stats.approved_submissions, allocator);
            public_upload.AddMember("rejected_submissions", stats.rejected_submissions, allocator);

            auto time_t = std::chrono::system_clock::to_time_t(stats.last_submission);
            std::tm tm = *std::localtime(&time_t);
            char time_str[30];
            std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm);
            public_upload.AddMember("last_submission", rapidjson::Value(time_str, allocator), allocator);

            public_uploads.PushBack(public_upload, allocator);
        }
        doc.AddMember("public_uploads", public_uploads, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        return buffer.GetString();
    }

}
