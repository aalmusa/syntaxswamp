#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"
#include "AuthMiddleware.h"
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
 * Transaction helper function to ensure ACID properties:
 * 
 * ACID stands for:
 * - Atomicity: All operations in a transaction either succeed completely or fail completely
 * - Consistency: Database remains in a consistent state before and after the transaction
 * - Isolation: Transactions are isolated from each other until they are committed
 * - Durability: Once a transaction is committed, it remains so even in the event of power loss or crashes
 * 
 * This function implements ACID properties by:
 * - Wrapping operations in an explicit SQL transaction (BEGIN/COMMIT/ROLLBACK)
 * - Providing atomicity by rolling back on any failure
 * - Supporting consistency through proper error handling
 * - Working with SQLite's internal mechanisms for isolation and durability
 * 
 * @param db The SQLite database connection
 * @param operation A function containing the database operations to perform inside the transaction
 * @return true if transaction completes successfully, false otherwise
 */
bool executeTransaction(sqlite3* db, const std::function<bool(sqlite3*)>& operation) {
    // Begin transaction
    if (sqlite3_exec(db, "BEGIN TRANSACTION", nullptr, nullptr, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to begin transaction: " << sqlite3_errmsg(db) << std::endl;
        return false;
    }
    
    // Execute the operation
    bool success = operation(db);
    
    // Commit or rollback based on the operation result
    if (success) {
        if (sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to commit transaction: " << sqlite3_errmsg(db) << std::endl;
            // Try to rollback if commit fails
            sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
            return false;
        }
    } else {
        if (sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr) != SQLITE_OK) {
            std::cerr << "Failed to rollback transaction: " << sqlite3_errmsg(db) << std::endl;
        }
        return false;
    }
    
    return true;
}

/**
 * Configure SQLite database settings to ensure ACID compliance
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
     * 
     * By wrapping table creation in a transaction:
     * - Either all tables are created successfully or none are (atomicity)
     * - Database schema remains consistent with complete tables (consistency)
     * - Other connections won't see partially created tables (isolation)
     * - Once committed, tables persist even after crashes (durability)
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
    
    // Mutex for post update operations - using a map to have a separate mutex per post
    std::unordered_map<int, std::unique_ptr<std::mutex>> postMutexes;
    std::mutex mutexMapMutex; // To protect the mutex map itself
    
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
    
    // GET all posts - no authentication required
    CROW_ROUTE(app, "/posts")
    ([&db](){
        crow::json::wvalue result;
        sqlite3_stmt* stmt;
        
        const char* sql = "SELECT id, title, created_at, updated_at FROM posts ORDER BY updated_at DESC";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        crow::json::wvalue::list posts;
        int index = 0;
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            crow::json::wvalue post;
            post["id"] = sqlite3_column_int(stmt, 0);
            post["title"] = (const char*)sqlite3_column_text(stmt, 1);
            post["created_at"] = (const char*)sqlite3_column_text(stmt, 2);
            post["updated_at"] = (const char*)sqlite3_column_text(stmt, 3);
            
            posts.push_back(std::move(post));
            index++;
        }
        
        sqlite3_finalize(stmt);
        result["posts"] = std::move(posts);
        
        return crow::response(result);
    });
    
    // GET a specific post - no authentication required
    CROW_ROUTE(app, "/posts/<int>")
    ([&db](int id){
        sqlite3_stmt* stmt;
        const char* sql = "SELECT id, title, html_code, css_code, js_code, created_at, updated_at FROM posts WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            crow::json::wvalue post;
            post["id"] = sqlite3_column_int(stmt, 0);
            post["title"] = (const char*)sqlite3_column_text(stmt, 1);
            post["html_code"] = (const char*)sqlite3_column_text(stmt, 2);
            post["css_code"] = (const char*)sqlite3_column_text(stmt, 3);
            post["js_code"] = (const char*)sqlite3_column_text(stmt, 4);
            post["created_at"] = (const char*)sqlite3_column_text(stmt, 5);
            post["updated_at"] = (const char*)sqlite3_column_text(stmt, 6);
            
            sqlite3_finalize(stmt);
            return crow::response(post);
        } else {
            sqlite3_finalize(stmt);
            return crow::response(404, "Post not found");
        }
    });
    
    // CREATE a new post - requires authentication - now with transaction
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
        
        if (x.has("html_code")) {
            html_code = x["html_code"].s();
        }
        
        if (x.has("css_code")) {
            css_code = x["css_code"].s();
        }
        
        if (x.has("js_code")) {
            js_code = x["js_code"].s();
        }
        
        int id = -1;
        crow::response errorResponse(500);
        
        /**
         * Create post using an atomic transaction
         * 
         * Benefits:
         * - Post creation either completely succeeds or fails (atomicity)
         * - Database constraints are maintained (consistency)
         * - Other operations cannot see partial post data (isolation)
         * - Once committed, post data persists even during crashes (durability)
         */
        bool success = executeTransaction(db, [&](sqlite3* db) -> bool {
            sqlite3_stmt* stmt;
            const char* sql = "INSERT INTO posts (user_id, title, html_code, css_code, js_code) VALUES (?, ?, ?, ?, ?)";
            
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
        result["message"] = "Post created successfully";
        
        auto res = crow::response(201, result);
        res.add_header("Content-Type", "application/json");
        
        std::cout << "Returning response: " << res.body << std::endl;
        return res;
    });
    
    // UPDATE a post - requires authentication - now with transaction
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
        
        /**
         * Mutex locking for thread-safety (enhanced isolation)
         * 
         * This two-level mutex approach:
         * 1. First acquires a lock on the mutex map to safely access/create the post-specific mutex
         * 2. Then locks the individual post mutex to prevent concurrent modifications to the same post
         * 
         * This provides application-level isolation beyond what SQLite provides internally
         */
        {
            std::lock_guard<std::mutex> mapLock(mutexMapMutex);
            if (postMutexes.find(id) == postMutexes.end()) {
                postMutexes[id] = std::make_unique<std::mutex>();
            }
        }
        
        // Now lock the specific post mutex for exclusive access
        std::lock_guard<std::mutex> postLock(*postMutexes[id]);
        
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
        
        crow::response errorResponse(500);
        
        /**
         * Execute post update as an atomic transaction
         * 
         * Combined with mutex locks, this provides:
         * - Double layer of isolation (app-level and database-level)
         * - Complete atomicity for complex update operations
         * - Consistency through enforcing authorization checks inside the transaction
         * - Durability through SQLite's transaction commit mechanism
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
                errorResponse = crow::response(403, "Forbidden - You don't have permission to modify this post");
                return false;
            }
            
            // Update the post
            sqlite3_stmt* stmt;
            const char* sql = "UPDATE posts SET title = ?, html_code = ?, css_code = ?, js_code = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?";
            
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
                errorResponse = crow::response(500, sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, html_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, css_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, js_code.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 5, id);
            
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                std::string error = sqlite3_errmsg(db);
                sqlite3_finalize(stmt);
                errorResponse = crow::response(500, error);
                return false;
            }
            
            sqlite3_finalize(stmt);
            return true;
        });
        
        if (!success) {
            return errorResponse;
        }
        
        crow::json::wvalue result;
        result["message"] = "Post updated successfully";
        
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

    //set the port, set the app to run on multiple threads, and run the app
    app.port(18080).multithreaded().run();
    
    // Close the database connection when the program ends
    sqlite3_close(db);
    return 0;
}