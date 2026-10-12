#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include "Config.hpp"

namespace HypoInspector {

struct ParticleInfo {
    cv::Rect boundingBox;
    std::vector<cv::Point> contour;
    cv::Point2f center;
    double areaPx = 0.0;
    double perimeterPx = 0.0;
    double diameterMm = 0.0;
    double circularity = 0.0;
    bool isClump = false;
};

struct AnalysisResult {
    bool success = false;
    std::string errorMessage;

    // Identifiers & Timestamps
    std::string bucketId;
    std::string timestamp;

    // Granule Size Distribution (Cumulative Percentiles in mm)
    double d10_mm = 0.0;
    double d50_mm = 0.0; // Median
    double d90_mm = 0.0;

    // Morphology & Agglomeration
    int totalGranules = 0;
    int clumpCount = 0;
    double totalGranuleAreaPx = 0.0;
    double clumpAreaPx = 0.0;
    double clumpRatio = 0.0; // Clump Area / Total Granule Area

    // 850nm NIR Reflectance & Moisture Calibration
    double meanIR = 0.0;        // Mean intensity I_sample across granule surface (DN)
    double moisturePct = 0.0;    // Moisture percentage %
    std::string verdict;         // "PASS", "FAIL (UNDER-DRY)", "FAIL (OVER-WET)"

    // Particles list
    std::vector<ParticleInfo> particles;

    // Annotated Visualization
    cv::Mat annotatedImage;
};

class SampleAnalyzer {
public:
    explicit SampleAnalyzer(const Config& config);

    // Primary inspection entry point
    AnalysisResult analyze(const cv::Mat& rawFrame, const std::string& bucketId);

    // Helper functions
    cv::Mat segmentDishROI(const cv::Mat& input, cv::Point& outCenter, int& outRadius) const;
    void updateConfig(const Config& config);

private:
    double computePercentile(const std::vector<double>& sortedValues, double percentile) const;
    std::string getCurrentTimestampString() const;

    Config m_config;
};

} // namespace HypoInspector

