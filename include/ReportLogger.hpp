#pragma once

#include <string>
#include <mutex>
#include "SampleAnalyzer.hpp"
#include "Config.hpp"

namespace HypoInspector {

class ReportLogger {
public:
    explicit ReportLogger(const Config& config);

    // Records analysis result to CSV audit log and exports snapshot image
    bool logInspection(const AnalysisResult& result, std::string& outImagePath);

    // Accessors
    std::string getLogPath() const;
    std::string getInspectionsDir() const;

private:
    std::string sanitizeFilename(const std::string& input) const;
    void ensureHeaderExists();

    Config m_config;
    mutable std::mutex m_mutex;
};

} // namespace HypoInspector

