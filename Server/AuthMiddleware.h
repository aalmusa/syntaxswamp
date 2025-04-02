#pragma once

#include "crow.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <ctime>

/**
 * @class AuthMiddleware
 * @brief Simple token-based authentication middleware for the API
 * 
 * This class provides basic authentication functionality through token management.
 * It allows endpoints to verify user identity and restrict access to authenticated users.
 * 
 */
class AuthMiddleware {
private:
    // Maps authentication tokens to user IDs
    std::unordered_map<std::string, int> userTokens;
    
    // Mutex to protect concurrent access to the tokens map
    std::mutex tokenMutex;

public:
    /**
     * Verifies if the request has valid authentication
     * 
     * Checks for the presence of a valid Bearer token in the Authorization header
     * and verifies it against stored tokens.
     * 
     * @param req The HTTP request to authenticate
     * @return true if the request has valid authentication, false otherwise
     */
    bool authenticate(const crow::request& req) {
        // Extract the Authorization header
        std::string authHeader = req.get_header_value("Authorization");
        
        // Check if header exists and has the Bearer prefix
        if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
            return false;
        }
        
        // Extract the token
        std::string token = authHeader.substr(7);
        
        // Check if token exists in our map
        std::lock_guard<std::mutex> lock(tokenMutex);
        return userTokens.find(token) != userTokens.end();
    }
    
    /**
     * Gets the user ID associated with the token in the request
     * 
     * @param req The HTTP request containing the authentication token
     * @return The user ID if valid token, -1 otherwise
     */
    int getUserId(const crow::request& req) {
        std::string authHeader = req.get_header_value("Authorization");
        if (authHeader.empty() || authHeader.substr(0, 7) != "Bearer ") {
            return -1;
        }
        
        std::string token = authHeader.substr(7);
        std::lock_guard<std::mutex> lock(tokenMutex);
        auto it = userTokens.find(token);
        return (it != userTokens.end()) ? it->second : -1;
    }
    
    /**
     * Generates a new authentication token for a user
     * 
     * Creates a simple token based on timestamp and user ID.
     * 
     * @param userId The user ID to associate with the token
     * @return The generated token string
     */
    std::string generateToken(int userId) {
        // Simple token generation - timestamp + user ID
        std::string token = std::to_string(std::time(nullptr)) + "_" + std::to_string(userId);
        
        // Store the token in our map
        std::lock_guard<std::mutex> lock(tokenMutex);
        userTokens[token] = userId;
        return token;
    }
};
