#include "SampleAnalyzer.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cmath>

namespace HypoInspector {

SampleAnalyzer::SampleAnalyzer(const Config& config) : m_config(config) {}

void SampleAnalyzer::updateConfig(const Config& config) {
    m_config = config;
}

std::string SampleAnalyzer::getCurrentTimestampString() const {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &tt);
#else
    localtime_r(&tt, &tm_buf);
#endif
    std::ostringstream oss;
    oss << std::setfill('0')
        << std::setw(4) << (tm_buf.tm_year + 1900) << "-"
        << std::setw(2) << (tm_buf.tm_mon + 1) << "-"
        << std::setw(2) << tm_buf.tm_mday << " "
        << std::setw(2) << tm_buf.tm_hour << ":"
        << std::setw(2) << tm_buf.tm_min << ":"
        << std::setw(2) << tm_buf.tm_sec;
    return oss.str();
}

double SampleAnalyzer::computePercentile(const std::vector<double>& sortedValues, double percentile) const {
    if (sortedValues.empty()) return 0.0;
    if (sortedValues.size() == 1) return sortedValues.front();
    if (percentile <= 0.0) return sortedValues.front();
    if (percentile >= 1.0) return sortedValues.back();

    double rank = percentile * (sortedValues.size() - 1);
    size_t lowerIdx = static_cast<size_t>(std::floor(rank));
    size_t upperIdx = static_cast<size_t>(std::ceil(rank));
    double fraction = rank - lowerIdx;

    return (1.0 - fraction) * sortedValues[lowerIdx] + fraction * sortedValues[upperIdx];
}

cv::Mat SampleAnalyzer::segmentDishROI(const cv::Mat& input, cv::Point& outCenter, int& outRadius) const {
    outCenter = cv::Point(m_config.roiCenterX, m_config.roiCenterY);
    outRadius = m_config.roiRadiusPx;

    cv::Mat gray;
    if (input.channels() == 3) {
        cv::cvtColor(input, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = input;
    }

    if (m_config.autoDetectDish) {
        // Optional Hough circle dish auto-detection
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(9, 9), 2.0);
        std::vector<cv::Vec3f> circles;
        cv::HoughCircles(blurred, circles, cv::HOUGH_GRADIENT, 1,
                         blurred.rows / 4, 100, 40,
                         blurred.rows / 4, blurred.rows / 2);
        if (!circles.empty()) {
            outCenter = cv::Point(cvRound(circles[0][0]), cvRound(circles[0][1]));
            outRadius = cvRound(circles[0][2]);
        }
    }

    cv::Mat mask = cv::Mat::zeros(gray.size(), CV_8UC1);
    cv::circle(mask, outCenter, outRadius, cv::Scalar(255), -1);
    return mask;
}

AnalysisResult SampleAnalyzer::analyze(const cv::Mat& rawFrame, const std::string& bucketId) {
    AnalysisResult result;
    result.bucketId = bucketId;
    result.timestamp = getCurrentTimestampString();

    if (rawFrame.empty()) {
        result.success = false;
        result.errorMessage = "Input frame is empty";
        return result;
    }

    // 1. Grayscale & Tray ROI Masking
    cv::Mat gray;
    if (rawFrame.channels() == 3) {
        // Extract 850nm IR channel (or luminance)
        cv::cvtColor(rawFrame, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = rawFrame.clone();
    }

    cv::Point dishCenter;
    int dishRadius = 0;
    cv::Mat dishMask = segmentDishROI(gray, dishCenter, dishRadius);

    // 2. Preprocessing: Gaussian Blur + Adaptive Gaussian Thresholding
    cv::Mat blurred;
    int ksize = std::max(3, (m_config.gaussianBlurKernel | 1)); // Ensure odd
    cv::GaussianBlur(gray, blurred, cv::Size(ksize, ksize), 1.2);

    cv::Mat binaryAdaptive;
    int blockSize = std::max(3, (m_config.adaptiveBlockSize | 1)); // Ensure odd
    cv::adaptiveThreshold(blurred, binaryAdaptive, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                          cv::THRESH_BINARY, blockSize, m_config.adaptiveC);

    // Calibrated dish floor isolation: dish base floor is ~48-52 DN, granules/clumps are >= 95 DN
    cv::Mat floorCutoff;
    cv::threshold(blurred, floorCutoff, 72, 255, cv::THRESH_BINARY);

    // Keep only granules strictly exceeding dish floor level and inside the tray ROI
    cv::Mat binaryMask;
    cv::bitwise_and(floorCutoff, dishMask, binaryMask);

    // Morphological opening to strip 1-pixel background artifacts
    cv::Mat morphKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::Mat binaryClean;
    cv::morphologyEx(binaryMask, binaryClean, cv::MORPH_OPEN, morphKernel);

    // 3. Contact Separation via Distance Transform + Watershed Segmentation
    cv::Mat dist;
    cv::distanceTransform(binaryClean, dist, cv::DIST_L2, 5);

    double minVal = 0.0, maxVal = 0.0;
    cv::minMaxLoc(dist, &minVal, &maxVal);

    cv::Mat binarySeparated = binaryClean.clone();

    if (maxVal > 2.0) {
        // Multi-scale local peak detection on distance transform
        cv::Mat distDilated;
        cv::dilate(dist, distDilated, cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)));

        cv::Mat localPeaks = (dist == distDilated) & (dist > 3.0);
        cv::Mat sureFg;
        localPeaks.convertTo(sureFg, CV_8U);

        cv::Mat markers;
        cv::connectedComponents(sureFg, markers);
        markers = markers + 1; // 1 becomes background offset

        // Background boundary
        cv::Mat sureBg;
        cv::dilate(binaryClean, sureBg, morphKernel, cv::Point(-1, -1), 2);
        cv::Mat unknown;
        cv::subtract(sureBg, sureFg, unknown);

        // Outside dish mask is definitely background
        markers.setTo(1, ~dishMask);
        markers.setTo(1, ~sureBg);
        markers.setTo(0, unknown);

        cv::Mat colorForWatershed;
        if (rawFrame.channels() == 3) {
            colorForWatershed = rawFrame.clone();
        } else {
            cv::cvtColor(rawFrame, colorForWatershed, cv::COLOR_GRAY2BGR);
        }

        cv::watershed(colorForWatershed, markers);

        // Separation lines (-1) split touching contacts in the binary mask
        binarySeparated.setTo(0, markers == -1);
    }

    // 4. Granule Contour Extraction & Morphology Analysis
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    cv::findContours(binarySeparated, contours, hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::vector<double> diameters;
    cv::Mat granuleMask = cv::Mat::zeros(gray.size(), CV_8UC1);

    result.particles.reserve(contours.size());
    result.totalGranules = 0;
    result.clumpCount = 0;
    result.totalGranuleAreaPx = 0.0;
    result.clumpAreaPx = 0.0;

    for (const auto& c : contours) {
        double area = cv::contourArea(c);
        if (area < m_config.minGranuleAreaPx) {
            continue; // Filter noise
        }

        double perimeter = cv::arcLength(c, true);
        if (perimeter <= 0.0) continue;

        // Equivalent circular diameter: d = 2 * sqrt(Area / pi) * scaleFactor
        double d_px = 2.0 * std::sqrt(area / M_PI);
        double d_mm = d_px * m_config.scaleFactorMmPerPixel;

        // Circularity: 4 * pi * Area / Perimeter^2
        double circularity = (4.0 * M_PI * area) / (perimeter * perimeter);
        circularity = std::min(1.0, std::max(0.0, circularity));

        // Agglomerate / Clump filter: d > clumpThresholdMm AND circularity < circularityThreshold
        bool isClump = (d_mm > m_config.clumpThresholdMm) &&
                       (circularity < m_config.circularityThreshold);

        ParticleInfo pInfo;
        pInfo.contour = c;
        pInfo.boundingBox = cv::boundingRect(c);
        pInfo.areaPx = area;
        pInfo.perimeterPx = perimeter;
        pInfo.diameterMm = d_mm;
        pInfo.circularity = circularity;
        pInfo.isClump = isClump;

        // Centroid
        cv::Moments m = cv::moments(c);
        if (m.m00 != 0.0) {
            pInfo.center = cv::Point2f(static_cast<float>(m.m10 / m.m00),
                                       static_cast<float>(m.m01 / m.m00));
        } else {
            pInfo.center = cv::Point2f(pInfo.boundingBox.x + pInfo.boundingBox.width / 2.0f,
                                       pInfo.boundingBox.y + pInfo.boundingBox.height / 2.0f);
        }

        result.particles.push_back(pInfo);
        diameters.push_back(d_mm);

        result.totalGranuleAreaPx += area;
        result.totalGranules++;

        if (isClump) {
            result.clumpCount++;
            result.clumpAreaPx += area;
        }

        // Add to granule surface mask for mean reflectance calculation
        cv::drawContours(granuleMask, std::vector<std::vector<cv::Point>>{c}, -1, cv::Scalar(255), -1);
    }

    // 5. Size Distribution Calculation (Cumulative Percentiles D10, D50, D90)
    std::sort(diameters.begin(), diameters.end());
    result.d10_mm = computePercentile(diameters, 0.10);
    result.d50_mm = computePercentile(diameters, 0.50); // Median
    result.d90_mm = computePercentile(diameters, 0.90);

    // 6. Agglomeration Ratio
    result.clumpRatio = (result.totalGranuleAreaPx > 0.0)
                        ? (result.clumpAreaPx / result.totalGranuleAreaPx)
                        : 0.0;

    // 7. 850nm NIR Reflectance & Moisture Calibration Engine
    // Compute mean intensity I_sample across masked granule surface
    cv::Scalar meanVal = cv::mean(gray, granuleMask);
    result.meanIR = meanVal[0];

    // Regression: Moisture % = alpha - (beta * I_mean) + (gamma * clumpRatio)
    double calcMoisture = m_config.alpha - (m_config.beta * result.meanIR) + (m_config.gamma * result.clumpRatio);
    result.moisturePct = std::max(0.0, calcMoisture);

    // 8. Quality Verdict (Hydrated Cal Hypo UN 2880: 5.5% to 10.0%)
    if (result.moisturePct < m_config.minMoisturePct) {
        result.verdict = "FAIL (UNDER-DRY)";
    } else if (result.moisturePct > m_config.maxMoisturePct) {
        result.verdict = "FAIL (OVER-WET)";
    } else {
        result.verdict = "PASS";
    }

    result.success = true;

    // 9. Generate Annotated Inspection Image
    cv::Mat annotated;
    if (rawFrame.channels() == 3) {
        annotated = rawFrame.clone();
    } else {
        cv::cvtColor(rawFrame, annotated, cv::COLOR_GRAY2BGR);
    }

    // Draw tray boundary
    cv::circle(annotated, dishCenter, dishRadius, cv::Scalar(180, 180, 80), 2, cv::LINE_AA);
    cv::putText(annotated, "TRAY BOUNDARY", cv::Point(dishCenter.x - 70, dishCenter.y - dishRadius + 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.55, cv::Scalar(180, 180, 80), 1, cv::LINE_AA);

    // Draw granules and clumps
    for (const auto& p : result.particles) {
        if (p.isClump) {
            // Clump: Prominent red contour and bounding box
            std::vector<std::vector<cv::Point>> cList = {p.contour};
            cv::drawContours(annotated, cList, -1, cv::Scalar(0, 0, 255), 3, cv::LINE_AA);
            cv::rectangle(annotated, p.boundingBox, cv::Scalar(0, 40, 255), 2);

            std::ostringstream tag;
            tag << "CLUMP d=" << std::fixed << std::setprecision(1) << p.diameterMm
                << "mm C=" << std::setprecision(2) << p.circularity;

            int ty = std::max(20, p.boundingBox.y - 6);
            cv::putText(annotated, tag.str(), cv::Point(p.boundingBox.x, ty),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 0, 0), 2, cv::LINE_AA);
            cv::putText(annotated, tag.str(), cv::Point(p.boundingBox.x, ty),
                        cv::FONT_HERSHEY_SIMPLEX, 0.45, cv::Scalar(0, 80, 255), 1, cv::LINE_AA);
        } else {
            // Normal granule: Green contour and circle
            std::vector<std::vector<cv::Point>> cList = {p.contour};
            cv::drawContours(annotated, cList, -1, cv::Scalar(0, 210, 0), 1, cv::LINE_AA);
        }
    }

    // Top HUD Bar on annotated image
    int hudHeight = 52;
    cv::Mat hudRoi = annotated(cv::Rect(0, 0, annotated.cols, hudHeight));
    cv::Mat hudOverlay = hudRoi.clone();
    hudOverlay.setTo(cv::Scalar(20, 20, 24));
    cv::addWeighted(hudOverlay, 0.85, hudRoi, 0.15, 0.0, hudRoi);

    cv::Scalar verdictColor = (result.verdict == "PASS") ? cv::Scalar(0, 220, 0)
                            : (result.verdict.find("UNDER-DRY") != std::string::npos) ? cv::Scalar(0, 140, 255)
                            : cv::Scalar(0, 0, 255);

    std::ostringstream hudText;
    hudText << "BUCKET: " << result.bucketId
            << " | MOISTURE: " << std::fixed << std::setprecision(2) << result.moisturePct << "%"
            << " [" << result.verdict << "]"
            << " | D50: " << std::setprecision(2) << result.d50_mm << "mm"
            << " | CLUMPS: " << result.clumpCount
            << " | IR: " << std::setprecision(1) << result.meanIR << " DN";

    cv::putText(annotated, hudText.str(), cv::Point(20, 34),
                cv::FONT_HERSHEY_SIMPLEX, 0.75, verdictColor, 2, cv::LINE_AA);

    result.annotatedImage = annotated;
    return result;
}

} // namespace HypoInspector
