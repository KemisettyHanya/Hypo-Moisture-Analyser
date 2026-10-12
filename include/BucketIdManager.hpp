#pragma once

#include <string>
#include <mutex>
#include <nlohmann/json.hpp>

namespace HypoInspector {

struct KeyProcessResult {
    bool scanTriggered = false;  // True when Enter is pressed and scan should run immediately
    bool bufferChanged = false;  // True when input buffer changed
    std::string scannedId = "";  // The new active Bucket ID committed by Enter
};

class BucketIdManager {
public:
    explicit BucketIdManager(std::string sessionStatePath = "config/session_state.json",
                             std::string batchPrefix = "BATCH");

    // Primary: USB Barcode / QR Scanner (HID keyboard emulation) buffer processor
    // Call this inside GUI cv::waitKey() loop.
    KeyProcessResult processKeyInput(int key);

    // Secondary: Auto-incrementing fallback ID generator (BATCH-<YYYYMMDD>-<0001>)
    std::string generateNextAutoId();

    // Tertiary: Manual override
    void setManualBucketId(const std::string& id);

    // Accessors
    std::string getActiveBucketId() const;
    std::string getInputBuffer() const;
    std::string getLastScannedId() const;
    bool isBuffering() const;
    void clearBuffer();
    std::string getScannerStatus() const;

    // Session State Persistence (config/session_state.json)
    bool loadState(const std::string& filepath = "");
    bool saveState(const std::string& filepath = "") const;

    // Setters
    void setSessionStatePath(const std::string& path);
    void setBatchPrefix(const std::string& prefix);

private:
    std::string getCurrentDateString() const; // Format: YYYYMMDD
    std::string formatAutoId(const std::string& dateStr, int count) const;

    mutable std::mutex m_mutex;
    std::string m_sessionStatePath;
    std::string m_batchPrefix;
    std::string m_activeBucketId;
    std::string m_lastScannedId;
    std::string m_inputBuffer;

    std::string m_lastDate;
    int m_counter = 1;
};

} // namespace HypoInspector

