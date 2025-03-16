#include "std_include.hpp"
#include "updater.hpp"
#include "debugging/serverlog.hpp"
#include "utilities/linux_compat.hpp"
#include "utilities/configuration.hpp"
#include <curl/curl.h>
#include <filesystem>

namespace {
    size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }
}

namespace updater {

    const std::string SERVER_VERSION = "v0.11";
    const std::string GITHUB_API_URL = "https://api.github.com/repos/bodnjenie14/Tsto---Simpsons-Tapped-Out---Private-Server/releases/latest";
    const std::string DOWNLOAD_URL_BASE = "https://github.com/bodnjenie14/Tsto---Simpsons-Tapped-Out---Private-Server/releases/download/";

    bool check_for_updates() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, 
                "Failed to initialize CURL");
            return false;
        }

        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, GITHUB_API_URL.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "TSTO-Server/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // For development only

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE,
                "Failed to check for updates: %s", curl_easy_strerror(res));
            return false;
        }

        // Parse JSON response to get latest version and asset name
        size_t tagPos = response.find("\"tag_name\":");
        size_t assetPos = response.find("\"name\":", response.find("\"assets\":"));

        if (tagPos != std::string::npos && assetPos != std::string::npos) {
            size_t startPos = response.find("\"", tagPos + 11) + 1;
            size_t endPos = response.find("\"", startPos);
            std::string latestVersion = response.substr(startPos, endPos - startPos);

            // Get asset name
            startPos = response.find("\"", assetPos + 7) + 1;
            endPos = response.find("\"", startPos);
            std::string assetName = response.substr(startPos, endPos - startPos);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Current version: %s, Latest version: %s, Asset: %s",
                SERVER_VERSION.c_str(), latestVersion.c_str(), assetName.c_str());

            // Store the asset name for download_and_update
            utils::configuration::WriteString("UpdateInfo", "LatestVersion", latestVersion);
            utils::configuration::WriteString("UpdateInfo", "AssetName", assetName);


            // For alpha/beta releases, check the asset version number
            if (latestVersion == "alpha" || latestVersion == "beta") {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                    "Extracting version from asset name: %s", assetName.c_str());
                
                size_t vPos = assetName.find(".V");
                if (vPos != std::string::npos) {
                    size_t startPos = vPos + 2; // Skip ".V"
                    std::string versionStr;
                    
                    // Read all digits and dots until we hit something else
                    for (size_t i = startPos; i < assetName.length(); i++) {
                        if (std::isdigit(assetName[i]) || assetName[i] == '.') {
                            versionStr += assetName[i];
                        } else {
                            break;
                        }
                    }
                    
                    if (!versionStr.empty()) {
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                            "Extracted version string: %s", versionStr.c_str());
                        
                        // Remove 'v' prefix from current version for comparison
                        std::string currentVer = SERVER_VERSION;
                        if (currentVer[0] == 'v') currentVer = currentVer.substr(1);
                        
                        // Compare version numbers
                        float currentNum = std::stof(currentVer);
                        float assetNum = std::stof(versionStr);
                        
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER,
                            "Version comparison - Current: %f, Asset: %f",
                            currentNum, assetNum);
                        
                        if (assetNum > currentNum) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                                "Update available: %s -> %s", 
                                SERVER_VERSION.c_str(), versionStr.c_str());
                            return true;
                        }
                    } else {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                            "Failed to extract version number from asset name");
                    }
                }
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "No update needed or version extraction failed");
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "Running latest version");
                return false;
            }

            // Regular version comparison for non-alpha/beta releases
            std::string currentVer = SERVER_VERSION;
            std::string latestVer = latestVersion;
            if (currentVer[0] == 'v') currentVer = currentVer.substr(1);
            if (latestVer[0] == 'v') latestVer = latestVer.substr(1);

            float currentNum = std::stof(currentVer);
            float latestNum = std::stof(latestVer);
            
            return latestNum > currentNum;
        }

        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to parse GitHub API response");
        return false;
    }

    std::string get_server_version() {
        return SERVER_VERSION;
    }

    bool download_update(const std::string& url, const std::string& output_path) {
        CURL* curl = curl_easy_init();
        if (!curl) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE,
                "Failed to initialize CURL for download");
            return false;
        }

        FILE* fp = fopen(output_path.c_str(), "wb");
        if (!fp) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE,
                "Failed to open output file: %s", output_path.c_str());
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // For development only

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);

        if (res != CURLE_OK) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE,
                "Failed to download update: %s", curl_easy_strerror(res));
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE,
            "Update downloaded successfully to: %s", output_path.c_str());

        return true;
    }

    void download_and_update() {
        std::string latestVersion = utils::configuration::ReadString("UpdateInfo", "LatestVersion", "alpha");
        std::string assetName = utils::configuration::ReadString("UpdateInfo", "AssetName", "Tsto_Server_Bodnjenie.V0.05.zip");

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Starting update process to version %s", latestVersion.c_str());
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Asset name: %s", assetName.c_str());

        std::filesystem::path updateDir = std::filesystem::temp_directory_path() / "tsto_update";
        std::filesystem::create_directories(updateDir);

        std::string downloadUrl = DOWNLOAD_URL_BASE + latestVersion + "/" + assetName;
        std::string zipPath = (updateDir / "update.zip").string();

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Download URL: %s", downloadUrl.c_str());
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Update directory: %s", updateDir.string().c_str());
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Zip path: %s", zipPath.c_str());

        if (!download_update(downloadUrl, zipPath)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Failed to download update");
            return;
        }

        // Platform-specific update process
#ifdef _WIN32
        std::string psCommand = "powershell -Command \"";
        psCommand += "$ProgressPreference = 'SilentlyContinue'; ";
        psCommand += "try {";
        psCommand += "  Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + updateDir.string() + "' -Force -ErrorAction Stop; ";
        psCommand += "  Start-Sleep -Seconds 1; ";
        psCommand += "  Stop-Process -Name 'tsto_server' -Force -ErrorAction SilentlyContinue; ";
        psCommand += "  Start-Sleep -Seconds 3; ";
        psCommand += "  Copy-Item -Path '" + updateDir.string() + "\\*' -Destination '.' -Recurse -Force -ErrorAction Stop; ";
        psCommand += "  Remove-Item '" + zipPath + "' -Force -ErrorAction SilentlyContinue; ";
        psCommand += "  Remove-Item '" + updateDir.string() + "' -Recurse -Force -ErrorAction SilentlyContinue; ";
        psCommand += "  Start-Process 'tsto_server.exe' -ErrorAction Stop; ";
        psCommand += "} catch { exit 1; }\"";
        if (system(psCommand.c_str()) != 0) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Update process failed");
        }
#else
        std::string bashCommand = "bash -c '";
        bashCommand += "unzip -o \"" + zipPath + "\" -d \"" + updateDir.string() + "\" && ";
        bashCommand += "sleep 1 && ";
        bashCommand += "pkill -f tsto_server && ";
        bashCommand += "sleep 3 && ";
        bashCommand += "cp -rf \"" + updateDir.string() + "/\"* . && ";
        bashCommand += "rm -f \"" + zipPath + "\" && ";
        bashCommand += "rm -rf \"" + updateDir.string() + "\" && ";
        bashCommand += "./tsto_server &'";
        if (system(bashCommand.c_str()) != 0) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Update process failed");
        }
#endif

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Update process completed");
    }

} // namespace updater