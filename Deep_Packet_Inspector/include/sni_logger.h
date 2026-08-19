#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <chrono>

namespace DPI {

// ============================================================================
// SNILogger - Tracks how often each domain (SNI) is seen and periodically
// flushes the counts to a CSV file. Safe to call record() from many threads
// at once (e.g. one call per FastPath worker thread).
// ============================================================================
class SNILogger {
public:
    explicit SNILogger(std::string csv_path,
                        std::chrono::seconds flush_interval = std::chrono::seconds(30));
    ~SNILogger();

    // Non-copyable, non-movable (owns a background thread + file handle)
    SNILogger(const SNILogger&) = delete;
    SNILogger& operator=(const SNILogger&) = delete;

    // Called from worker threads whenever a domain is seen
    void record(const std::string& domain, std::chrono::system_clock::time_point ts);

    // Force an immediate flush (e.g. before printing a final report)
    void flushNow();

private:
    struct Entry {
        uint64_t count = 0;
        std::chrono::system_clock::time_point last_seen;
    };

    void flushLoop();     // background thread body
    void flushToDisk();   // swaps out the map and writes it to the CSV

    std::string csv_path_;
    std::chrono::seconds flush_interval_;

    std::mutex mutex_;
    std::unordered_map<std::string, Entry> counts_;

    std::thread worker_;
    std::atomic<bool> stop_{false};
    std::condition_variable cv_;
    std::mutex cv_mutex_;
};

} // namespace DPI
