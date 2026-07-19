#include "cache/sqlite_cache.h"

#include <sqlite3.h>

#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>

namespace cache {

namespace {

constexpr const char* create_table_sql = R"SQL(
CREATE TABLE IF NOT EXISTS pending_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    topic TEXT NOT NULL,
    payload TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
)
)SQL";

}  // namespace

SqliteCache::SqliteCache(std::string path)
    : path_(std::move(path)),
      ready_(initialize()) {}

SqliteCache::~SqliteCache() {
    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

bool SqliteCache::is_ready() const noexcept {
    return ready_;
}

const std::string& SqliteCache::error_message() const noexcept {
    return error_message_;
}

bool SqliteCache::initialize() {
    const std::filesystem::path database_path(path_);
    const std::filesystem::path parent =
        database_path.parent_path();

    if (!parent.empty()) {
        std::error_code error;
        std::filesystem::create_directories(parent, error);

        if (error) {
            set_error(
                "create cache directory failed: "
                + error.message()
            );
            return false;
        }
    }

    const int open_result = sqlite3_open_v2(
        path_.c_str(),
        &database_,
        SQLITE_OPEN_READWRITE
            | SQLITE_OPEN_CREATE
            | SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (open_result != SQLITE_OK) {
        set_error(
            "open SQLite cache failed: "
            + std::string(
                database_ != nullptr
                    ? sqlite3_errmsg(database_)
                    : sqlite3_errstr(open_result)
            )
        );
        return false;
    }

    sqlite3_busy_timeout(database_, 3000);

    return execute("PRAGMA journal_mode=WAL")
        && execute("PRAGMA synchronous=FULL")
        && execute(create_table_sql);
}

bool SqliteCache::execute(const char* sql) const {
    char* sqlite_error = nullptr;
    const int result = sqlite3_exec(
        database_,
        sql,
        nullptr,
        nullptr,
        &sqlite_error
    );

    if (result == SQLITE_OK) {
        return true;
    }

    const std::string message =
        sqlite_error != nullptr
            ? sqlite_error
            : sqlite3_errmsg(database_);

    sqlite3_free(sqlite_error);
    set_error("SQLite command failed: " + message);
    return false;
}

void SqliteCache::set_error(
    const std::string& message) const {

    error_message_ = message;
    std::cerr << "[ERROR] " << message << std::endl;
}

bool SqliteCache::append(
    const std::string& topic,
    const std::string& payload) {

    if (!ready_) {
        return false;
    }

    constexpr const char* insert_sql =
        "INSERT INTO pending_messages(topic, payload) "
        "VALUES(?, ?)";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(
            database_,
            insert_sql,
            -1,
            &statement,
            nullptr) != SQLITE_OK) {

        set_error(
            "prepare cache insert failed: "
            + std::string(sqlite3_errmsg(database_))
        );
        return false;
    }

    const int topic_result = sqlite3_bind_text(
        statement,
        1,
        topic.c_str(),
        static_cast<int>(topic.size()),
        SQLITE_TRANSIENT
    );

    const int payload_result = sqlite3_bind_text(
        statement,
        2,
        payload.c_str(),
        static_cast<int>(payload.size()),
        SQLITE_TRANSIENT
    );

    const bool success =
        topic_result == SQLITE_OK
        && payload_result == SQLITE_OK
        && sqlite3_step(statement) == SQLITE_DONE;

    if (!success) {
        set_error(
            "insert cache message failed: "
            + std::string(sqlite3_errmsg(database_))
        );
    }

    sqlite3_finalize(statement);
    return success;
}

std::vector<CachedMessage> SqliteCache::load_all() const {
    std::vector<CachedMessage> messages;

    if (!ready_) {
        return messages;
    }

    constexpr const char* select_sql =
        "SELECT topic, payload "
        "FROM pending_messages ORDER BY id";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(
            database_,
            select_sql,
            -1,
            &statement,
            nullptr) != SQLITE_OK) {

        set_error(
            "prepare cache query failed: "
            + std::string(sqlite3_errmsg(database_))
        );
        return messages;
    }

    int step_result = SQLITE_ROW;

    while ((step_result = sqlite3_step(statement))
           == SQLITE_ROW) {

        const auto* topic = sqlite3_column_text(
            statement,
            0
        );
        const auto* payload = sqlite3_column_text(
            statement,
            1
        );

        messages.push_back({
            topic != nullptr
                ? reinterpret_cast<const char*>(topic)
                : "",
            payload != nullptr
                ? reinterpret_cast<const char*>(payload)
                : ""
        });
    }

    if (step_result != SQLITE_DONE) {
        set_error(
            "query cache messages failed: "
            + std::string(sqlite3_errmsg(database_))
        );
        messages.clear();
    }

    sqlite3_finalize(statement);
    return messages;
}

bool SqliteCache::remove_first(std::size_t count) {
    if (!ready_ || count == 0) {
        return ready_;
    }

    if (count > static_cast<std::size_t>(
                    std::numeric_limits<int>::max())) {

        set_error("cache removal count is too large");
        return false;
    }

    constexpr const char* delete_sql = R"SQL(
DELETE FROM pending_messages
WHERE id IN (
    SELECT id FROM pending_messages
    ORDER BY id
    LIMIT ?
)
)SQL";

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(
            database_,
            delete_sql,
            -1,
            &statement,
            nullptr) != SQLITE_OK) {

        set_error(
            "prepare cache removal failed: "
            + std::string(sqlite3_errmsg(database_))
        );
        return false;
    }

    const bool success =
        sqlite3_bind_int(
            statement,
            1,
            static_cast<int>(count)
        ) == SQLITE_OK
        && sqlite3_step(statement) == SQLITE_DONE;

    if (!success) {
        set_error(
            "remove cached messages failed: "
            + std::string(sqlite3_errmsg(database_))
        );
    }

    sqlite3_finalize(statement);
    return success;
}

}  // namespace cache
