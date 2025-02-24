#include <std_include.hpp>
#include "updater.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <wininet.h>
#include <shlobj.h>
#pragma comment(lib, "wininet.lib")
#include "configuration.hpp"

namespace updater {

    const std::string SERVER_VERSION = "v0.04";
    const std::string GITHUB_API_URL = "https://api.github.com/repos/bodnjenie14/Tsto---Simpsons-Tapped-Out---Private-Server/releases/latest";
    const std::string DOWNLOAD_URL_BASE = "https://github.com/bodnjenie14/Tsto---Simpsons-Tapped-Out---Private-Server/releases/download/";

    bool check_for_updates() {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Checking GitHub API: %s", GITHUB_API_URL.c_str());

        HINTERNET hInternet = InternetOpenA("TSTO-Server-Updater", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
        if (!hInternet) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to initialize WinINet");
            return false;
        }

        HINTERNET hConnect = InternetOpenUrlA(hInternet, GITHUB_API_URL.c_str(), NULL, 0, INTERNET_FLAG_RELOAD, 0);
        if (!hConnect) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to connect to GitHub API");
            InternetCloseHandle(hInternet);
            return false;
        }

        char buffer[4096];
        DWORD bytesRead;
        std::string response;

        while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            response.append(buffer, bytesRead);
        }

        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);

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
                    "✓ Running latest version");
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

void download_and_update() {
    std::string latestVersion = utils::configuration::ReadString("UpdateInfo", "LatestVersion", "alpha");
    std::string assetName = utils::configuration::ReadString("UpdateInfo", "AssetName", "Tsto_Server_Bodnjenie.V0.05.zip");

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Starting update process to version %s", latestVersion.c_str());
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Asset name: %s", assetName.c_str());

    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string updateDir = std::string(tempPath) + "\\tsto_update";
    CreateDirectoryA(updateDir.c_str(), NULL);

    std::string downloadUrl = DOWNLOAD_URL_BASE + latestVersion + "/" + assetName;
    std::string zipPath = updateDir + "\\update.zip";

    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Download URL: %s", downloadUrl.c_str());
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Update directory: %s", updateDir.c_str());
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "Zip path: %s", zipPath.c_str());

    std::string psCommand = "powershell -Command \"";
    psCommand += "$ProgressPreference = 'SilentlyContinue'; ";
    psCommand += "try {";

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Downloading update file...");
    psCommand += "  Invoke-WebRequest '" + downloadUrl + "' -OutFile '" + zipPath + "' -ErrorAction Stop; ";
    psCommand += "  Start-Sleep -Seconds 1; ";

    psCommand += "  if (Test-Path '" + zipPath + "') {";
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Download complete. Extracting files...");
    
    psCommand += "    Expand-Archive -Path '" + zipPath + "' -DestinationPath '" + updateDir + "' -Force -ErrorAction Stop; ";
    psCommand += "    Start-Sleep -Seconds 1; ";
    
    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_UPDATE, "[UPDATE] Extraction complete. Stopping current server...");
    psCommand += "    Stop-Process -Name 'tsto_server' -Force -ErrorAction SilentlyContinue; ";
    psCommand += "    Start-Sleep -Seconds 3; ";

    psCommand += "    if (-not (Test-Path '" + updateDir + "')) { exit 2; }";

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Installing new version...");
    psCommand += "    Copy-Item -Path '" + updateDir + "\\*' -Destination '.' -Recurse -Force -ErrorAction Stop; ";
    psCommand += "    Start-Sleep -Seconds 1; ";

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Cleaning up...");
    psCommand += "    Remove-Item '" + zipPath + "' -Force -ErrorAction SilentlyContinue; ";
    psCommand += "    Remove-Item '" + updateDir + "' -Recurse -Force -ErrorAction SilentlyContinue; ";

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Update complete! Starting new server version...");
    psCommand += "    Start-Sleep -Seconds 2; ";
    psCommand += "    Start-Process 'tsto_server.exe' -ErrorAction Stop; ";
    
    psCommand += "  } else { exit 3; }";
    psCommand += "} catch { exit 1; }\"";

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Executing update process...");
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Server will restart automatically if update succeeds");

    int result = system(psCommand.c_str());

    if (result == 1) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] PowerShell execution failed.");
    } else if (result == 2) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Extraction failed. Update directory missing.");
    } else if (result == 3) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Download failed. File not found.");
    } else if (result == 0) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_UPDATE, "[UPDATE] Update process completed successfully!");
    } else {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_UPDATE, "[ERROR] Unknown error during update process. Exit code: %d", result);
    }
}


}