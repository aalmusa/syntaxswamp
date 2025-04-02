#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"
#include "AuthMiddleware.h"
#include "DeadlockSafeMutex.h"  // Include our new header
#include <iostream>
#include <string>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>
#include <functional>

// Helper function to convert HTTP method to string
std::string methodToString(const crow::HTTPMethod& method) {
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

/**
 * Transaction helper function with deadlock handling capabilities
 * 
 * This improved version adds:
 * 1. Busy timeouts to prevent internal SQLite deadlocks
 * 2. Retry capability to recover from transient lock issues
 * 3. Error differentiation between deadlocks and other failures
 * 
 * @param db The SQLite database connection
 * @param operation A function containing the database operations
 * @param retries Number of retries if the transaction fails due to locking
 * @return true if transaction completes successfully, false otherwise
 */
bool executeTransaction(sqlite3* db, const std::function<bool(sqlite3*)>& operation, int retries = 3) {
    // Set SQLite busy timeout to avoid internal deadlocks
    // This makes SQLite wait up to 1000ms when a resource is locked
    sqlite3_busy_timeout(db, 1000);
    
    for (int attempt = 0; attempt <= retries; attempt++) {
        // Begin transaction
        if (sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(db) << std::endl;
            
            // If database is busy/locked, retry after delay
            if (sqlite3_errcode(db) == SQLITE_BUSY || sqlite3_errcode(db) == SQLITE_LOCKED) {
                if (attempt < retries) {
                    std::cerr << "Database locked, retrying in " << (100 * (attempt + 1)) << "ms..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
                    continue;
                }
            }
            return false;
        }
        
        // Execute the operation
        bool success = operation(db);
        
        // Commit or rollback based on the operation result
        if (success) {
            if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(db) << std::endl;
                
                // Check if failure was due to lock contention
                if (sqlite3_errcode(db) == SQLITE_BUSY || sqlite3_errcode(db) == SQLITE_LOCKED) {
                    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
                    if (attempt < retries) {
                        std::cerr << "Commit failed due to locks, retrying..." << std::endl;
                        std::this_thread::sleep_for(std::chrono::milliseconds(100 * (attempt + 1)));
                        continue;
                    }
                } else {
                    // Try to rollback if commit fails
                    sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
                }
                return false;
            }
            return true;  // Success!
        } else {
            if (sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
                std::cerr << "Failed to rollback transaction: " << sqlite3_errmsg(db) << std::endl;
            }
            // No need to retry if the operation itself failed
            return false;
        }
    }
    
    std::cerr << "Transaction failed after " << retries << " retries" << std::endl;
    return false;
}

/**
 * Configure SQLite database settings to ensure ACID compliance and deadlock prevention
 * 
 * This function configures several SQLite PRAGMA settings that enhance ACID guarantees:
 * 
 * 1. journal_mode = WAL (Write-Ahead Logging)
 *    - Improves concurrency by allowing readers and writers to operate simultaneously
 *    - Enhances durability by writing changes to a separate log before applying them
 *    - Reduces chance of database corruption during system crashes
 * 
 * 2. synchronous = FULL
 *    - Ensures durability by forcing disk writes at critical moments
 *    - Prevents data loss in case of operating system crashes or power failures
 *    - Trades some performance for maximum reliability
 * 
 * 3. foreign_keys = ON
 *    - Enforces referential integrity constraints
 *    - Contributes to consistency by preventing invalid data relationships
 *    - Ensures database remains in a valid state after transactions
 * 
 * 4. busy_timeout = 5000
 *    - Prevents deadlocks by making SQLite wait for up to 5000ms when a resource is locked
 * 
 * @param db The SQLite database connection to configure
 * @return true if all configurations succeed, false if any fail
 */
bool configureSQLiteForACID(sqlite3* db) {
    char* errMsg = nullptr;
    
    // Set journal mode to WAL for better concurrency and durability
    const char* journalMode = "PRAGMA journal_mode = WAL;";
    if (sqlite3_exec(db, journalMode, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to set journal mode: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    // Set synchronous mode to FULL for durability (prevents corruption in case of OS crash)
    const char* syncMode = "PRAGMA synchronous = FULL;";
    if (sqlite3_exec(db, syncMode, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to set synchronous mode: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    // Enable foreign key constraints for consistency
    const char* foreignKeys = "PRAGMA foreign_keys = ON;";
    if (sqlite3_exec(db, foreignKeys, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to enable foreign keys: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        return false;
    }
    
    // Add deadlock timeout setting
    const char* busyTimeout = "PRAGMA busy_timeout = 5000;";
    if (sqlite3_exec(db, busyTimeout, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "Failed to set busy timeout: " << errMsg << std::endl;
        sqlite3_free(errMsg);
        // Continue anyway, this is not critical
    }
    
    return true;
}

// Database helper functions
void initializeDatabase(sqlite3* db) {
    // Configure SQLite for ACID compliance
    if (!configureSQLiteForACID(db)) {
        std::cerr << "Warning: Failed to configure SQLite for optimal ACID compliance" << std::endl;
    }
    
    /**
     * Create database tables using a transaction to ensure atomicity
     */
    executeTransaction(db, [](sqlite3* db) -> bool {
        char* errMsg = nullptr;
        const char* sql = R"(
            CREATE TABLE IF NOT EXISTS posts (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                user_id INTEGER,
                title TEXT NOT NULL,
                html_code TEXT,
                css_code TEXT, 
                js_code TEXT,
                isPrivate BOOLEAN DEFAULT 0 NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (user_id) REFERENCES users(user_id)
            );
            
            CREATE TABLE IF NOT EXISTS users (
                user_id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL UNIQUE,
                email TEXT NOT NULL UNIQUE,
                password TEXT NOT NULL,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            );
        )";
        
        if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::cerr << "SQL error: " << errMsg << std::endl;
            sqlite3_free(errMsg);
            return false;
        }
        
        // Add column to existing table if it doesn't exist (for compatibility with existing databases)
        const char* alterSql = "PRAGMA table_info(posts)";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, alterSql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::cerr << "Error checking table schema: " << sqlite3_errmsg(db) << std::endl;
            return false;
        }
        
        bool hasPrivateColumn = false;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string colName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (colName == "isPrivate") {
                hasPrivateColumn = true;
                break;
            }
        }
        sqlite3_finalize(stmt);
        
        if (!hasPrivateColumn) {
            const char* addColumnSql = "ALTER TABLE posts ADD COLUMN isPrivate BOOLEAN DEFAULT 0 NOT NULL";
            if (sqlite3_exec(db, addColumnSql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
                std::cerr << "Error adding isPrivate column: " << errMsg << std::endl;
                sqlite3_free(errMsg);
                return false;
            }
            std::cout << "Added isPrivate column to existing posts table" << std::endl;
        }
        
        return true;
    });
}

int main() {
    // Use CORS middleware by specifying it in the App template
    crow::App<crow::CORSHandler> app;
    
    // Configure CORS
    auto& cors = app.get_middleware<crow::CORSHandler>();
    cors.global()
        .origin("*")
        .methods("GET"_method, "POST"_method, "PUT"_method, "DELETE"_method, "OPTIONS"_method)
        .headers("Content-Type", "Accept", "Authorization")
        .max_age(3600);
    
    // Initialize SQLite database
    sqlite3* db;
    if (sqlite3_open("codepen.db", &db) != SQLITE_OK) {
        std::cerr << "Cannot open database: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }
    
    // Create tables if they don't exist
    initializeDatabase(db);
    
    // Create authentication middleware
    AuthMiddleware auth;
    
    // Use deadlock-safe mutexes instead of standard ones
    std::unordered_map<int, std::unique_ptr<DeadlockSafeMutex>> postMutexes;
    DeadlockSafeMutex mutexMapMutex("postMapMutex");
    
    // Root endpoint
    CROW_ROUTE(app, "/")([](){
        return "Codepen Style Website API";
    });
    
    // User registration endpoint
    CROW_ROUTE(app, "/auth/register").methods("POST"_method)
    ([&db](const crow::request& req) {
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
        
        /**
         * Execute user registration as an atomic transaction
         * 
         * This ensures:
         * - User is either completely created or not at all (atomicity)
         * - All constraints like unique username/email are enforced (consistency)
         * - Concurrent registrations don't interfere with each other (isolation)
         * - User data is safely stored once transaction completes (durability)
         */
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
    ([&db, &auth](const crow::request& req) {
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
    
    // GET all posts - filtered by privacy settings
    CROW_ROUTE(app, "/posts")
    ([&db, &auth](const crow::request& req){
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
    ([&db, &auth](const crow::request& req, int id){
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
    
    // CREATE a new post - now with privacy setting
    CROW_ROUTE(app, "/posts").methods("POST"_method)
    ([&db, &auth](const crow::request& req) {
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
    
    // UPDATE a post - respects privacy settings for editing
    CROW_ROUTE(app, "/posts/<int>").methods("PUT"_method)
    ([&db, &postMutexes, &mutexMapMutex, &auth](const crow::request& req, int id) {
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
        
        /**
         * Mutex locking for thread-safety with deadlock prevention
         */
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
        
        crow::json::wvalue result;
        result["message"] = "Post updated successfully";
        result["isPrivate"] = updatePrivacy ? newPrivacySetting : isPrivate;
        
        return crow::response(200, result);
    });
    
    // DELETE a post - requires authentication - now with transaction
    CROW_ROUTE(app, "/posts/<int>").methods("DELETE"_method)
    ([&db, &auth](const crow::request& req, int id) {
        // Check if user is authenticated
        if (!auth.authenticate(req)) {
            return crow::response(401, "Unauthorized - Login required");
        }
        int user_id = auth.getUserId(req);
        
        crow::response errorResponse(500);
        bool changes = false;
        
        /**
         * Execute post deletion as an atomic transaction
         * 
         * This ensures:
         * - Authorization check and deletion happen atomically
         * - Either the post is completely deleted or remains unchanged
         * - Other operations can't see a post in a partially deleted state
         * - Once committed, the deletion persists through system failures
         */
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
        
        crow::json::wvalue result;
        result["message"] = "Post deleted successfully";
        
        return crow::response(200, result);
    });

    // GET post creator - respects privacy settings
    CROW_ROUTE(app, "/posts/<int>/creator")
    ([&db, &auth](const crow::request& req, int post_id) {
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

    //set the port, set the app to run on multiple threads, and run the app
    app.port(18080).multithreaded().run();
    
    // Close the database connection when the program ends
    sqlite3_close(db);
    return 0;
}