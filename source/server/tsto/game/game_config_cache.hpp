#pragma once
#include <std_include.hpp>
#include <rapidjson/document.h>
#include <mutex>
#include <string>
#include <ctime>
#include <io.hpp>
#include "debugging/serverlog.hpp"

namespace tsto::game {

    class GameConfigCache {
    public:
        static bool is_valid() {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (!cache_valid_) {
                return false;
            }
            
            struct stat file_stat;
            if (stat("config.json", &file_stat) == 0) {
                if (file_stat.st_mtime > last_modified_) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CONFIG-CACHE] Config file has been modified, invalidating cache");
                    cache_valid_ = false;
                    return false;
                }
            } else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CONFIG-CACHE] Failed to stat config file, invalidating cache");
                cache_valid_ = false;
                return false;
            }
            
            return true;
        }
        
        static bool load() {
            std::lock_guard<std::mutex> lock(mutex_);
            
            try {
                if (!utils::io::read_file("config.json", &json_data_)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[CONFIG-CACHE] Failed to read config.json");
                    cache_valid_ = false;
                    return false;
                }
                
                config_doc_.Parse<rapidjson::kParseCommentsFlag | rapidjson::kParseTrailingCommasFlag>(json_data_.c_str());
                
                if (config_doc_.HasParseError()) {
                    rapidjson::ParseErrorCode error = config_doc_.GetParseError();
                    size_t errorOffset = config_doc_.GetErrorOffset();
                    
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[CONFIG-CACHE] JSON parse error: code %d at offset %zu",
                        error, errorOffset);
                    
                    cache_valid_ = false;
                    return false;
                }
                
                struct stat file_stat;
                if (stat("config.json", &file_stat) == 0) {
                    last_modified_ = file_stat.st_mtime;
                }
                
                cache_valid_ = true;
                
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[CONFIG-CACHE] Successfully loaded and cached config.json (%zu bytes)",
                    json_data_.size());
                
                return true;
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CONFIG-CACHE] Error loading config: %s", ex.what());
                cache_valid_ = false;
                return false;
            }
        }
        
        static void invalidate() {
            std::lock_guard<std::mutex> lock(mutex_);
            cache_valid_ = false;
        }
        
        static const rapidjson::Document& get_document() {
            std::lock_guard<std::mutex> lock(mutex_);
            return config_doc_;
        }
        
        static const std::string& get_json_data() {
            std::lock_guard<std::mutex> lock(mutex_);
            return json_data_;
        }
        
    private:
        static std::mutex mutex_;
        static rapidjson::Document config_doc_;
        static std::string json_data_;
        static std::time_t last_modified_;
        static bool cache_valid_;
    };
    
    std::mutex GameConfigCache::mutex_;
    rapidjson::Document GameConfigCache::config_doc_;
    std::string GameConfigCache::json_data_;
    std::time_t GameConfigCache::last_modified_ = 0;
    bool GameConfigCache::cache_valid_ = false;

}   
