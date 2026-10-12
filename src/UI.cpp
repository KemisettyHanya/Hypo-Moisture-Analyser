#include "UI.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>

namespace HypoInspector {

UI::UI(const Config& config)
    : m_config(config),
      m_windowName("Benchtop Cal Hypo Inspection Station (IMX708 NoIR 850nm)") {
    // Check if display server is present
    const char* disp = std::getenv("DISPLAY");
    const char* wayland = std::getenv("WAYLAND_DISPLAY");
    if (m_config.headless || (disp == nullptr && wayland == nullptr)) {
        m_headless = true;
    }
}

UI::~UI() {
    close();
}

bool UI::initialize() {
    if (m_headless) {
        std::cout << "[UI] Running in Headless Mode (no GUI window displayed)." << std::endl;
        return true;
    }

    try {
        cv::namedWindow(m_windowName, cv::WINDOW_NORMAL);
        cv::resizeWindow(m_windowName, 1600, 900);
        m_windowCreated = true;
        std::cout << "[UI] GUI window initialized: " << m_windowName << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[UI] Warning: Could not create GUI window (" << e.what()
                  << "). Falling back to Headless mode." << std::endl;
        m_headless = true;
        return false;
    }
}

bool UI::isHeadless() const {
    return m_headless;
}

void UI::close() {
    if (m_windowCreated) {
        try {
            cv::destroyWindow(m_windowName);
        } catch (...) {}
        m_windowCreated = false;
    }
}

void UI::drawCardBackground(cv::Mat& canvas, const cv::Rect& rect,
                            const std::string& title,
                            const cv::Scalar& headerColor) const {
    // Card background
    cv::rectangle(canvas, rect, cv::Scalar(32, 35, 42), -1);
    // Card subtle border
    cv::rectangle(canvas, rect, cv::Scalar(55, 60, 72), 1);

    if (!title.empty()) {
        // Card title bar
        cv::Rect headerRect(rect.x, rect.y, rect.width, 28);
        cv::rectangle(canvas, headerRect, cv::Scalar(40, 44, 53), -1);
        cv::line(canvas, cv::Point(rect.x, rect.y + 28),
                 cv::Point(rect.x + rect.width, rect.y + 28),
                 cv::Scalar(65, 72, 85), 1);
        cv::putText(canvas, title, cv::Point(rect.x + 12, rect.y + 19),
                    cv::FONT_HERSHEY_SIMPLEX, 0.48, headerColor, 1, cv::LINE_AA);
    }
}

cv::Mat UI::renderDashboard(const cv::Mat& viewportImage,
                            const AnalysisResult& lastResult,
                            const BucketIdManager& bucketMgr,
                            const std::string& statusMessage) {
    cv::Mat canvas = cv::Mat::zeros(m_canvasHeight, m_canvasWidth, CV_8UC3);
    // Base dark background
    canvas.setTo(cv::Scalar(20, 22, 27));

    // ---------------------------------------------------------
    // 1. Left Viewport (0 to 1320 width): Camera / Inspection View
    // ---------------------------------------------------------
    int vpWidth = 1320;
    int vpHeight = 1040;
    int vpX = 20;
    int vpY = 20;

    cv::Rect vpRect(vpX, vpY, vpWidth, vpHeight);
    cv::rectangle(canvas, vpRect, cv::Scalar(28, 30, 36), -1);
    cv::rectangle(canvas, vpRect, cv::Scalar(60, 65, 78), 2);

    if (!viewportImage.empty()) {
        cv::Mat displayImg;
        if (viewportImage.cols != vpWidth || viewportImage.rows != vpHeight) {
            cv::resize(viewportImage, displayImg, cv::Size(vpWidth, vpHeight));
        } else {
            displayImg = viewportImage;
        }
        displayImg.copyTo(canvas(vpRect));
    } else {
        cv::putText(canvas, "NO ACTIVE CAMERA FEED / SAMPLE IMAGE",
                    cv::Point(vpX + 400, vpY + 520),
                    cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(120, 125, 135), 2, cv::LINE_AA);
    }

    // Viewport Header Tag
    cv::Rect tagRect(vpX + 15, vpY + 15, 230, 32);
    cv::rectangle(canvas, tagRect, cv::Scalar(15, 15, 18), -1);
    cv::rectangle(canvas, tagRect, cv::Scalar(0, 180, 255), 1);
    cv::putText(canvas, "BENCHTOP VIEWPORT", cv::Point(tagRect.x + 14, tagRect.y + 22),
                cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(0, 200, 255), 1, cv::LINE_AA);

    // ---------------------------------------------------------
    // 2. Right Sidebar (1360 to 1900 width): Telemetry Cards
    // ---------------------------------------------------------
    int sbX = 1360;
    int sbWidth = 540;

    // A. Station Header Banner
    cv::Rect headRect(sbX, 20, sbWidth, 80);
    drawCardBackground(canvas, headRect);
    cv::putText(canvas, "CALCIUM HYPOCHLORITE", cv::Point(sbX + 16, 52),
                cv::FONT_HERSHEY_SIMPLEX, 0.72, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
    cv::putText(canvas, "BENCHTOP QUALITY & MOISTURE STATION", cv::Point(sbX + 16, 76),
                cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(0, 180, 255), 1, cv::LINE_AA);
    cv::putText(canvas, m_config.chamberName, cv::Point(sbX + sbWidth - 175, 52),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(150, 155, 165), 1, cv::LINE_AA);

    // B. Bucket ID & Barcode Ingestion Card
    cv::Rect bucketCard(sbX, 115, sbWidth, 120);
    drawCardBackground(canvas, bucketCard, "SAMPLE BUCKET IDENTIFICATION");

    std::string activeId = bucketMgr.getActiveBucketId();
    if (activeId.empty()) activeId = "NO ACTIVE ID";

    // Active ID display box
    cv::Rect idBox(sbX + 16, 152, sbWidth - 32, 42);
    cv::rectangle(canvas, idBox, cv::Scalar(18, 20, 25), -1);
    cv::rectangle(canvas, idBox, cv::Scalar(0, 180, 255), 2);
    cv::putText(canvas, activeId, cv::Point(idBox.x + 16, idBox.y + 28),
                cv::FONT_HERSHEY_SIMPLEX, 0.85, cv::Scalar(0, 220, 255), 2, cv::LINE_AA);

    // Scanner Status
    std::string scanStatus = bucketMgr.getScannerStatus();
    cv::Scalar scanColor = bucketMgr.isBuffering() ? cv::Scalar(0, 160, 255) : cv::Scalar(0, 220, 100);
    cv::circle(canvas, cv::Point(sbX + 24, 214), 5, scanColor, -1);
    cv::putText(canvas, "Scanner: " + scanStatus, cv::Point(sbX + 38, 218),
                cv::FONT_HERSHEY_SIMPLEX, 0.45, scanColor, 1, cv::LINE_AA);

    if (bucketMgr.isBuffering()) {
        std::string bufDisplay = "Buffer: [" + bucketMgr.getInputBuffer() + "_]";
        cv::putText(canvas, bufDisplay, cv::Point(sbX + 260, 218),
                    cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);
    }

    // C. Moisture & Quality Verdict Hero Card
    cv::Rect moistCard(sbX, 250, sbWidth, 230);
    drawCardBackground(canvas, moistCard, "MOISTURE CALIBRATION (NIR 850nm)");

    if (lastResult.success) {
        // Moisture Value
        std::ostringstream moistStr;
        moistStr << std::fixed << std::setprecision(2) << lastResult.moisturePct << " %";

        cv::Scalar verdictColor = cv::Scalar(0, 220, 0); // PASS green
        cv::Scalar verdictBg = cv::Scalar(0, 70, 0);
        if (lastResult.verdict.find("OVER-WET") != std::string::npos) {
            verdictColor = cv::Scalar(0, 0, 255); // FAIL red
            verdictBg = cv::Scalar(0, 0, 90);
        } else if (lastResult.verdict.find("UNDER-DRY") != std::string::npos) {
            verdictColor = cv::Scalar(0, 140, 255); // WARNING orange
            verdictBg = cv::Scalar(0, 50, 110);
        }

        cv::putText(canvas, moistStr.str(), cv::Point(sbX + 25, 345),
                    cv::FONT_HERSHEY_SIMPLEX, 1.8, verdictColor, 4, cv::LINE_AA);

        // Quality Verdict Badge
        cv::Rect badgeRect(sbX + 265, 295, 250, 55);
        cv::rectangle(canvas, badgeRect, verdictBg, -1);
        cv::rectangle(canvas, badgeRect, verdictColor, 2);

        int textOffset = (lastResult.verdict == "PASS") ? 80 : 20;
        cv::putText(canvas, lastResult.verdict, cv::Point(badgeRect.x + textOffset, badgeRect.y + 36),
                    cv::FONT_HERSHEY_SIMPLEX, 0.72, verdictColor, 2, cv::LINE_AA);

        // Safe Window Indicator Bar
        cv::Rect barBg(sbX + 25, 395, sbWidth - 50, 20);
        cv::rectangle(canvas, barBg, cv::Scalar(45, 48, 55), -1);

        // Cal Hypo Safe Window: 5.5% to 10.0% mapped on 0% to 15% range
        double minR = 0.0, maxR = 15.0;
        int safeStart = barBg.x + static_cast<int>((5.5 - minR) / (maxR - minR) * barBg.width);
        int safeEnd = barBg.x + static_cast<int>((10.0 - minR) / (maxR - minR) * barBg.width);

        cv::Rect safeZone(safeStart, barBg.y, safeEnd - safeStart, barBg.height);
        cv::rectangle(canvas, safeZone, cv::Scalar(0, 100, 0), -1);

        // Current needle
        double clampedM = std::min(maxR, std::max(minR, lastResult.moisturePct));
        int needleX = barBg.x + static_cast<int>((clampedM - minR) / (maxR - minR) * barBg.width);
        cv::line(canvas, cv::Point(needleX, barBg.y - 4), cv::Point(needleX, barBg.y + barBg.height + 4),
                 verdictColor, 3);

        cv::putText(canvas, "0%", cv::Point(barBg.x, barBg.y + 36),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(140, 140, 140), 1, cv::LINE_AA);
        cv::putText(canvas, "5.5% (Safe Min)", cv::Point(safeStart - 20, barBg.y + 36),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(0, 200, 0), 1, cv::LINE_AA);
        cv::putText(canvas, "10.0% (Safe Max)", cv::Point(safeEnd - 30, barBg.y + 36),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(0, 200, 0), 1, cv::LINE_AA);
        cv::putText(canvas, "15%", cv::Point(barBg.x + barBg.width - 24, barBg.y + 36),
                    cv::FONT_HERSHEY_SIMPLEX, 0.40, cv::Scalar(140, 140, 140), 1, cv::LINE_AA);

        cv::putText(canvas, "Standard: Hydrated Calcium Hypochlorite UN 2880 (5.5 - 10.0% H2O)",
                    cv::Point(sbX + 25, 462), cv::FONT_HERSHEY_SIMPLEX, 0.42,
                    cv::Scalar(170, 175, 185), 1, cv::LINE_AA);
    } else {
        cv::putText(canvas, "AWAITING SAMPLE SCAN", cv::Point(sbX + 110, 360),
                    cv::FONT_HERSHEY_SIMPLEX, 0.75, cv::Scalar(130, 135, 145), 2, cv::LINE_AA);
    }

    // D. Granule Morphology & Particle Sizing Card
    cv::Rect morphCard(sbX, 495, sbWidth, 235);
    drawCardBackground(canvas, morphCard, "GRANULE MORPHOLOGY & SIZING (WATERSHED)");

    if (lastResult.success) {
        int rowY = 540;
        int col1 = sbX + 25;
        int col2 = sbX + 270;

        // D10 / D50 / D90 percentiles
        std::ostringstream sD10, sD50, sD90;
        sD10 << "D10 (Fine):  " << std::fixed << std::setprecision(2) << lastResult.d10_mm << " mm";
        sD50 << "D50 (Median):" << std::fixed << std::setprecision(2) << lastResult.d50_mm << " mm";
        sD90 << "D90 (Coarse):" << std::fixed << std::setprecision(2) << lastResult.d90_mm << " mm";

        cv::putText(canvas, sD10.str(), cv::Point(col1, rowY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(210, 215, 225), 1, cv::LINE_AA);
        cv::putText(canvas, sD50.str(), cv::Point(col1, rowY + 32),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(0, 220, 255), 2, cv::LINE_AA);
        cv::putText(canvas, sD90.str(), cv::Point(col1, rowY + 64),
                    cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(210, 215, 225), 1, cv::LINE_AA);

        // Total count & Clumps
        std::ostringstream sCount, sClump, sRatio, sIR;
        sCount << "Total Granules: " << lastResult.totalGranules;
        sClump << "Wet Clumps:     " << lastResult.clumpCount;
        sRatio << "Clump Area:     " << std::fixed << std::setprecision(1) << (lastResult.clumpRatio * 100.0) << " %";
        sIR    << "Mean 850nm IR:  " << std::fixed << std::setprecision(1) << lastResult.meanIR << " DN";

        cv::Scalar clumpColor = (lastResult.clumpCount > 0) ? cv::Scalar(0, 60, 255) : cv::Scalar(0, 220, 100);

        cv::putText(canvas, sCount.str(), cv::Point(col2, rowY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.52, cv::Scalar(210, 215, 225), 1, cv::LINE_AA);
        cv::putText(canvas, sClump.str(), cv::Point(col2, rowY + 32),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, clumpColor, 2, cv::LINE_AA);
        cv::putText(canvas, sRatio.str(), cv::Point(col2, rowY + 64),
                    cv::FONT_HERSHEY_SIMPLEX, 0.52, clumpColor, 1, cv::LINE_AA);

        // Divider & IR intensity
        cv::line(canvas, cv::Point(col1, rowY + 95), cv::Point(sbX + sbWidth - 25, rowY + 95),
                 cv::Scalar(55, 60, 72), 1);
        cv::putText(canvas, sIR.str(), cv::Point(col1, rowY + 122),
                    cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(255, 220, 100), 2, cv::LINE_AA);

        std::string formStr = "Formula: Moisture = " + std::to_string(m_config.alpha).substr(0, 4) +
                              " - " + std::to_string(m_config.beta).substr(0, 5) + "*IR + " +
                              std::to_string(m_config.gamma).substr(0, 4) + "*ClumpRatio";
        cv::putText(canvas, formStr, cv::Point(col1, rowY + 152),
                    cv::FONT_HERSHEY_SIMPLEX, 0.42, cv::Scalar(150, 155, 165), 1, cv::LINE_AA);
    } else {
        cv::putText(canvas, "No data available", cv::Point(sbX + 180, 610),
                    cv::FONT_HERSHEY_SIMPLEX, 0.65, cv::Scalar(120, 125, 135), 1, cv::LINE_AA);
    }

    // E. Operator Control & Keyboard Guide Card
    cv::Rect ctrlCard(sbX, 745, sbWidth, 315);
    drawCardBackground(canvas, ctrlCard, "BENCHTOP OPERATOR CONTROLS");

    int cY = 790;
    int kX = sbX + 25;

    struct KeyGuide {
        std::string key;
        std::string desc;
        cv::Scalar color;
    };

    std::vector<KeyGuide> guides = {
        {"[BARCODE SCAN / ENTER]", "Buffer Barcode & Trigger Inspection Scan", cv::Scalar(0, 220, 255)},
        {"[SPACE]",                "Trigger Inspection on Active Bucket ID", cv::Scalar(0, 220, 100)},
        {"[A]",                    "Generate Auto-Incrementing Batch ID",    cv::Scalar(255, 200, 100)},
        {"[N]",                    "Switch Test Sample Plate (Dry / Wet)",   cv::Scalar(255, 150, 220)},
        {"[M]",                    "Manual Bucket ID Input Prompt",          cv::Scalar(200, 200, 200)},
        {"[Q / ESC]",              "Exit Benchtop Inspector",               cv::Scalar(140, 140, 140)}
    };

    for (const auto& g : guides) {
        cv::putText(canvas, g.key, cv::Point(kX, cY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.47, g.color, 1, cv::LINE_AA);
        cv::putText(canvas, g.desc, cv::Point(kX + 200, cY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.44, cv::Scalar(190, 195, 205), 1, cv::LINE_AA);
        cY += 30;
    }

    // System Status Line
    if (!statusMessage.empty()) {
        cv::Rect statusBox(sbX + 16, 995, sbWidth - 32, 45);
        cv::rectangle(canvas, statusBox, cv::Scalar(15, 18, 24), -1);
        cv::rectangle(canvas, statusBox, cv::Scalar(0, 180, 255), 1);
        cv::putText(canvas, statusMessage, cv::Point(statusBox.x + 12, statusBox.y + 28),
                    cv::FONT_HERSHEY_SIMPLEX, 0.48, cv::Scalar(0, 220, 255), 1, cv::LINE_AA);
    }

    return canvas;
}

UIEvent UI::processEvents(const cv::Mat& dashboardCanvas,
                          BucketIdManager& bucketMgr,
                          int waitDelayMs) {
    if (m_headless) {
        return UIEvent::None;
    }

    if (!m_windowCreated) {
        if (!initialize()) {
            return UIEvent::None;
        }
    }

    try {
        cv::imshow(m_windowName, dashboardCanvas);
    } catch (const cv::Exception& e) {
        std::cerr << "[UI] cv::imshow error: " << e.what() << std::endl;
        return UIEvent::None;
    }

    int key = cv::waitKey(waitDelayMs);
    if (key < 0) {
        return UIEvent::None;
    }

    int code = key & 0xFF;

    // If operator is NOT actively typing into buffer, hotkeys are available
    if (!bucketMgr.isBuffering()) {
        if (code == 'q' || code == 'Q') {
            return UIEvent::Quit;
        }
        if (code == 27) { // ESC
            return UIEvent::Quit;
        }
        if (code == ' ') { // SPACE triggers scan
            return UIEvent::TriggerScan;
        }
        if (code == 'a' || code == 'A') {
            return UIEvent::GenerateAutoId;
        }
        if (code == 'n' || code == 'N') {
            return UIEvent::CycleSample;
        }
        if (code == 'm' || code == 'M') {
            return UIEvent::ManualIdPrompt;
        }
    }

    // Process key in BucketIdManager (handles barcode scanner characters, Enter, backspace)
    KeyProcessResult keyRes = bucketMgr.processKeyInput(key);
    if (keyRes.scanTriggered) {
        return UIEvent::TriggerScan;
    }

    return UIEvent::None;
}

} // namespace HypoInspector

