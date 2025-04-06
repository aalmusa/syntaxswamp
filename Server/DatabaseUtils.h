#pragma once
#include "sqlite3.h"
#include <iostream>
#include <functional>
#include <string>
#include <chrono>
#include <thread>

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
