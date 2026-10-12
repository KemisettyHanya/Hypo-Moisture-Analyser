#include "CaptureSource.hpp"
#include <iostream>
#include <filesystem>
#include <random>
#include <cmath>

namespace HypoInspector {

// -------------------------------------------------------------
// Synthetic Sample Generator
// -------------------------------------------------------------

static void drawPetriDishBase(cv::Mat& img, cv::Point center, int radius) {
    // Fill background chamber
    img.setTo(cv::Scalar(20, 20, 22));

    // Dish exterior shadow & acrylic rim
    cv::circle(img, center, radius + 15, cv::Scalar(32, 32, 36), 8);
    cv::circle(img, center, radius + 6, cv::Scalar(75, 78, 85), 6);
    cv::circle(img, center, radius, cv::Scalar(95, 100, 108), 2);

    // Matte dark dish floor (calibrated 48-52 DN)
    cv::circle(img, center, radius - 1, cv::Scalar(48, 50, 52), -1);
}

cv::Mat SyntheticSampleGenerator::generateDrySamplePlate(int width, int height) {
    cv::Mat img = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Point center(width / 2, height / 2);
    int dishRadius = 460;
    drawPetriDishBase(img, center, dishRadius);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> distAngle(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> distRad(25.0, dishRadius - 45.0);
    std::uniform_real_distribution<double> distGranuleR(15.0, 26.0); // 1.5 to 2.6 mm
    std::uniform_real_distribution<double> distIntensity(175.0, 195.0); // Bright NIR reflectance
    std::uniform_real_distribution<double> distDeform(0.92, 1.08);

    std::vector<cv::Point> centers;
    std::vector<double> radii;
    int maxAttempts = 300;

    for (int i = 0; i < maxAttempts; ++i) {
        double theta = distAngle(rng);
        double r = std::sqrt(distRad(rng) / (dishRadius - 45.0)) * (dishRadius - 45.0);
        int gx = static_cast<int>(center.x + r * std::cos(theta));
        int gy = static_cast<int>(center.y + r * std::sin(theta));
        double rad = distGranuleR(rng);

        bool tooClose = false;
        for (size_t j = 0; j < centers.size(); ++j) {
            double dx = gx - centers[j].x;
            double dy = gy - centers[j].y;
            if (std::sqrt(dx * dx + dy * dy) < (rad + radii[j]) * 1.08) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            centers.push_back(cv::Point(gx, gy));
            radii.push_back(rad);
            double intensity = distIntensity(rng);

            std::vector<cv::Point> pts;
            const int numVerts = 14;
            for (int v = 0; v < numVerts; ++v) {
                double angle = (2.0 * M_PI * v) / numVerts;
                double rLocal = rad * distDeform(rng);
                int px = static_cast<int>(gx + rLocal * std::cos(angle));
                int py = static_cast<int>(gy + rLocal * std::sin(angle));
                pts.push_back(cv::Point(px, py));
            }
            std::vector<std::vector<cv::Point>> contours = {pts};
            cv::fillPoly(img, contours, cv::Scalar(intensity * 0.90, intensity * 0.90, intensity * 0.90));
            cv::circle(img, cv::Point(gx, gy), static_cast<int>(rad * 0.45),
                       cv::Scalar(intensity, intensity, intensity), -1);
        }
    }

    cv::Mat noise = cv::Mat(img.size(), CV_8UC3);
    cv::randn(noise, cv::Scalar(0, 0, 0), cv::Scalar(2, 2, 2));
    cv::add(img, noise, img);

    return img;
}

cv::Mat SyntheticSampleGenerator::generateWetSamplePlate(int width, int height) {
    cv::Mat img = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Point center(width / 2, height / 2);
    int dishRadius = 460;
    drawPetriDishBase(img, center, dishRadius);

    std::mt19937 rng(1337);
    std::uniform_real_distribution<double> distAngle(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> distRad(40.0, dishRadius - 60.0);
    // Water absorbs 850nm NIR -> darker reflectance (mean ~ 104 - 118 DN)
    std::uniform_real_distribution<double> distIntensity(104.0, 118.0);

    // 1. Generate 7 large wet agglomerates / caked clumps (d > 4.5 mm, irregular circularity < 0.65)
    int numClumps = 7;
    for (int c = 0; c < numClumps; ++c) {
        double theta = distAngle(rng);
        double r = std::sqrt(distRad(rng) / (dishRadius - 60.0)) * (dishRadius - 80.0);
        int cx = static_cast<int>(center.x + r * std::cos(theta));
        int cy = static_cast<int>(center.y + r * std::sin(theta));

        double clumpVal = distIntensity(rng);

        std::vector<cv::Point> clumpPts;
        const int lobes = 24;
        double baseRadius = 56.0 + (c % 4) * 8.0; // 56 to 80 pixels -> 5.6 to 8.0 mm (> 4.5 mm!)
        for (int v = 0; v < lobes; ++v) {
            double angle = (2.0 * M_PI * v) / lobes;
            double modulation = 1.0 + 0.38 * std::sin(3.0 * angle) + 0.22 * std::cos(5.0 * angle);
            double rLocal = baseRadius * modulation;
            int px = static_cast<int>(cx + rLocal * std::cos(angle));
            int py = static_cast<int>(cy + rLocal * std::sin(angle));
            clumpPts.push_back(cv::Point(px, py));
        }

        std::vector<std::vector<cv::Point>> contours = {clumpPts};
        cv::fillPoly(img, contours, cv::Scalar(clumpVal, clumpVal, clumpVal));
        cv::circle(img, cv::Point(cx, cy), static_cast<int>(baseRadius * 0.40),
                   cv::Scalar(clumpVal + 6, clumpVal + 6, clumpVal + 6), -1);
    }

    // 2. Generate surrounding wet individual granules
    std::uniform_real_distribution<double> distGranuleR(14.0, 24.0);
    int numGranules = 70;
    for (int i = 0; i < numGranules; ++i) {
        double theta = distAngle(rng);
        double r = std::sqrt(distRad(rng) / (dishRadius - 60.0)) * (dishRadius - 50.0);
        int gx = static_cast<int>(center.x + r * std::cos(theta));
        int gy = static_cast<int>(center.y + r * std::sin(theta));

        double rad = distGranuleR(rng);
        double val = distIntensity(rng);

        cv::circle(img, cv::Point(gx, gy), static_cast<int>(rad),
                   cv::Scalar(val, val, val), -1);
    }

    cv::Mat noise = cv::Mat(img.size(), CV_8UC3);
    cv::randn(noise, cv::Scalar(0, 0, 0), cv::Scalar(2, 2, 2));
    cv::add(img, noise, img);

    return img;
}

cv::Mat SyntheticSampleGenerator::generateUnderdrySamplePlate(int width, int height) {
    cv::Mat img = cv::Mat::zeros(height, width, CV_8UC3);
    cv::Point center(width / 2, height / 2);
    int dishRadius = 460;
    drawPetriDishBase(img, center, dishRadius);

    std::mt19937 rng(999);
    std::uniform_real_distribution<double> distAngle(0.0, 2.0 * M_PI);
    std::uniform_real_distribution<double> distRad(25.0, dishRadius - 45.0);
    std::uniform_real_distribution<double> distGranuleR(14.0, 24.0); // 1.4 mm to 2.4 mm (< 4.5 mm)
    // Under-dry Cal Hypo is exceptionally bright in 850nm NIR (> 220 DN)
    std::uniform_real_distribution<double> distIntensity(225.0, 242.0);

    std::vector<cv::Point> centers;
    std::vector<double> radii;
    int maxAttempts = 250;

    for (int i = 0; i < maxAttempts; ++i) {
        double theta = distAngle(rng);
        double r = std::sqrt(distRad(rng) / (dishRadius - 45.0)) * (dishRadius - 45.0);
        int gx = static_cast<int>(center.x + r * std::cos(theta));
        int gy = static_cast<int>(center.y + r * std::sin(theta));
        double rad = distGranuleR(rng);

        bool tooClose = false;
        for (size_t j = 0; j < centers.size(); ++j) {
            double dx = gx - centers[j].x;
            double dy = gy - centers[j].y;
            if (std::sqrt(dx * dx + dy * dy) < (rad + radii[j]) * 1.08) {
                tooClose = true;
                break;
            }
        }
        if (!tooClose) {
            centers.push_back(cv::Point(gx, gy));
            radii.push_back(rad);
            double intensity = distIntensity(rng);

            cv::circle(img, cv::Point(gx, gy), static_cast<int>(rad),
                       cv::Scalar(intensity * 0.90, intensity * 0.90, intensity * 0.90), -1);
            cv::circle(img, cv::Point(gx, gy), static_cast<int>(rad * 0.50),
                       cv::Scalar(intensity, intensity, intensity), -1);
        }
    }

    cv::Mat noise = cv::Mat(img.size(), CV_8UC3);
    cv::randn(noise, cv::Scalar(0, 0, 0), cv::Scalar(2, 2, 2));
    cv::add(img, noise, img);

    return img;
}

bool SyntheticSampleGenerator::generateDefaultSamples(const std::string& outputDir) {
    try {
        std::filesystem::create_directories(outputDir);

        std::string dryPath = outputDir + "/dry_sample.png";
        std::string wetPath = outputDir + "/wet_sample.png";
        std::string underdryPath = outputDir + "/underdry_sample.png";

        if (!std::filesystem::exists(dryPath)) {
            std::cout << "[SyntheticGen] Generating synthetic dry sample: " << dryPath << std::endl;
            cv::Mat dry = generateDrySamplePlate();
            cv::imwrite(dryPath, dry);
        }

        if (!std::filesystem::exists(wetPath)) {
            std::cout << "[SyntheticGen] Generating synthetic wet sample: " << wetPath << std::endl;
            cv::Mat wet = generateWetSamplePlate();
            cv::imwrite(wetPath, wet);
        }

        if (!std::filesystem::exists(underdryPath)) {
            std::cout << "[SyntheticGen] Generating synthetic underdry sample: " << underdryPath << std::endl;
            cv::Mat underdry = generateUnderdrySamplePlate();
            cv::imwrite(underdryPath, underdry);
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "[SyntheticGen] Error generating synthetic plates: " << e.what() << std::endl;
        return false;
    }
}

// -------------------------------------------------------------
// StaticFileSource Implementation
// -------------------------------------------------------------

StaticFileSource::StaticFileSource(const Config& config) : m_config(config) {}

StaticFileSource::~StaticFileSource() {
    close();
}

void StaticFileSource::discoverSampleImages() {
    m_sampleFiles.clear();

    // Check directory
    if (std::filesystem::exists(m_config.sampleImageDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(m_config.sampleImageDir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp") {
                    m_sampleFiles.push_back(entry.path().string());
                }
            }
        }
    }

    // Sort for deterministic order
    std::sort(m_sampleFiles.begin(), m_sampleFiles.end());

    // If default file is set, try to place it first
    if (!m_config.defaultSampleImage.empty()) {
        auto it = std::find(m_sampleFiles.begin(), m_sampleFiles.end(), m_config.defaultSampleImage);
        if (it != m_sampleFiles.end()) {
            m_currentIndex = std::distance(m_sampleFiles.begin(), it);
        }
    }
}

bool StaticFileSource::initialize() {
    // Generate synthetic sample plates if directory is empty or default files missing
    SyntheticSampleGenerator::generateDefaultSamples(m_config.sampleImageDir);

    discoverSampleImages();
    if (m_sampleFiles.empty()) {
        std::cerr << "[StaticFileSource] Error: No sample images found in "
                  << m_config.sampleImageDir << std::endl;
        m_initialized = false;
        return false;
    }

    std::cout << "[StaticFileSource] Initialized with " << m_sampleFiles.size()
              << " sample plates. Active: " << getCurrentSamplePath() << std::endl;
    m_initialized = true;
    return true;
}

bool StaticFileSource::captureSample(cv::Mat& outFrame) {
    if (!m_initialized || m_sampleFiles.empty()) {
        if (!initialize()) {
            return false;
        }
    }

    std::string path = getCurrentSamplePath();
    cv::Mat raw = cv::imread(path, cv::IMREAD_COLOR);
    if (raw.empty()) {
        std::cerr << "[StaticFileSource] Failed to load image: " << path << std::endl;
        return false;
    }

    // Resize to target config frame size if needed
    if (raw.cols != m_config.frameWidth || raw.rows != m_config.frameHeight) {
        cv::resize(raw, raw, cv::Size(m_config.frameWidth, m_config.frameHeight));
    }

    // Multi-frame average simulation to suppress high-ISO sensor noise
    int avgCount = std::max(1, m_config.frameAverageCount);
    if (avgCount > 1) {
        cv::Mat accum = cv::Mat::zeros(raw.size(), CV_32FC3);
        cv::Mat rawFloat;
        raw.convertTo(rawFloat, CV_32FC3);

        for (int i = 0; i < avgCount; ++i) {
            cv::Mat noise = cv::Mat(raw.size(), CV_32FC3);
            cv::randn(noise, cv::Scalar(0, 0, 0), cv::Scalar(2.5, 2.5, 2.5));
            cv::Mat frameNoisy = rawFloat + noise;
            cv::accumulate(frameNoisy, accum);
        }

        accum /= static_cast<float>(avgCount);
        accum.convertTo(outFrame, CV_8UC3);
    } else {
        outFrame = raw;
    }

    return true;
}

void StaticFileSource::close() {
    m_initialized = false;
}

std::string StaticFileSource::getSourceName() const {
    return "StaticFileSource (" + getCurrentSamplePath() + ")";
}

void StaticFileSource::cycleNextSample() {
    if (m_sampleFiles.empty()) return;
    m_currentIndex = (m_currentIndex + 1) % m_sampleFiles.size();
    std::cout << "[StaticFileSource] Switched to sample: " << getCurrentSamplePath() << std::endl;
}

void StaticFileSource::setSamplePath(const std::string& path) {
    auto it = std::find(m_sampleFiles.begin(), m_sampleFiles.end(), path);
    if (it != m_sampleFiles.end()) {
        m_currentIndex = std::distance(m_sampleFiles.begin(), it);
    } else {
        m_sampleFiles.push_back(path);
        m_currentIndex = m_sampleFiles.size() - 1;
    }
}

std::string StaticFileSource::getCurrentSamplePath() const {
    if (m_sampleFiles.empty()) return "";
    return m_sampleFiles[m_currentIndex];
}

std::vector<std::string> StaticFileSource::getAvailableSamples() const {
    return m_sampleFiles;
}

// -------------------------------------------------------------
// PiCamera3Source Implementation
// -------------------------------------------------------------

PiCamera3Source::PiCamera3Source(const Config& config) : m_config(config) {}

PiCamera3Source::~PiCamera3Source() {
    close();
}

bool PiCamera3Source::initialize() {
    std::cout << "[PiCamera3Source] Initializing Raspberry Pi Camera Module 3 NoIR on device "
              << m_config.deviceIndex << "..." << std::endl;

    // Try opening V4L2 device index
    m_cap.open(m_config.deviceIndex, cv::CAP_V4L2);
    if (!m_cap.isOpened()) {
        std::cerr << "[PiCamera3Source] Warning: Unable to open V4L2 video device "
                  << m_config.deviceIndex << ". Trying default backend..." << std::endl;
        m_cap.open(m_config.deviceIndex);
    }

    if (!m_cap.isOpened()) {
        std::cerr << "[PiCamera3Source] Hardware camera not available on device "
                  << m_config.deviceIndex << ". (Normal on Ubuntu Desktop POC)" << std::endl;
        m_initialized = false;
        return false;
    }

    // Set resolution and camera properties
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, m_config.frameWidth);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, m_config.frameHeight);
    m_cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25); // Manual exposure mode
    m_cap.set(cv::CAP_PROP_EXPOSURE, static_cast<double>(m_config.shutterSpeedUs) / 10000.0);
    m_cap.set(cv::CAP_PROP_GAIN, m_config.analogGain);
    m_cap.set(cv::CAP_PROP_AUTOFOCUS, 0); // Lock focus to disable VCM autofocus hunt
    m_cap.set(cv::CAP_PROP_FOCUS, m_config.lensPosition);

    m_initialized = true;
    std::cout << "[PiCamera3Source] Camera initialized successfully ("
              << m_config.frameWidth << "x" << m_config.frameHeight << ")." << std::endl;
    return true;
}

bool PiCamera3Source::captureSample(cv::Mat& outFrame) {
    if (!m_initialized) {
        if (!initialize()) {
            return false;
        }
    }

    int avgCount = std::max(1, m_config.frameAverageCount);
    cv::Mat accum = cv::Mat::zeros(m_config.frameHeight, m_config.frameWidth, CV_32FC3);

    cv::Mat singleFrame;
    for (int i = 0; i < avgCount; ++i) {
        if (!m_cap.read(singleFrame) || singleFrame.empty()) {
            std::cerr << "[PiCamera3Source] Frame read failed at step " << i << std::endl;
            return false;
        }
        cv::Mat floatFrame;
        singleFrame.convertTo(floatFrame, CV_32FC3);
        cv::accumulate(floatFrame, accum);
    }

    accum /= static_cast<float>(avgCount);
    accum.convertTo(outFrame, CV_8UC3);
    return true;
}

void PiCamera3Source::close() {
    if (m_cap.isOpened()) {
        m_cap.release();
    }
    m_initialized = false;
}

std::string PiCamera3Source::getSourceName() const {
    return "PiCamera3 (IMX708 NoIR 850nm)";
}

// -------------------------------------------------------------
// Factory
// -------------------------------------------------------------

std::unique_ptr<ICaptureSource> createCaptureSource(const Config& config) {
    if (config.cameraMode == "picamera3" || config.cameraMode == "v4l2") {
        auto piCam = std::make_unique<PiCamera3Source>(config);
        if (piCam->initialize()) {
            return piCam;
        }
        std::cout << "[CaptureFactory] Falling back to StaticFileSource for benchtop simulation." << std::endl;
    }
    return std::make_unique<StaticFileSource>(config);
}

} // namespace HypoInspector
