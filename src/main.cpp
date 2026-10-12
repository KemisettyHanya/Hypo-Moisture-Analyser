#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <filesystem>

#include "Config.hpp"
#include "BucketIdManager.hpp"
#include "CaptureSource.hpp"
#include "SampleAnalyzer.hpp"
#include "ReportLogger.hpp"
#include "UI.hpp"

using namespace HypoInspector;

void printUsage(const char* progName) {
    std::cout << "Usage: " << progName << " [options]\n\n"
              << "Options:\n"
              << "  --config <path>       Path to configuration JSON (default: config/settings.json)\n"
              << "  --headless            Run in headless mode (no GUI window)\n"
              << "  --batch <N>           Run N automated inspections on available samples and exit\n"
              << "  --sample <path>       Directly specify initial sample image path\n"
              << "  --bucket-id <id>      Set initial Bucket ID\n"
              << "  --help                Show this help message\n\n"
              << "Benchtop Controls:\n"
              << "  [USB Barcode Scan]   Sends barcode characters terminating with Enter (triggers scan)\n"
              << "  [SPACE]              Trigger optical capture and quality inspection\n"
              << "  [A]                  Generate next auto-incrementing Batch ID (BATCH-<YYYYMMDD>-<0001>)\n"
              << "  [N]                  Switch test sample plate (Dry <-> Wet)\n"
              << "  [M]                  Prompt for manual Bucket ID entry\n"
              << "  [Q / ESC]            Quit application\n";
}

int main(int argc, char* argv[]) {
    std::cout << "=========================================================\n"
              << " Calcium Hypochlorite Benchtop Inspection System\n"
              << " 850nm NIR Reflectance & Granule Morphology Quality Station\n"
              << "=========================================================\n"
              << std::endl;

    std::string configPath = "config/settings.json";
    bool cliHeadless = false;
    int batchCount = -1;
    std::string cliSample = "";
    std::string cliBucketId = "";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--headless") {
            cliHeadless = true;
        } else if (arg == "--batch" && i + 1 < argc) {
            batchCount = std::stoi(argv[++i]);
        } else if (arg == "--sample" && i + 1 < argc) {
            cliSample = argv[++i];
        } else if (arg == "--bucket-id" && i + 1 < argc) {
            cliBucketId = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    // 1. Load System Configuration
    Config config;
    config.loadFromFile(configPath);
    if (cliHeadless) {
        config.headless = true;
    }
    if (!cliSample.empty()) {
        config.defaultSampleImage = cliSample;
    }

    // 2. Initialize Bucket ID Subsystem
    BucketIdManager bucketMgr(config.sessionStatePath, config.batchPrefix);
    if (!cliBucketId.empty()) {
        bucketMgr.setManualBucketId(cliBucketId);
    }
    std::cout << "[Main] Active Bucket ID: " << bucketMgr.getActiveBucketId() << std::endl;

    // 3. Initialize Optical Capture Subsystem
    auto captureSource = createCaptureSource(config);
    if (!captureSource->initialize()) {
        std::cerr << "[Main] Fatal: Failed to initialize capture source." << std::endl;
        return 1;
    }

    // 4. Initialize Analyzer and Audit Logger
    SampleAnalyzer analyzer(config);
    ReportLogger logger(config);

    // 5. Initialize UI Dashboard
    UI ui(config);
    ui.initialize();

    std::cout << "[Main] All subsystems initialized. Starting benchtop inspection workflow..." << std::endl;

    // Helper lambda to run a full inspection cycle
    auto performInspection = [&](const std::string& bucketId, cv::Mat& outFrame, AnalysisResult& outResult) {
        std::cout << "\n---------------------------------------------------------" << std::endl;
        std::cout << "[Inspection] Arming optical scan for Bucket ID: " << bucketId << std::endl;

        if (!captureSource->captureSample(outFrame)) {
            std::cerr << "[Inspection] Capture failed!" << std::endl;
            return false;
        }

        outResult = analyzer.analyze(outFrame, bucketId);
        if (!outResult.success) {
            std::cerr << "[Inspection] Analysis failed: " << outResult.errorMessage << std::endl;
            return false;
        }

        std::string savedSnapshotPath;
        logger.logInspection(outResult, savedSnapshotPath);

        std::cout << "[Inspection Summary]\n"
                  << "  Bucket ID:        " << outResult.bucketId << "\n"
                  << "  Moisture:         " << std::fixed << std::setprecision(2) << outResult.moisturePct << " %\n"
                  << "  Verdict:          " << outResult.verdict << "\n"
                  << "  D10 / D50 / D90:  " << std::setprecision(2)
                  << outResult.d10_mm << " mm / " << outResult.d50_mm << " mm / " << outResult.d90_mm << " mm\n"
                  << "  Granule Count:    " << outResult.totalGranules << "\n"
                  << "  Clump Count:      " << outResult.clumpCount << " (Area: "
                  << std::setprecision(1) << (outResult.clumpRatio * 100.0) << "%)\n"
                  << "  850nm Mean IR:    " << std::setprecision(1) << outResult.meanIR << " DN\n"
                  << "  Snapshot:         " << savedSnapshotPath << "\n"
                  << "---------------------------------------------------------\n";
        return true;
    };

    // -------------------------------------------------------------
    // Automated / Batch / Headless Execution Mode
    // -------------------------------------------------------------
    if (batchCount > 0 || ui.isHeadless()) {
        int runs = (batchCount > 0) ? batchCount : 2;
        std::cout << "[Main] Executing " << runs << " automated inspection runs in headless mode...\n";

        auto* staticSrc = dynamic_cast<StaticFileSource*>(captureSource.get());

        for (int run = 1; run <= runs; ++run) {
            std::string currentId = bucketMgr.getActiveBucketId();
            cv::Mat frame;
            AnalysisResult result;

            performInspection(currentId, frame, result);

            // Render dashboard canvas and save snapshot of full dashboard as well
            cv::Mat dashboard = ui.renderDashboard(result.annotatedImage, result, bucketMgr,
                                                  "Automated Run " + std::to_string(run) + " / " + std::to_string(runs));
            std::string dashPath = config.inspectionsDir + "/dashboard_" + currentId + ".jpg";
            cv::imwrite(dashPath, dashboard);
            std::cout << "[Main] Dashboard render saved: " << dashPath << std::endl;

            if (run < runs) {
                // Advance to next test sample if using StaticFileSource
                if (staticSrc != nullptr) {
                    staticSrc->cycleNextSample();
                }
                // Advance auto-increment ID
                bucketMgr.generateNextAutoId();
            }
        }

        std::cout << "\n[Main] Automated batch run completed successfully. Check logs at: "
                  << config.batchLogPath << std::endl;
        return 0;
    }

    // -------------------------------------------------------------
    // Interactive GUI Benchtop Event Loop
    // -------------------------------------------------------------
    cv::Mat currentFrame;
    AnalysisResult currentResult;
    std::string statusMsg = "READY: Scan Barcode, press ENTER, or press SPACE";

    // Perform initial capture to populate dashboard
    performInspection(bucketMgr.getActiveBucketId(), currentFrame, currentResult);

    auto* staticSrc = dynamic_cast<StaticFileSource*>(captureSource.get());

    bool running = true;
    while (running) {
        // Viewport displays annotated result if available, else raw capture
        cv::Mat viewport = currentResult.annotatedImage.empty() ? currentFrame : currentResult.annotatedImage;

        cv::Mat dashboard = ui.renderDashboard(viewport, currentResult, bucketMgr, statusMsg);
        UIEvent event = ui.processEvents(dashboard, bucketMgr, 30);

        switch (event) {
            case UIEvent::TriggerScan: {
                statusMsg = "SCANNING...";
                std::string scanId = bucketMgr.getActiveBucketId();
                if (performInspection(scanId, currentFrame, currentResult)) {
                    statusMsg = "INSPECTION DONE: " + currentResult.verdict +
                                " (" + std::to_string(currentResult.moisturePct).substr(0, 4) + "%)";
                } else {
                    statusMsg = "INSPECTION ERROR";
                }
                break;
            }

            case UIEvent::GenerateAutoId: {
                std::string newId = bucketMgr.generateNextAutoId();
                statusMsg = "GENERATED AUTO ID: " + newId;
                break;
            }

            case UIEvent::CycleSample: {
                if (staticSrc != nullptr) {
                    staticSrc->cycleNextSample();
                    statusMsg = "SWITCHED SAMPLE: " + staticSrc->getCurrentSamplePath();
                    // Preview new sample
                    captureSource->captureSample(currentFrame);
                    currentResult = AnalysisResult(); // Reset until scanned
                } else {
                    statusMsg = "HARDWARE CAMERA ACTIVE (CYCLE UNAVAILABLE)";
                }
                break;
            }

            case UIEvent::ManualIdPrompt: {
                std::cout << "\n[Manual ID Entry] Enter new Bucket ID: ";
                std::string newId;
                if (std::cin >> newId && !newId.empty()) {
                    bucketMgr.setManualBucketId(newId);
                    statusMsg = "MANUAL ID SET: " + newId;
                }
                break;
            }

            case UIEvent::Quit: {
                std::cout << "[Main] Operator initiated exit. Shutting down...\n";
                running = false;
                break;
            }

            case UIEvent::None:
            default:
                break;
        }
    }

    captureSource->close();
    ui.close();
    std::cout << "[Main] Benchtop Inspection System terminated cleanly." << std::endl;
    return 0;
}

