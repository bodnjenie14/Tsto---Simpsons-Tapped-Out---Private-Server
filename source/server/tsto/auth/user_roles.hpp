#pragma once

#include <string>
#include <vector>
#include <tuple>

namespace tsto {

    // Role types
    enum class RoleType {
        ADMIN,
        TOWN_OPERATOR,
        GAME_MODERATOR,
        CONTENT_MANAGER,
        VIEWER,
        UNKNOWN
    };

    // User role class
    class UserRole {
    public:
        // Initialize the database
        static bool Initialize();

        // Get role by username
        static RoleType GetRoleByUsername(const std::string& username);

        // Authenticate user
        static bool AuthenticateUser(const std::string& username, const std::string& password);

        // Create or update user
        static bool CreateOrUpdateUser(const std::string& username, const std::string& password, RoleType role);

        // Delete user
        static bool DeleteUser(const std::string& username);

        // Change password
        static bool ChangePassword(const std::string& username, const std::string& new_password);
        
        // Rename user (change username)
        static bool RenameUser(const std::string& old_username, const std::string& new_username);

        // Get all users
        static std::vector<std::tuple<std::string, RoleType>> GetAllUsers();
        
        // Session management functions
        // Create a new session for a user
        static std::string CreateSession(const std::string& username, const std::string& ip_address, const std::string& user_agent, int expiry_hours = 24);
        
        // Validate a session token
        static bool ValidateSession(const std::string& token, std::string& username);
        
        // Invalidate a session (logout)
        static bool InvalidateSession(const std::string& token);
        
        // Clean up expired sessions
        static void CleanupExpiredSessions();

        // Role type to string
        static std::string RoleTypeToString(RoleType role);

        // String to role type
        static RoleType StringToRoleType(const std::string& role_str);
        
        // Town upload statistics
        // Record a town upload for a user
        static bool RecordTownUpload(const std::string& username);
        
        // Get town upload statistics for all users
        static std::vector<std::tuple<std::string, int, std::string>> GetTownUploadStats();
        
        // Clear all town upload statistics
        static bool ClearTownUploadStats();

    private:
        // Database path
        static const std::string DB_PATH;
    };

}
