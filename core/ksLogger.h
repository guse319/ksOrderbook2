#ifndef KS_LOGGER_H
#define KS_LOGGER_H

#include <chrono>
#include <fstream>
#include <string>

class KsLogger {

public:

    KsLogger(const std::string& log_file_path, const bool& prefixes);
    ~KsLogger();

    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);

private:

    void log(const std::string& level, const std::string& message);

    std::string getCurrentTimestamp();
    std::string getLogPrefix(const std::string& level);

    std::string log_file_path_;
    bool prefixes_;

    std::ofstream log_file_;

};

#endif // KS_LOGGER_H