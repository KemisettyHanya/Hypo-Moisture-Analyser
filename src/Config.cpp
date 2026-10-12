#include "Config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace HypoInspector {

nlohmann::json Config::toJson() const {
    nlohmann::json j;
    j["system"] = {
        {"chamberName", chamberName},
        {"cameraMode", cameraMode},
        {"headless", headless},
        {"sampleImageDir", sampleImageDir},
        {"defaultSampleImage", defaultSampleImage}
    };
    j["camera"] = {
        {"frameWidth", frameWidth},
        {"frameHeight", frameHeight},
        {"shutterSpeedUs", shutterSpeedUs},
        {"analogGain", analogGain},
        {"lensPosition", lensPosition},
        {"frameAverageCount", frameAverageCount},
        {"deviceIndex", deviceIndex}
    };
    j["morphology"] = {
        {"scaleFactorMmPerPixel", scaleFactorMmPerPixel},
        {"clumpThresholdMm", clumpThresholdMm},
        {"circularityThreshold", circularityThreshold},
        {"minGranuleAreaPx", minGranuleAreaPx},
        {"gaussianBlurKernel", gaussianBlurKernel},
        {"adaptiveBlockSize", adaptiveBlockSize},
        {"adaptiveC", adaptiveC},
        {"roiCenterX", roiCenterX},
        {"roiCenterY", roiCenterY},
        {"roiRadiusPx", roiRadiusPx},
        {"autoDetectDish", autoDetectDish}
    };
    j["calibration"] = {
        {"alpha", alpha},
        {"beta", beta},
        {"gamma", gamma}
    };
    j["quality"] = {
        {"minMoisturePct", minMoisturePct},
        {"maxMoisturePct", maxMoisturePct}
    };
    j["batch"] = {
        {"batchPrefix", batchPrefix},
        {"batchLogPath", batchLogPath},
        {"inspectionsDir", inspectionsDir},
        {"sessionStatePath", sessionStatePath}
    };
    return j;
}

void Config::fromJson(const nlohmann::json& j) {
    if (j.contains("system")) {
        const auto& s = j["system"];
        chamberName = s.value("chamberName", chamberName);
        cameraMode = s.value("cameraMode", cameraMode);
        headless = s.value("headless", headless);
        sampleImageDir = s.value("sampleImageDir", sampleImageDir);
        defaultSampleImage = s.value("defaultSampleImage", defaultSampleImage);
    }
    if (j.contains("camera")) {
        const auto& c = j["camera"];
        frameWidth = c.value("frameWidth", frameWidth);
        frameHeight = c.value("frameHeight", frameHeight);
        shutterSpeedUs = c.value("shutterSpeedUs", shutterSpeedUs);
        analogGain = c.value("analogGain", analogGain);
        lensPosition = c.value("lensPosition", lensPosition);
        frameAverageCount = c.value("frameAverageCount", frameAverageCount);
        deviceIndex = c.value("deviceIndex", deviceIndex);
    }
    if (j.contains("morphology")) {
        const auto& m = j["morphology"];
        scaleFactorMmPerPixel = m.value("scaleFactorMmPerPixel", scaleFactorMmPerPixel);
        clumpThresholdMm = m.value("clumpThresholdMm", clumpThresholdMm);
        circularityThreshold = m.value("circularityThreshold", circularityThreshold);
        minGranuleAreaPx = m.value("minGranuleAreaPx", minGranuleAreaPx);
        gaussianBlurKernel = m.value("gaussianBlurKernel", gaussianBlurKernel);
        adaptiveBlockSize = m.value("adaptiveBlockSize", adaptiveBlockSize);
        adaptiveC = m.value("adaptiveC", adaptiveC);
        roiCenterX = m.value("roiCenterX", roiCenterX);
        roiCenterY = m.value("roiCenterY", roiCenterY);
        roiRadiusPx = m.value("roiRadiusPx", roiRadiusPx);
        autoDetectDish = m.value("autoDetectDish", autoDetectDish);
    }
    if (j.contains("calibration")) {
        const auto& cal = j["calibration"];
        alpha = cal.value("alpha", alpha);
        beta = cal.value("beta", beta);
        gamma = cal.value("gamma", gamma);
    }
    if (j.contains("quality")) {
        const auto& q = j["quality"];
        minMoisturePct = q.value("minMoisturePct", minMoisturePct);
        maxMoisturePct = q.value("maxMoisturePct", maxMoisturePct);
    }
    if (j.contains("batch")) {
        const auto& b = j["batch"];
        batchPrefix = b.value("batchPrefix", batchPrefix);
        batchLogPath = b.value("batchLogPath", batchLogPath);
        inspectionsDir = b.value("inspectionsDir", inspectionsDir);
        sessionStatePath = b.value("sessionStatePath", sessionStatePath);
    }
}

bool Config::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[Config] Warning: Could not open config file: " << filepath
                  << ". Using default configuration." << std::endl;
        return false;
    }
    try {
        nlohmann::json j;
        file >> j;
        fromJson(j);
        std::cout << "[Config] Loaded configuration from " << filepath << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Error parsing " << filepath << ": " << e.what()
                  << ". Falling back to defaults." << std::endl;
        return false;
    }
}

bool Config::saveToFile(const std::string& filepath) const {
    try {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[Config] Error: Could not write to " << filepath << std::endl;
            return false;
        }
        file << toJson().dump(4) << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Config] Error saving to " << filepath << ": " << e.what() << std::endl;
        return false;
    }
}

} // namespace HypoInspector

