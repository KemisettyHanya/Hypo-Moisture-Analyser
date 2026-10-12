#include <iostream>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>

#include "Config.hpp"
#include "BucketIdManager.hpp"
#include "CaptureSource.hpp"
#include "SampleAnalyzer.hpp"
#include "ReportLogger.hpp"

using namespace HypoInspector;

void testBucketIdManager() {
    std::cout << "[TEST] Running testBucketIdManager..." << std::endl;

    std::string testStateFile = "config/test_session_state.json";
    if (std::filesystem::exists(testStateFile)) {
        std::filesystem::remove(testStateFile);
    }

    BucketIdManager mgr(testStateFile, "BATCH");

    // 1. Initial ID should match pattern BATCH-YYYYMMDD-0001
    std::string initId = mgr.getActiveBucketId();
    std::regex autoPattern(R"(BATCH-\d{8}-0001)");
    assert(std::regex_match(initId, autoPattern) && "Initial auto ID does not match BATCH-YYYYMMDD-0001");

    // 2. Auto-increment
    std::string nextId = mgr.generateNextAutoId();
    std::regex nextPattern(R"(BATCH-\d{8}-0002)");
    assert(std::regex_match(nextId, nextPattern) && "Next auto ID does not increment counter to 0002");

    // 3. Scanner HID emulation keystroke buffering
    std::string barcode = "BC-987654";
    for (char c : barcode) {
        KeyProcessResult res = mgr.processKeyInput(static_cast<int>(c));
        assert(!res.scanTriggered && "Scan should not trigger before Enter");
        assert(res.bufferChanged && "Buffer should change on printable char");
    }
    assert(mgr.isBuffering() && "Manager should report isBuffering() == true");
    assert(mgr.getInputBuffer() == barcode && "Buffer content mismatch");

    // Press Enter to complete barcode scan
    KeyProcessResult enterRes = mgr.processKeyInput(10); // '\n'
    assert(enterRes.scanTriggered && "Scan should trigger on newline");
    assert(enterRes.scannedId == barcode && "Scanned ID should equal barcode");
    assert(mgr.getActiveBucketId() == barcode && "Active ID should be updated to scanned barcode");
    assert(!mgr.isBuffering() && "Buffer should be empty after Enter");

    // 4. Backspace test
    mgr.processKeyInput('A');
    mgr.processKeyInput('B');
    mgr.processKeyInput('C');
    assert(mgr.getInputBuffer() == "ABC");
    mgr.processKeyInput(8); // Backspace
    assert(mgr.getInputBuffer() == "AB");
    mgr.processKeyInput(27); // ESC clears
    assert(mgr.getInputBuffer().empty());

    // 5. Persistence test
    assert(std::filesystem::exists(testStateFile) && "Session state file should exist");
    BucketIdManager reloadMgr(testStateFile, "BATCH");
    assert(reloadMgr.getActiveBucketId() == barcode && "Persisted bucket ID did not restore properly");

    std::filesystem::remove(testStateFile);
    std::cout << "[TEST] testBucketIdManager PASSED." << std::endl;
}

void testConfigSerialization() {
    std::cout << "[TEST] Running testConfigSerialization..." << std::endl;
    Config cfg;
    cfg.chamberName = "Chamber-Unit-Test";
    cfg.alpha = 20.0;
    cfg.beta = 0.08;
    cfg.gamma = 11.5;

    std::string tmpCfg = "config/test_settings.json";
    bool saved = cfg.saveToFile(tmpCfg);
    assert(saved && "Failed to save config to file");

    Config loaded;
    bool readSuccess = loaded.loadFromFile(tmpCfg);
    assert(readSuccess && "Failed to load config from file");
    assert(loaded.chamberName == "Chamber-Unit-Test");
    assert(std::abs(loaded.alpha - 20.0) < 1e-6);
    assert(std::abs(loaded.beta - 0.08) < 1e-6);
    assert(std::abs(loaded.gamma - 11.5) < 1e-6);

    std::filesystem::remove(tmpCfg);
    std::cout << "[TEST] testConfigSerialization PASSED." << std::endl;
}

void testSampleAnalyzerAndPhysics() {
    std::cout << "[TEST] Running testSampleAnalyzerAndPhysics..." << std::endl;
    Config cfg;
    cfg.scaleFactorMmPerPixel = 0.05;
    cfg.clumpThresholdMm = 4.5;
    cfg.circularityThreshold = 0.65;
    cfg.alpha = 19.5;
    cfg.beta = 0.075;
    cfg.gamma = 12.0;
    cfg.minMoisturePct = 5.5;
    cfg.maxMoisturePct = 10.0;

    SampleAnalyzer analyzer(cfg);

    // 1. Dry Sample Plate Test
    cv::Mat dryPlate = SyntheticSampleGenerator::generateDrySamplePlate();
    AnalysisResult dryRes = analyzer.analyze(dryPlate, "TEST-DRY-01");

    assert(dryRes.success && "Dry analysis failed");
    std::cout << "  Dry Sample: Moisture=" << dryRes.moisturePct << "%, Verdict=" << dryRes.verdict
              << ", Clumps=" << dryRes.clumpCount << ", D50=" << dryRes.d50_mm << "mm" << std::endl;

    assert(dryRes.verdict == "PASS" && "Dry sample should yield PASS verdict");
    assert(dryRes.moisturePct >= cfg.minMoisturePct && dryRes.moisturePct <= cfg.maxMoisturePct &&
           "Dry moisture should fall within safe window (5.5 - 10.0%)");
    assert(dryRes.d10_mm <= dryRes.d50_mm && dryRes.d50_mm <= dryRes.d90_mm &&
           "Percentiles ordering must be D10 <= D50 <= D90");
    assert(dryRes.clumpCount == 0 && "Dry sample should have zero wet clumps");

    // 2. Wet Sample Plate Test
    cv::Mat wetPlate = SyntheticSampleGenerator::generateWetSamplePlate();
    AnalysisResult wetRes = analyzer.analyze(wetPlate, "TEST-WET-01");

    assert(wetRes.success && "Wet analysis failed");
    std::cout << "  Wet Sample: Moisture=" << wetRes.moisturePct << "%, Verdict=" << wetRes.verdict
              << ", Clumps=" << wetRes.clumpCount << ", D50=" << wetRes.d50_mm << "mm" << std::endl;

    assert(wetRes.verdict == "FAIL (OVER-WET)" && "Wet sample should yield FAIL (OVER-WET)");
    assert(wetRes.moisturePct > cfg.maxMoisturePct && "Wet sample moisture should exceed 10.0%");
    assert(wetRes.clumpCount > 0 && "Wet sample should contain detected clumps");
    assert(wetRes.clumpRatio > 0.10 && "Wet sample clump area ratio should be > 10%");

    // 3. Under-dry Sample Plate Test
    cv::Mat underPlate = SyntheticSampleGenerator::generateUnderdrySamplePlate();
    AnalysisResult underRes = analyzer.analyze(underPlate, "TEST-UNDERDRY-01");

    assert(underRes.success && "Underdry analysis failed");
    std::cout << "  Underdry Sample: Moisture=" << underRes.moisturePct << "%, Verdict=" << underRes.verdict
              << ", MeanIR=" << underRes.meanIR << std::endl;

    assert(underRes.verdict == "FAIL (UNDER-DRY)" && "Underdry sample should yield FAIL (UNDER-DRY)");
    assert(underRes.moisturePct < cfg.minMoisturePct && "Underdry moisture should be < 5.5%");

    std::cout << "[TEST] testSampleAnalyzerAndPhysics PASSED." << std::endl;
}

void testReportLogger() {
    std::cout << "[TEST] Running testReportLogger..." << std::endl;
    Config cfg;
    cfg.batchLogPath = "data/test_log.csv";
    cfg.inspectionsDir = "data/test_inspections";

    if (std::filesystem::exists(cfg.batchLogPath)) std::filesystem::remove(cfg.batchLogPath);
    if (std::filesystem::exists(cfg.inspectionsDir)) std::filesystem::remove_all(cfg.inspectionsDir);

    ReportLogger logger(cfg);

    AnalysisResult res;
    res.success = true;
    res.bucketId = "UNIT-TEST-123";
    res.timestamp = "2026-10-12 12:00:00";
    res.d10_mm = 1.5;
    res.d50_mm = 2.2;
    res.d90_mm = 3.1;
    res.clumpCount = 2;
    res.meanIR = 145.2;
    res.moisturePct = 8.12;
    res.verdict = "PASS";
    res.annotatedImage = cv::Mat::zeros(100, 100, CV_8UC3);

    std::string outImg;
    bool logged = logger.logInspection(res, outImg);
    assert(logged && "Failed to log inspection");
    assert(std::filesystem::exists(cfg.batchLogPath) && "CSV log file not found");
    assert(std::filesystem::exists(outImg) && "Inspection snapshot image not found");

    // Read CSV contents and verify
    std::ifstream file(cfg.batchLogPath);
    std::string line;
    std::getline(file, line); // Header
    assert(line == "Timestamp,BucketID,D10_mm,D50_mm,D90_mm,ClumpCount,MeanIR,MoisturePct,Verdict");
    std::getline(file, line); // Row
    assert(line.find("UNIT-TEST-123") != std::string::npos);
    assert(line.find("PASS") != std::string::npos);

    std::filesystem::remove(cfg.batchLogPath);
    std::filesystem::remove_all(cfg.inspectionsDir);
    std::cout << "[TEST] testReportLogger PASSED." << std::endl;
}

int main() {
    std::cout << "=========================================================\n"
              << " Running Automated Unit & Integration Test Suite\n"
              << "=========================================================\n";
    testBucketIdManager();
    testConfigSerialization();
    testSampleAnalyzerAndPhysics();
    testReportLogger();
    std::cout << "\n>>> ALL 4 TEST SUITES PASSED CLEANLY! <<<\n";
    return 0;
}

