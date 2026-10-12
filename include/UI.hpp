#pragma once

#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include "Config.hpp"
#include "BucketIdManager.hpp"
#include "SampleAnalyzer.hpp"

namespace HypoInspector {

enum class UIEvent {
    None,
    TriggerScan,
    GenerateAutoId,
    CycleSample,
    ManualIdPrompt,
    Quit
};

class UI {
public:
    explicit UI(const Config& config);
    ~UI();

    // Initializes UI display window (or configures headless mode)
    bool initialize();

    // Renders complete benchtop operator dashboard
    cv::Mat renderDashboard(const cv::Mat& viewportImage,
                            const AnalysisResult& lastResult,
                            const BucketIdManager& bucketMgr,
                            const std::string& statusMessage = "");

    // Displays dashboard in GUI window and handles keyboard / HID scanner input
    UIEvent processEvents(const cv::Mat& dashboardCanvas,
                          BucketIdManager& bucketMgr,
                          int waitDelayMs = 30);

    bool isHeadless() const;
    void close();

private:
    void drawCardBackground(cv::Mat& canvas, const cv::Rect& rect,
                            const std::string& title = "",
                            const cv::Scalar& headerColor = cv::Scalar(180, 180, 180)) const;

    Config m_config;
    std::string m_windowName;
    bool m_headless = false;
    bool m_windowCreated = false;
    int m_canvasWidth = 1920;
    int m_canvasHeight = 1080;
};

} // namespace HypoInspector

