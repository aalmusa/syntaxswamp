#pragma once
#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"
#include "AuthMiddleware.h"
#include "DeadlockSafeMutex.h"
#include "PostLockSystem.h"
#include "DatabaseUtils.h"
#include <iostream>
#include <unordered_map>
#include <memory>
#include <string>

// Helper function to convert HTTP method to string
inline std::string methodToString(const crow::HTTPMethod& method) {
    switch(method) {
        case crow::HTTPMethod::Get: return "GET";
        case crow::HTTPMethod::Post: return "POST";
        case crow::HTTPMethod::Put: return "PUT";
        case crow::HTTPMethod::Delete: return "DELETE";
        case crow::HTTPMethod::Head: return "HEAD";
        case crow::HTTPMethod::Options: return "OPTIONS";
        case crow::HTTPMethod::Connect: return "CONNECT";
        case crow::HTTPMethod::Trace: return "TRACE";
        case crow::HTTPMethod::Patch: return "PATCH";
        default: return "UNKNOWN";
    }
}

// Setup authentication routes
inline void setupAuthRoutes(
    crow::App<crow::CORSHandler>& app,
    sqlite3* db,
    AuthMiddleware& auth
) {
    // User registration endpoint
    CROW_ROUTE(app, "/auth/register").methods("POST"_method)
    ([db](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }
        
        if (!x.has("username") || !x.has("email") || !x.has("password")) {
            return crow::response(400, "Missing required fields");
        }
        
        std::string username = x["username"].s();
        std::string email = x["email"].s();
        std::string password = x["password"].s();
        
        int user_id = -1;
        crow::response errorResponse(500);
        
        bool success = executeTransaction(db, [&](sqlite3* db) -> bool {
            sqlite3_stmt* stmt;
            const char* sql = "INSERT INTO users (username, email, password) VALUES (?, ?, ?)";
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                errorResponse = crow::response(500, sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, password.c_str(), -1, SQLITE_TRANSIENT);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string error = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                
                if (error.find("UNIQUE constraint failed") != std::string::npos) {
                    errorResponse = crow::response(409, "Username or email already exists");
                } else {
                    errorResponse = crow::response(500, error);
                }
                return false;
            }
            
            user_id = sqlite3_last_insert_rowid(db);
            sqlite3_finalize(stmt);
            return true;
        });
        
        if (!success) {
            return errorResponse;
        }
        
        crow::json::wvalue result;
        result["user_id"] = user_id;
        result["message"] = "User registered successfully";
        
        return crow::response(201, result);
    });
    
    // User login endpoint
    CROW_ROUTE(app, "/auth/login").methods("POST"_method)
    ([db, &auth](const crow::request& req) {
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }
        
        if (!x.has("username") || !x.has("password")) {
            return crow::response(400, "Missing username or password");
        }
        
        std::string username = x["username"].s();
        std::string password = x["password"].s();
        
        sqlite3_stmt* stmt;
        const char* sql = "SELECT user_id, password FROM users WHERE username = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) != SQLITE_ROW) {
            sqlite3_finalize(stmt);
            return crow::response(401, "Invalid username or password");
        }
        
        int user_id = sqlite3_column_int(stmt, 0);
        std::string stored_password = (const char*)sqlite3_column_text(stmt, 1);
        
        sqlite3_finalize(stmt);
        
        if (password != stored_password) {
            return crow::response(401, "Invalid username or password");
        }
        
        // Generate authentication token
        std::string token = auth.generateToken(user_id);
        
        crow::json::wvalue result;
        result["token"] = token;
        result["user_id"] = user_id;
        result["username"] = username;
        
        return crow::response(200, result);
    });
}

// Setup post routes
inline void setupPostRoutes(
    crow::App<crow::CORSHandler>& app,
    sqlite3* db,
    AuthMiddleware& auth,
    std::unordered_map<int, std::unique_ptr<DeadlockSafeMutex>>& postMutexes,
    DeadlockSafeMutex& mutexMapMutex,
    std::unordered_map<int, PostLock>& postLocks,
    DeadlockSafeMutex& locksMapMutex
) {
    // GET all posts - filtered by privacy settings
    CROW_ROUTE(app, "/posts")
    ([db, &auth](const crow::request& req){
        crow::json::wvalue result;
        sqlite3_stmt* stmt;
        
        // Check if user is authenticated
        int user_id = auth.authenticate(req) ? auth.getUserId(req) : -1;
        
        const char* sql;
        if (user_id != -1) {
            // Authenticated user: show their private posts + all public posts
            sql = "SELECT id, user_id, title, created_at, updated_at, isPrivate FROM posts WHERE (isPrivate = 0 OR user_id = ?) ORDER BY updated_at DESC";
        } else {
            // Unauthenticated user: show only public posts
            sql = "SELECT id, user_id, title, created_at, updated_at, isPrivate FROM posts WHERE isPrivate = 0 ORDER BY updated_at DESC";
        }
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        // Bind user_id parameter if authenticated
        if (user_id != -1) {
            sqlite3_bind_int(stmt, 1, user_id);
        }
        
        crow::json::wvalue::list posts;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            crow::json::wvalue post;
            post["id"] = sqlite3_column_int(stmt, 0);
            post["user_id"] = sqlite3_column_int(stmt, 1);
            post["title"] = (const char*)sqlite3_column_text(stmt, 2);
            post["created_at"] = (const char*)sqlite3_column_text(stmt, 3);
            post["updated_at"] = (const char*)sqlite3_column_text(stmt, 4);
            post["isPrivate"] = sqlite3_column_int(stmt, 5) != 0;
            
            posts.push_back(std::move(post));
        }
        
        sqlite3_finalize(stmt);
        result["posts"] = std::move(posts);
        
        return crow::response(result);
    });
    
    // GET a specific post - checks privacy settings
    CROW_ROUTE(app, "/posts/<int>")
    ([db, &auth](const crow::request& req, int id){
        // Check if user is authenticated
        int user_id = auth.authenticate(req) ? auth.getUserId(req) : -1;
        
        // Add debugging to track authentication issues
        std::cout << "Fetching post " << id << ", authenticated user_id: " << user_id << std::endl;
        
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id, user_id, title, html_code, css_code, js_code, created_at, updated_at, isPrivate FROM posts WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            int post_user_id = sqlite3_column_int(stmt, 1);
            bool isPrivate = sqlite3_column_int(stmt, 8) != 0;
            
            // Debug log for privacy check
            std::cout << "Post " << id << " belongs to user " << post_user_id 
                      << ", isPrivate: " << (isPrivate ? "true" : "false") << std::endl;
            
            // Check privacy: if private, only creator can view
            if (isPrivate && post_user_id != user_id) {
                sqlite3_finalize(stmt);
                return crow::response(403, "This post is private");
            }
            
            crow::json::wvalue post;
            post["id"] = sqlite3_column_int(stmt, 0);
            post["user_id"] = post_user_id;
            post["title"] = (const char*)sqlite3_column_text(stmt, 2);
            post["html_code"] = (const char*)sqlite3_column_text(stmt, 3);
            post["css_code"] = (const char*)sqlite3_column_text(stmt, 4);
            post["js_code"] = (const char*)sqlite3_column_text(stmt, 5);
            post["created_at"] = (const char*)sqlite3_column_text(stmt, 6);
            post["updated_at"] = (const char*)sqlite3_column_text(stmt, 7);
            post["isPrivate"] = isPrivate;
            
            sqlite3_finalize(stmt);
            return crow::response(post);
        } else {
            sqlite3_finalize(stmt);
            return crow::response(404, "Post not found");
        }
    });
    
    // CREATE a new post - with privacy setting
    CROW_ROUTE(app, "/posts").methods("POST"_method)
    ([db, &auth](const crow::request& req) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        std::cout << "Received POST request to /posts" << std::endl;
        std::cout << "Request body: " << req.body << std::endl;
        
        auto x = crow::json::load(req.body);
        if (!x) {
            std::cout << "Invalid JSON received" << std::endl;
            return crow::response(400, "Invalid JSON");
        }
        
        if (!x.has("title")) {
            std::cout << "Missing title field in request" << std::endl;
            return crow::response(400, "Missing title field");
        }
        
        std::string title = x["title"].s();
        std::string html_code = "";
        std::string css_code = "";
        std::string js_code = "";
        bool isPrivate = false;  // Default to public post
        
        if (x.has("html_code")) {
            html_code = x["html_code"].s();
        }
        
        if (x.has("css_code")) {
            css_code = x["css_code"].s();
        }
        
        if (x.has("js_code")) {
            js_code = x["js_code"].s();
        }
        
        if (x.has("isPrivate")) {
            isPrivate = x["isPrivate"].b();
        }
        
        int id = -1;
        crow::response errorResponse(500);
        
        bool success = executeTransaction(db, [&](sqlite3* db) -> bool {
            sqlite3_stmt* stmt;
            const char* sql = "INSERT INTO posts (user_id, title, html_code, css_code, js_code, isPrivate) VALUES (?, ?, ?, ?, ?, ?)";
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                std::string error = sqlite3_errmsg(db);
                std::cout << "SQLite prepare error: " << error << std::endl;
                errorResponse = crow::response(500, error);
                return false;
            }
            
            sqlite3_bind_int(stmt, 1, user_id);
            sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, html_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, css_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, js_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 6, isPrivate ? 1 : 0);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string error = sqlite3_errmsg(db);
                std::cout << "SQLite execution error: " << error << std::endl;
                sqlite3_finalize(stmt);
                errorResponse = crow::response(500, error);
                return false;
            }
            
            id = sqlite3_last_insert_rowid(db);
            sqlite3_finalize(stmt);
            return true;
        });
        
        if (!success) {
            return errorResponse;
        }
        
        std::cout << "Post created successfully with ID: " << id << std::endl;
        
        crow::json::wvalue result;
        result["id"] = id;
        result["isPrivate"] = isPrivate;
        result["message"] = "Post created successfully";
        
        auto res = crow::response(201, result);
        res.add_header("Content-Type", "application/json");
        
        std::cout << "Returning response: " << res.body << std::endl;
        return res;
    });
    
    // UPDATE a post - respects privacy settings and releases locks
    CROW_ROUTE(app, "/posts/<int>").methods("PUT"_method)
    ([db, &postMutexes, &mutexMapMutex, &postLocks, &locksMapMutex, &auth](const crow::request& req, int id) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }
        
        // Get post privacy status and ownership before acquiring locks
        sqlite3_stmt* privacy_stmt;
        int post_owner_id = -1;
        bool isPrivate = false;
        
        const char* privacy_sql = "SELECT user_id, isPrivate FROM posts WHERE id = ?";
        if (sqlite3_prepare_v2(db, privacy_sql, -1, &privacy_stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(privacy_stmt, 1, id);
        
        if (sqlite3_step(privacy_stmt) == SQLITE_ROW) {
            post_owner_id = sqlite3_column_int(privacy_stmt, 0);
            isPrivate = sqlite3_column_int(privacy_stmt, 1) != 0;
        } else {
            sqlite3_finalize(privacy_stmt);
            return crow::response(404, "Post not found");
        }
        
        sqlite3_finalize(privacy_stmt);
        
        // If post is private, only the owner can edit it
        if (isPrivate && user_id != post_owner_id) {
            return crow::response(403, "You don't have permission to edit this private post");
        }
        
        // Mutex locking for thread-safety with deadlock prevention
        DeadlockSafeMutex* postMutex = nullptr;
        
        // Phase 1: Get the post-specific mutex
        if (!mutexMapMutex.tryLockWithTimeout(500)) {
            return crow::response(503, "Server busy, please try again later");
        }
        
        try {
            // Create the mutex if it doesn't exist
            if (postMutexes.find(id) == postMutexes.end()) {
                postMutexes[id] = std::make_unique<DeadlockSafeMutex>("post_" + std::to_string(id));
            }
            postMutex = postMutexes[id].get();
            mutexMapMutex.unlock();  // Release map mutex before acquiring post mutex
        }
        catch (...) {
            mutexMapMutex.unlock();  // Ensure we release the mutex even if an exception occurs
            return crow::response(500, "Internal server error");
        }
        
        // Phase 2: Lock the post-specific mutex
        if (!postMutex->tryLockWithTimeout(1000)) {
            return crow::response(503, "Post is being edited by another user, please try again later");
        }
        
        // Extract data from request
        std::string title = "";
        std::string html_code = "";
        std::string css_code = "";
        std::string js_code = "";
        
        if (x.has("title")) {
            title = x["title"].s();
        }
        
        if (x.has("html_code")) {
            html_code = x["html_code"].s();
        }
        
        if (x.has("css_code")) {
            css_code = x["css_code"].s();
        }
        
        if (x.has("js_code")) {
            js_code = x["js_code"].s();
        }
        
        // Allow updating privacy setting if user is the owner
        bool updatePrivacy = false;
        bool newPrivacySetting = isPrivate;
        
        if (x.has("isPrivate") && user_id == post_owner_id) {
            updatePrivacy = true;
            newPrivacySetting = x["isPrivate"].b();
        }
        
        crow::response errorResponse(500);
        
        bool success = executeTransaction(db, [&](sqlite3* db) -> bool {
            // Update the post (privacy check already done)
            sqlite3_stmt* stmt;
            const char* sql;
            
            if (updatePrivacy) {
                sql = "UPDATE posts SET title = ?, html_code = ?, css_code = ?, js_code = ?, isPrivate = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?";
            } else {
                sql = "UPDATE posts SET title = ?, html_code = ?, css_code = ?, js_code = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?";
            }
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                errorResponse = crow::response(500, sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, html_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, css_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, js_code.c_str(), -1, SQLITE_TRANSIENT);
            
            if (updatePrivacy) {
                sqlite3_bind_int(stmt, 5, newPrivacySetting ? 1 : 0);
                sqlite3_bind_int(stmt, 6, id);
            } else {
                sqlite3_bind_int(stmt, 5, id);
            }
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string error = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                errorResponse = crow::response(500, error);
                return false;
            }
            
            sqlite3_finalize(stmt);
            return true;
        }, 3);  // Allow up to 3 retries
        
        // Release the post mutex
        postMutex->unlock();
        
        if (!success) {
            return errorResponse;
        }
        
        // After successful update, release any lock the user holds on this post
        if (locksMapMutex.tryLockWithTimeout(500)) {
            try {
                auto it = postLocks.find(id);
                if (it != postLocks.end() && it->second.user_id == user_id) {
                    // Remove the lock since user has completed their edit
                    postLocks.erase(it);
                    std::cout << "Released lock on post " << id << " after successful update" << std::endl;
                }
                locksMapMutex.unlock();
            }
            catch (...) {
                locksMapMutex.unlock();
                std::cerr << "Error while releasing post lock" << std::endl;
                // Don't fail the response just because of a lock issue
            }
        }
        
        crow::json::wvalue result;
        result["message"] = "Post updated successfully";
        result["isPrivate"] = updatePrivacy ? newPrivacySetting : isPrivate;
        result["lock_released"] = true;
        
        return crow::response(200, result);
    });
    
    // DELETE a post - requires authentication
    CROW_ROUTE(app, "/posts/<int>").methods("DELETE"_method)
    ([db, &auth, &postLocks, &locksMapMutex](const crow::request& req, int id) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        crow::response errorResponse(500);
        bool changes = false;
        
        bool success = executeTransaction(db, [&](sqlite3* db) -> bool {
            // Check if post exists and belongs to the authenticated user
            sqlite3_stmt* check_stmt;
            const char* check_sql = "SELECT id FROM posts WHERE id = ? AND user_id = ?";
            
            if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr) != SQLITE_OK) {
                errorResponse = crow::response(500, sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_int(check_stmt, 1, id);
            sqlite3_bind_int(check_stmt, 2, user_id);
            
            bool authorized = sqlite3_step(check_stmt) == SQLITE_ROW;
            sqlite3_finalize(check_stmt);
            
            if (!authorized) {
                errorResponse = crow::response(403, "Forbidden - You don't have permission to delete this post");
                return false;
            }
            
            // Delete the post
            sqlite3_stmt* stmt;
            const char* sql = "DELETE FROM posts WHERE id = ?";
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                errorResponse = crow::response(500, sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_int(stmt, 1, id);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string error = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                errorResponse = crow::response(500, error);
                return false;
            }
            
            sqlite3_finalize(stmt);
            changes = sqlite3_changes(db) > 0;
            
            return true;
        });
        
        if (!success) {
            return errorResponse;
        }
        
        if (!changes) {
            return crow::response(404, "Post not found");
        }
        
        // Also clean up any locks for this post
        if (locksMapMutex.tryLockWithTimeout(500)) {
            auto it = postLocks.find(id);
            if (it != postLocks.end()) {
                postLocks.erase(it);
                std::cout << "Removed lock for deleted post " << id << std::endl;
            }
            locksMapMutex.unlock();
        }
        
        crow::json::wvalue result;
        result["message"] = "Post deleted successfully";
        
        return crow::response(200, result);
    });

    // GET post creator - respects privacy settings
    CROW_ROUTE(app, "/posts/<int>/creator")
    ([db, &auth](const crow::request& req, int post_id) {
        // Use our transaction helper to ensure ACID properties
        crow::response errorResponse(500);
        crow::json::wvalue result;
        bool success = false;
        
        // Check if user is authenticated
        int user_id = auth.authenticate(req) ? auth.getUserId(req) : -1;
        
        // First check if the post is private
        sqlite3_stmt* privacy_stmt;
        const char* privacy_sql = "SELECT user_id, isPrivate FROM posts WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, privacy_sql, -1, &privacy_stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(privacy_stmt, 1, post_id);
        
        int post_user_id = -1;
        bool isPrivate = false;
        
        if (sqlite3_step(privacy_stmt) == SQLITE_ROW) {
            post_user_id = sqlite3_column_int(privacy_stmt, 0);
            isPrivate = sqlite3_column_int(privacy_stmt, 1) != 0;
        } else {
            sqlite3_finalize(privacy_stmt);
            return crow::response(404, "Post not found");
        }
        
        sqlite3_finalize(privacy_stmt);
        
        // If post is private and current user is not the owner, deny access
        if (isPrivate && post_user_id != user_id) {
            return crow::response(403, "This post is private");
        }
        
        // Query to get post creator information
        sqlite3_stmt* stmt;
        const char* sql = 
            "SELECT u.user_id, u.username, u.email, p.id, p.isPrivate "
            "FROM users u "
            "JOIN posts p ON u.user_id = p.user_id "
            "WHERE p.id = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            errorResponse = crow::response(500, "Database error: " + std::string(sqlite3_errmsg(db)));
            return errorResponse;
        }
        
        sqlite3_bind_int(stmt, 1, post_id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // Post found, extract creator information
            result["user_id"] = sqlite3_column_int(stmt, 0);
            
            // Safely handle NULL values in the query results
            const unsigned char* username = sqlite3_column_text(stmt, 1);
            if (username) result["username"] = std::string(reinterpret_cast<const char*>(username));
            
            // Only include email if user is the creator or an admin
            if (user_id == sqlite3_column_int(stmt, 0)) {
                const unsigned char* email = sqlite3_column_text(stmt, 2);
                if (email) result["email"] = std::string(reinterpret_cast<const char*>(email));
            }
            
            // Add the post_id and privacy for reference
            result["post_id"] = sqlite3_column_int(stmt, 3);
            result["isPrivate"] = sqlite3_column_int(stmt, 4) != 0;
            
            success = true;
        }
        
        sqlite3_finalize(stmt);
        
        if (!success) {
            return crow::response(404, "Post not found or has no creator");
        }
        
        return crow::response(result);
    });
}

// Setup post lock routes
inline void setupPostLockRoutes(
    crow::App<crow::CORSHandler>& app,
    sqlite3* db,
    AuthMiddleware& auth,
    std::unordered_map<int, PostLock>& postLocks,
    DeadlockSafeMutex& locksMapMutex
) {
    // ACQUIRE a lock on a post for editing
    CROW_ROUTE(app, "/posts/<int>/lock").methods("POST"_method)
    ([db, &postLocks, &locksMapMutex, &auth](const crow::request& req, int post_id) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        // Parse request body for custom lock duration (optional)
        int lock_duration = DEFAULT_LOCK_DURATION;
        auto body = crow::json::load(req.body);
        if (body && body.has("duration")) {
            lock_duration = body["duration"].i();
            // Cap the duration to prevent excessive locks
            lock_duration = std::min(lock_duration, 3600); // Max 1 hour
        }
        
        // Get username for the lock
        std::string username = "Unknown User";
        sqlite3_stmt* name_stmt;
        const char* name_sql = "SELECT username FROM users WHERE user_id = ?";
        
        if (sqlite3_prepare_v2(db, name_sql, -1, &name_stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(name_stmt, 1, user_id);
            
            if (sqlite3_step(name_stmt) == SQLITE_ROW) {
                username = (const char*)sqlite3_column_text(name_stmt, 0);
            }
            sqlite3_finalize(name_stmt);
        }
        
        // Check if post exists and respect privacy settings
        sqlite3_stmt* post_stmt;
        const char* post_sql = "SELECT user_id, isPrivate FROM posts WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, post_sql, -1, &post_stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(post_stmt, 1, post_id);
        
        if (sqlite3_step(post_stmt) != SQLITE_ROW) {
            sqlite3_finalize(post_stmt);
            return crow::response(404, "Post not found");
        }
        
        int post_owner_id = sqlite3_column_int(post_stmt, 0);
        bool isPrivate = sqlite3_column_int(post_stmt, 1) != 0;
        sqlite3_finalize(post_stmt);
        
        // Check privacy - private posts can only be edited by owner
        if (isPrivate && post_owner_id != user_id) {
            return crow::response(403, "Cannot acquire lock on a private post you don't own");
        }
        
        // Try to acquire the lock
        if (!locksMapMutex.tryLockWithTimeout(500)) {
            return crow::response(503, "Server busy, please try again later");
        }
        
        crow::json::wvalue result;
        crow::response response(200);
        
        try {
            auto now = std::chrono::system_clock::now();
            
            // Check if the post is already locked
            auto lock_it = postLocks.find(post_id);
            if (lock_it != postLocks.end()) {
                // If lock exists, check if it belongs to current user or has expired
                if (lock_it->second.user_id == user_id) {
                    // User already has the lock, just extend it
                    lock_it->second.expires_at = now + std::chrono::seconds(lock_duration);
                    result["message"] = "Lock extended";
                    result["expires_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                        lock_it->second.expires_at.time_since_epoch()).count();
                    result["lock_holder"] = username;
                    result["seconds_remaining"] = lock_duration;
                } 
                else if (lock_it->second.expires_at <= now) {
                    // Lock expired, assign to current user
                    lock_it->second.user_id = user_id;
                    lock_it->second.expires_at = now + std::chrono::seconds(lock_duration);
                    lock_it->second.username = username;
                    result["message"] = "Lock acquired (previous lock expired)";
                    result["expires_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                        lock_it->second.expires_at.time_since_epoch()).count();
                    result["lock_holder"] = username;
                    result["seconds_remaining"] = lock_duration;
                } 
                else {
                    // Lock is still valid and belongs to another user
                    result["message"] = "Post is currently being edited by another user";
                    result["lock_holder"] = lock_it->second.username;
                    
                    auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                        lock_it->second.expires_at - now).count();
                    result["seconds_remaining"] = remaining;
                    
                    response.code = 423; // Locked (HTTP status code)
                }
            } 
            else {
                // No lock exists, create new lock
                postLocks[post_id] = {
                    user_id,
                    now + std::chrono::seconds(lock_duration),
                    username
                };
                
                result["message"] = "Lock acquired successfully";
                result["expires_at"] = std::chrono::duration_cast<std::chrono::seconds>(
                    postLocks[post_id].expires_at.time_since_epoch()).count();
                result["lock_holder"] = username;
                result["seconds_remaining"] = lock_duration;
            }
            
            locksMapMutex.unlock();
            response.body = result.dump();
            return response;
        }
        catch (...) {
            locksMapMutex.unlock();
            return crow::response(500, "Internal server error while processing lock");
        }
    });
    
    // RELEASE a lock on a post (explicit release)
    CROW_ROUTE(app, "/posts/<int>/lock").methods("DELETE"_method)
    ([&postLocks, &locksMapMutex, &auth](const crow::request& req, int post_id) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        if (!locksMapMutex.tryLockWithTimeout(500)) {
            return crow::response(503, "Server busy, please try again later");
        }
        
        try {
            // Check if the post is locked
            auto lock_it = postLocks.find(post_id);
            if (lock_it == postLocks.end()) {
                locksMapMutex.unlock();
                return crow::response(404, "No lock found for this post");
            }
            
            // Only the lock owner can release the lock
            if (lock_it->second.user_id != user_id) {
                locksMapMutex.unlock();
                return crow::response(403, "You don't have permission to release this lock");
            }
            
            // Remove the lock
            postLocks.erase(lock_it);
            locksMapMutex.unlock();
            
            crow::json::wvalue result;
            result["message"] = "Lock released successfully";
            return crow::response(200, result);
        }
        catch (...) {
            locksMapMutex.unlock();
            return crow::response(500, "Internal server error while processing lock release");
        }
    });
    
    // CHECK lock status on a post
    CROW_ROUTE(app, "/posts/<int>/lock").methods("GET"_method)
    ([&postLocks, &locksMapMutex, &auth](const crow::request& req, int post_id) {
        // No auth required to check lock status
        
        if (!locksMapMutex.tryLockWithTimeout(500)) {
            return crow::response(503, "Server busy, please try again later");
        }
        
        try {
            auto now = std::chrono::system_clock::now();
            
            // Check if the post is locked
            auto lock_it = postLocks.find(post_id);
            crow::json::wvalue result;
            
            if (lock_it != postLocks.end() && lock_it->second.expires_at > now) {
                // Valid lock exists
                result["locked"] = true;
                result["user_id"] = lock_it->second.user_id;
                result["username"] = lock_it->second.username;
                
                auto remaining = std::chrono::duration_cast<std::chrono::seconds>(
                    lock_it->second.expires_at - now).count();
                result["seconds_remaining"] = remaining;
                
                // Check if requesting user is the lock holder
                int user_id = auth.authenticate(req) ? auth.getUserId(req) : -1;
                result["is_lock_holder"] = (user_id == lock_it->second.user_id);
            } 
            else {
                // No valid lock
                if (lock_it != postLocks.end()) {
                    // Lock exists but expired, clean it up
                    postLocks.erase(lock_it);
                }
                result["locked"] = false;
            }
            
            locksMapMutex.unlock();
            return crow::response(200, result);
        }
        catch (...) {
            locksMapMutex.unlock();
            return crow::response(500, "Internal server error while checking lock status");
        }
    });
}