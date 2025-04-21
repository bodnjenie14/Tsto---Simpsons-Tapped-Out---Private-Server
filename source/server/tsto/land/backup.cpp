#include "std_include.hpp"
#include "backup.hpp"
#include "debugging/serverlog.hpp"
#include "configuration.hpp"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <ctime>

namespace tsto {
namespace land {

BackupManager& BackupManager::get_instance() {
    static BackupManager instance;
    return instance;
}

BackupManager::BackupManager() 
    : backup_interval_hours_(2), backup_interval_seconds_(0), use_seconds_mode_(false), should_stop_(false) {
    last_backup_time_ = std::chrono::system_clock::now();
}

BackupManager::~BackupManager() {
    stop();
}

void BackupManager::initialize(int backup_interval_hours, int backup_interval_seconds) {
    backup_interval_hours_ = backup_interval_hours;
    backup_interval_seconds_ = backup_interval_seconds;
    
    use_seconds_mode_ = (backup_interval_seconds > 0);
    
    std::filesystem::create_directories(get_backup_dir());
    
    if (use_seconds_mode_) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup manager initialized with %d second interval (testing mode)", backup_interval_seconds_);
    } else {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup manager initialized with %d hour interval", backup_interval_hours_);
    }
}

void BackupManager::start() {
    std::lock_guard<std::mutex> lock(backup_mutex_);
    
    if (!backup_thread_.joinable()) {
        should_stop_ = false;
        backup_thread_ = std::thread(&BackupManager::backup_thread_func, this);
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup thread started");
    } else {
        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup thread already running");
    }
}

void BackupManager::stop() {
    {
        std::lock_guard<std::mutex> lock(backup_mutex_);
        should_stop_ = true;
    }
    
    if (backup_thread_.joinable()) {
        backup_thread_.join();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup thread stopped");
    }
}

void BackupManager::backup_thread_func() {
    if (use_seconds_mode_) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup thread started, will run every %d seconds (testing mode)", backup_interval_seconds_);
    } else {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Backup thread started, will run every %d hours", backup_interval_hours_);
    }
    
    while (!should_stop_) {
        if (use_seconds_mode_) {
            for (int i = 0; i < backup_interval_seconds_ && !should_stop_; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        } else {
            for (int i = 0; i < 4 * backup_interval_hours_ && !should_stop_; ++i) {
                std::this_thread::sleep_for(std::chrono::minutes(15));
            }
        }
        
        if (should_stop_) break;
        
        auto now = std::chrono::system_clock::now();
        
        bool should_backup = false;
        if (use_seconds_mode_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_backup_time_).count();
            should_backup = (elapsed >= backup_interval_seconds_);
        } else {
            auto elapsed = std::chrono::duration_cast<std::chrono::hours>(now - last_backup_time_).count();
            should_backup = (elapsed >= backup_interval_hours_);
        }
        
        if (should_backup) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[BACKUP] Starting scheduled backup of all towns");
            
            int backed_up = backup_all_towns();
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[BACKUP] Scheduled backup completed, %d files backed up", backed_up);
            
            last_backup_time_ = now;
        }
    }
}

std::string BackupManager::get_backup_dir() const {
    return "town_backups";
}

bool BackupManager::should_create_backup(const std::string& source_path, const std::string& backup_path) {
    if (!std::filesystem::exists(backup_path)) {
        return true;
    }
    
    try {
        std::uintmax_t source_size = std::filesystem::file_size(source_path);
        std::uintmax_t backup_size = std::filesystem::file_size(backup_path);
        
        if (source_size < backup_size) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                "[BACKUP] Source file %s is smaller than backup (%d < %d bytes), skipping backup",
                source_path.c_str(), source_size, backup_size);
            return false;
        }
        
        if (source_size == backup_size) {
            auto source_time = std::filesystem::last_write_time(source_path);
            auto backup_time = std::filesystem::last_write_time(backup_path);
            
            return source_time > backup_time;
        }
        
        return true;
    }
    catch (const std::filesystem::filesystem_error& e) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
            "[BACKUP] Error checking file sizes: %s", e.what());
        return false;
    }
}

bool BackupManager::backup_town_file(const std::string& source_path) {
    if (!std::filesystem::exists(source_path)) {
        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
            "[BACKUP] Source file does not exist: %s", source_path.c_str());
        return false;
    }
    
    try {
        std::filesystem::path source(source_path);
        std::string filename = source.filename().string();
        
        auto now = std::chrono::system_clock::now();
        auto now_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream timestamp;
        timestamp << std::put_time(std::localtime(&now_time_t), "%Y%m%d_%H%M%S");
        
        std::stringstream date_str;
        date_str << std::put_time(std::localtime(&now_time_t), "%Y-%m-%d");
        
        std::string backup_dir = get_backup_dir();
        std::string date_subdir = backup_dir + "/" + date_str.str();
        
        std::filesystem::create_directories(date_subdir);
        
        std::string backup_path = date_subdir + "/" + filename + "." + timestamp.str() + ".bak";
        
        std::string latest_backup_path = backup_dir + "/" + filename + ".latest.bak";
        
        if (!should_create_backup(source_path, latest_backup_path)) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[BACKUP] Skipping backup of %s - current backup is larger or identical",
                source_path.c_str());
            return false;
        }
        
        std::filesystem::copy_file(
            source_path, 
            backup_path, 
            std::filesystem::copy_options::overwrite_existing
        );
        
        std::filesystem::copy_file(
            source_path, 
            latest_backup_path, 
            std::filesystem::copy_options::overwrite_existing
        );
        
        std::uintmax_t file_size = std::filesystem::file_size(source_path);
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Successfully backed up %s to %s (%d bytes)",
            source_path.c_str(), backup_path.c_str(), file_size);
        
        return true;
    }
    catch (const std::filesystem::filesystem_error& e) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
            "[BACKUP] Error backing up file %s: %s", source_path.c_str(), e.what());
        return false;
    }
}

int BackupManager::backup_all_towns() {
    std::string backup_dir = get_backup_dir();
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
        "[BACKUP] Starting backup of all town files from towns to %s", backup_dir.c_str());
    
    std::error_code ec;
    if (!std::filesystem::exists(backup_dir, ec)) {
        bool created = std::filesystem::create_directories(backup_dir, ec);
        if (!created || ec) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[BACKUP] Failed to create backup directory: %s, error: %s", 
                backup_dir.c_str(), ec.message().c_str());
            return 0;
        }
    }
    
    try {
        int success_count = 0;
        
        for (const auto& entry : std::filesystem::directory_iterator("towns")) {
            if (!entry.is_regular_file()) continue;
            
            std::string path = entry.path().string();
            std::string extension = entry.path().extension().string();
            
            if (extension == ".pb") {
                if (backup_town_file(path)) {
                    success_count++;
                }
            }
        }
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[BACKUP] Completed backup of all town files, %d files backed up successfully",
            success_count);
        
        return success_count;
    }
    catch (const std::filesystem::filesystem_error& e) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
            "[BACKUP] Error during backup of all towns: %s", e.what());
        return 0;
    }
}

}   
}   
