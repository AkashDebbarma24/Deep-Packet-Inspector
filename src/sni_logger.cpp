#include "sni_logger.h"

#include <fstream>
#include <iomanip>
#include <ctime>

namespace DPI {

SNILogger::SNILogger(std::string csv_path, std::chrono::seconds flush_interval)
    : csv_path_(std::move(csv_path)), flush_interval_(flush_interval) {
    // Write a CSV header once at startup so the file is readable even
    // if the program is interrupted before the first flush.
    std::ofstream ofs(csv_path_, std::ios::app);
    if (ofs.tellp() == 0) {
        ofs << "domain,count,last_seen\n";
    }
    ofs.close();

    worker_ = std::thread(&SNILogger::flushLoop, this);
}

SNILogger::~SNILogger() {
    stop_.store(true, std::memory_order_relaxed);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
    flushToDisk(); // final flush so nothing collected since the last tick is lost
}

void SNILogger::record(const std::string& domain, std::chrono::system_clock::time_point ts) {
    if (domain.empty()) return;
    std::lock_guard<std::mutex> lock(mutex_);
    auto& e = counts_[domain];
    ++e.count;
    e.last_seen = ts;
}

void SNILogger::flushNow() {
    flushToDisk();
}

void SNILogger::flushLoop() {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    while (!stop_.load(std::memory_order_relaxed)) {
        cv_.wait_for(lock, flush_interval_);
        if (stop_.load(std::memory_order_relaxed)) break;
        flushToDisk();
    }
}

void SNILogger::flushToDisk() {
    // Swap the live map out under lock, then do the slow disk I/O
    // with no lock held, so record() on other threads is never blocked
    // waiting for a CSV write to finish.
    std::unordered_map<std::string, Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (counts_.empty()) return;
        snapshot.swap(counts_);
    }

    std::ofstream ofs(csv_path_, std::ios::app);
    if (!ofs.is_open()) return;

    for (const auto& [domain, entry] : snapshot) {
        std::time_t t = std::chrono::system_clock::to_time_t(entry.last_seen);
        std::tm tm_buf{};
#if defined(_WIN32)
        gmtime_s(&tm_buf, &t);
#else
        gmtime_r(&t, &tm_buf);
#endif
        ofs << domain << ','
            << entry.count << ','
            << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '\n';
    }
}

} // namespace DPI
