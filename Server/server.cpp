#include "crow.h"
#include "crow/middlewares/cors.h"
#include "sqlite3.h"
#include <iostream>
#include <string>
#include <ctime>
#include <mutex>
#include <unordered_map>
#include <shared_mutex>

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

// Database helper functions
void initializeDatabase(sqlite3* db) {
    char* errMsg = nullptr;
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS posts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            html_code TEXT,
            css_code TEXT, 
            js_code TEXT,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
            updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
    )";
    
    if (sqlite3_exec(db, sql, nullptr, nullptr, &errMsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
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
    
    // Mutex for post update operations - using a map to have a separate mutex per post
    std::unordered_map<int, std::unique_ptr<std::mutex>> postMutexes;
    std::mutex mutexMapMutex; // To protect the mutex map itself
    
    // Root endpoint
    CROW_ROUTE(app, "/")([](){
        return "Codepen Style Website API";
    });
    
    // GET all posts
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
    
    // GET a specific post
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
    
    // CREATE a new post
    CROW_ROUTE(app, "/posts").methods("POST"_method)
    ([&db](const crow::request& req){
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
        
        std::cout << "Processing valid request with title: " << x["title"].s() << std::endl;
        
        sqlite3_stmt* stmt;
        const char* sql = "INSERT INTO posts (title, html_code, css_code, js_code) VALUES (?, ?, ?, ?)";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            std::string error = sqlite3_errmsg(db);
            std::cout << "SQLite prepare error: " << error << std::endl;
            return crow::response(500, error);
        }
        
        std::string title = x["title"].s();
        std::string html_code;
        std::string css_code;
        std::string js_code;
        
        if (x.has("html_code")) {
            html_code = x["html_code"].s();
        }
        
        if (x.has("css_code")) {
            css_code = x["css_code"].s();
        }
        
        if (x.has("js_code")) {
            js_code = x["js_code"].s();
        }
        
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, html_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, css_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, js_code.c_str(), -1, SQLITE_TRANSIENT);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = sqlite3_errmsg(db);
            std::cout << "SQLite execution error: " << error << std::endl;
            sqlite3_finalize(stmt);
            return crow::response(500, error);
        }
        
        int id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        
        std::cout << "Post created successfully with ID: " << id << std::endl;
        
        crow::json::wvalue result;
        result["id"] = id;
        result["message"] = "Post created successfully";
        
        auto res = crow::response(201, result);
        res.add_header("Content-Type", "application/json");
        
        std::cout << "Returning response: " << res.body << std::endl;
        return res;
    });
    
    // UPDATE a post
    CROW_ROUTE(app, "/posts/<int>").methods("PUT"_method)
    ([&db, &postMutexes, &mutexMapMutex](const crow::request& req, int id){
        auto x = crow::json::load(req.body);
        if (!x) {
            return crow::response(400, "Invalid JSON");
        }
        
        // Get or create a mutex for this post
        {
            std::lock_guard<std::mutex> mapLock(mutexMapMutex);
            if (postMutexes.find(id) == postMutexes.end()) {
                postMutexes[id] = std::make_unique<std::mutex>();
            }
        }
        
        // Now lock the specific post mutex for exclusive access
        std::lock_guard<std::mutex> postLock(*postMutexes[id]);
        
        // Check if post exists
        sqlite3_stmt* check_stmt;
        sqlite3_prepare_v2(db, "SELECT id FROM posts WHERE id = ?", -1, &check_stmt, nullptr);
        sqlite3_bind_int(check_stmt, 1, id);
        
        bool exists = sqlite3_step(check_stmt) == SQLITE_ROW;
        sqlite3_finalize(check_stmt);
        
        if (!exists) {
            return crow::response(404, "Post not found");
        }
        
        // Update the post - this is the critical section
        sqlite3_stmt* stmt;
        const char* sql = "UPDATE posts SET title = ?, html_code = ?, css_code = ?, js_code = ?, updated_at = CURRENT_TIMESTAMP WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        std::string title;
        std::string html_code;
        std::string css_code;
        std::string js_code;
        
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
        
        sqlite3_bind_text(stmt, 1, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, html_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, css_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, js_code.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 5, id);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return crow::response(500, error);
        }
        
        sqlite3_finalize(stmt);
        
        crow::json::wvalue result;
        result["message"] = "Post updated successfully";
        
        return crow::response(200, result);
    });
    
    // DELETE a post
    CROW_ROUTE(app, "/posts/<int>").methods("DELETE"_method)
    ([&db](int id){
        sqlite3_stmt* stmt;
        const char* sql = "DELETE FROM posts WHERE id = ?";
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return crow::response(500, sqlite3_errmsg(db));
        }
        
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            std::string error = sqlite3_errmsg(db);
            sqlite3_finalize(stmt);
            return crow::response(500, error);
        }
        
        sqlite3_finalize(stmt);
        
        if (sqlite3_changes(db) == 0) {
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