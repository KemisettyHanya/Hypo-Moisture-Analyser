#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace HypoInspector {

struct Config {
    // System Settings
    std::string chamberName = "Benchtop-Station-01";
    std::string cameraMode = "static_file"; // "static_file", "picamera3", "v4l2"
    bool headless = false;
    std::string sampleImageDir = "data/sample_images";
    std::string defaultSampleImage = "data/sample_images/dry_sample.png";

    // Camera / Optical Capture Settings
    int frameWidth = 1920;
    int frameHeight = 1080;
    int shutterSpeedUs = 500;
    double analogGain = 1.0;
    double lensPosition = 0.0; // Locked focal position
    int frameAverageCount = 4;
    int deviceIndex = 0;

    // Granule Morphology Settings
    double scaleFactorMmPerPixel = 0.05; // 0.05 mm/pixel -> 20 pixels = 1.0 mm
    double clumpThresholdMm = 4.5;       // Particles > 4.5 mm are candidate clumps
    double circularityThreshold = 0.65;  // Circularity < 0.65 flags wet agglomerates
    double minGranuleAreaPx = 25.0;      // Filter noise artifacts
    int gaussianBlurKernel = 5;
    int adaptiveBlockSize = 25;
    double adaptiveC = -5.0;

    // Circular Tray ROI (Petri dish)
    int roiCenterX = 960;
    int roiCenterY = 540;
    int roiRadiusPx = 460;
    bool autoDetectDish = false;

    // Moisture Calibration Regression:
    // Moisture % = alpha - (beta * I_mean) + (gamma * clumpRatio)
    double alpha = 19.5;
    double beta = 0.075;
    double gamma = 12.0;

    // Quality Thresholds (Hydrated Cal Hypo UN 2880)
    double minMoisturePct = 5.5;  // < 5.5% -> Under-dry hazard (Spontaneous deflagration)
    double maxMoisturePct = 10.0; // > 10.0% -> Over-wet hazard (Degradation / Drum bulging)

    // Batch & Session Management
    std::string batchPrefix = "BATCH";
    std::string batchLogPath = "data/batch_log.csv";
    std::string inspectionsDir = "data/inspections";
    std::string sessionStatePath = "config/session_state.json";

    // Load / Save Methods
    bool loadFromFile(const std::string& filepath);
    bool saveToFile(const std::string& filepath) const;

    // JSON serialization
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

} // namespace HypoInspector

