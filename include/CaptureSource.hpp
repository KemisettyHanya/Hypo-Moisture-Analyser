#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include "Config.hpp"

namespace HypoInspector {

class ICaptureSource {
public:
    virtual ~ICaptureSource() = default;
    virtual bool initialize() = 0;
    virtual bool captureSample(cv::Mat& outFrame) = 0;
    virtual void close() = 0;
    virtual std::string getSourceName() const = 0;
};

// Generates synthetic benchtop sample plates if missing
class SyntheticSampleGenerator {
public:
    static bool generateDefaultSamples(const std::string& outputDir);
    static cv::Mat generateDrySamplePlate(int width = 1920, int height = 1080);
    static cv::Mat generateWetSamplePlate(int width = 1920, int height = 1080);
    static cv::Mat generateUnderdrySamplePlate(int width = 1920, int height = 1080);
};

// Desktop Simulation: Ingests pre-recorded sample images
class StaticFileSource : public ICaptureSource {
public:
    explicit StaticFileSource(const Config& config);
    ~StaticFileSource() override;

    bool initialize() override;
    bool captureSample(cv::Mat& outFrame) override;
    void close() override;
    std::string getSourceName() const override;

    void cycleNextSample();
    void setSamplePath(const std::string& path);
    std::string getCurrentSamplePath() const;
    std::vector<std::string> getAvailableSamples() const;

private:
    void discoverSampleImages();

    Config m_config;
    std::vector<std::string> m_sampleFiles;
    size_t m_currentIndex = 0;
    bool m_initialized = false;
};

// Raspberry Pi 5: Hardware capture via libcamera / V4L2 from Camera Module 3 NoIR
class PiCamera3Source : public ICaptureSource {
public:
    explicit PiCamera3Source(const Config& config);
    ~PiCamera3Source() override;

    bool initialize() override;
    bool captureSample(cv::Mat& outFrame) override;
    void close() override;
    std::string getSourceName() const override;

private:
    Config m_config;
    cv::VideoCapture m_cap;
    bool m_initialized = false;
};

// Factory to create capture source based on configuration
std::unique_ptr<ICaptureSource> createCaptureSource(const Config& config);

} // namespace HypoInspector

