#include "ReportLogger.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace HypoInspector {

ReportLogger::ReportLogger(const Config& config) : m_config(config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    ensureHeaderExists();
}

std::string ReportLogger::sanitizeFilename(const std::string& input) const {
    std::string clean = input;
    for (char& c : clean) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
            c == '"' || c == '<' || c == '>' || c == '|' || c == ' ') {
            c = '_';
        }
    }
    return clean;
}

void ReportLogger::ensureHeaderExists() {
    try {
        std::filesystem::path csvPath(m_config.batchLogPath);
        if (csvPath.has_parent_path()) {
            std::filesystem::create_directories(csvPath.parent_path());
        }

        std::filesystem::path inspPath(m_config.inspectionsDir);
        std::filesystem::create_directories(inspPath);

        bool needsHeader = !std::filesystem::exists(csvPath) ||
                           std::filesystem::file_size(csvPath) == 0;

        if (needsHeader) {
            std::ofstream file(csvPath, std::ios::out | std::ios::app);
            if (file.is_open()) {
                file << "Timestamp,BucketID,D10_mm,D50_mm,D90_mm,ClumpCount,MeanIR,MoisturePct,Verdict\n";
                file.flush();
                std::cout << "[ReportLogger] Created new CSV audit trail: " << m_config.batchLogPath << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ReportLogger] Error initializing log file: " << e.what() << std::endl;
    }
}

bool ReportLogger::logInspection(const AnalysisResult& result, std::string& outImagePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!result.success) {
        std::cerr << "[ReportLogger] Cannot log failed analysis result." << std::endl;
        return false;
    }

    // 1. Export Inspection Snapshot Image
    try {
        std::filesystem::create_directories(m_config.inspectionsDir);
        std::string safeId = sanitizeFilename(result.bucketId);
        std::string safeTime = sanitizeFilename(result.timestamp);
        outImagePath = m_config.inspectionsDir + "/" + safeId + "_" + safeTime + ".jpg";

        if (!result.annotatedImage.empty()) {
            bool saved = cv::imwrite(outImagePath, result.annotatedImage);
            if (saved) {
                std::cout << "[ReportLogger] Inspection image saved: " << outImagePath << std::endl;
            } else {
                std::cerr << "[ReportLogger] Warning: cv::imwrite returned false for " << outImagePath << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ReportLogger] Error saving inspection image: " << e.what() << std::endl;
    }

    // 2. Append to CSV Audit Trail
    try {
        ensureHeaderExists();
        std::ofstream csv(m_config.batchLogPath, std::ios::out | std::ios::app);
        if (!csv.is_open()) {
            std::cerr << "[ReportLogger] Error: Unable to open CSV log file: "
                      << m_config.batchLogPath << std::endl;
            return false;
        }

        // Schema: Timestamp,BucketID,D10_mm,D50_mm,D90_mm,ClumpCount,MeanIR,MoisturePct,Verdict
        csv << result.timestamp << ","
            << result.bucketId << ","
            << std::fixed << std::setprecision(2) << result.d10_mm << ","
            << std::fixed << std::setprecision(2) << result.d50_mm << ","
            << std::fixed << std::setprecision(2) << result.d90_mm << ","
            << result.clumpCount << ","
            << std::fixed << std::setprecision(1) << result.meanIR << ","
            << std::fixed << std::setprecision(2) << result.moisturePct << ","
            << result.verdict << "\n";

        // Flush immediately to disk to guard against benchtop power drops
        csv.flush();

        std::cout << "[ReportLogger] Logged audit entry for " << result.bucketId
                  << " (Moisture: " << std::fixed << std::setprecision(2) << result.moisturePct
                  << "%, Verdict: " << result.verdict << ")" << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[ReportLogger] Error writing CSV entry: " << e.what() << std::endl;
        return false;
    }
}

std::string ReportLogger::getLogPath() const {
    return m_config.batchLogPath;
}

std::string ReportLogger::getInspectionsDir() const {
    return m_config.inspectionsDir;
}

} // namespace HypoInspector

