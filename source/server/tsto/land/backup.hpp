#pragma once

#include "std_include.hpp"
#include <string>
#include <vector>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <filesystem>
#include "debugging/serverlog.hpp"
#include "configuration.hpp"

namespace tsto {
namespace land {

class BackupManager {
public:
    static BackupManager& get_instance();

    void initialize(int backup_interval_hours = 4, int backup_interval_seconds = 0);

    void start();

    void stop();

    bool backup_town_file(const std::string& source_path);

    int backup_all_towns();

    bool should_create_backup(const std::string& source_path, const std::string& backup_path);

    std::string get_backup_dir() const;

private:
    BackupManager();
    ~BackupManager();

    BackupManager(const BackupManager&) = delete;
    BackupManager& operator=(const BackupManager&) = delete;

    void backup_thread_func();

    int backup_interval_hours_;
    int backup_interval_seconds_;    
    bool use_seconds_mode_;           

    std::chrono::system_clock::time_point last_backup_time_;

    std::thread backup_thread_;
    std::mutex backup_mutex_;
    std::atomic<bool> should_stop_;
};

}   
}   
