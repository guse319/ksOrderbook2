#include "ksLogger.h"

KsLogger::KsLogger(const std::string& log_file_path, const bool& prefixes) : log_file_path_(log_file_path), prefixes_(prefixes) {
    log_file_.open(log_file_path_, std::ios::app);
    if (!log_file_.is_open()) {
        throw std::runtime_error("Unable to open log file: " + log_file_path_);
    }
}

KsLogger::~KsLogger() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void KsLogger::info(const std::string& message) {
    log("INFO", message);
}

void KsLogger::warning(const std::string& message) {
    log("WARNING", message);
}

void KsLogger::error(const std::string& message) {
    log("ERROR", message);
}

void KsLogger::log(const std::string& level, const std::string& message) {
    if (prefixes_) {
        log_file_ << getLogPrefix(level) << " " << message << std::endl;
    } else {
        log_file_ << message << std::endl;
    }
}

std::string KsLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm buf;
    localtime_r(&in_time_t, &buf);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &buf);
    auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()).count() % 1000000;
    char full_timestamp[28];
    snprintf(full_timestamp, sizeof(full_timestamp), "%s.%06ld", timestamp, microseconds);
    return std::string(full_timestamp);
}

std::string KsLogger::getLogPrefix(const std::string& level) {
    return "[" + getCurrentTimestamp() + "] [" + level + "]";
}