#include "BucketIdManager.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <filesystem>

namespace HypoInspector {

BucketIdManager::BucketIdManager(std::string sessionStatePath, std::string batchPrefix)
    : m_sessionStatePath(std::move(sessionStatePath)),
      m_batchPrefix(std::move(batchPrefix)) {
    m_lastDate = getCurrentDateString();
    if (!loadState(m_sessionStatePath)) {
        // Initialize fresh ID if no state file exists
        m_activeBucketId = formatAutoId(m_lastDate, m_counter);
        saveState(m_sessionStatePath);
    }
}

std::string BucketIdManager::getCurrentDateString() const {
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
        << std::setw(4) << (tm_buf.tm_year + 1900)
        << std::setw(2) << (tm_buf.tm_mon + 1)
        << std::setw(2) << tm_buf.tm_mday;
    return oss.str();
}

std::string BucketIdManager::formatAutoId(const std::string& dateStr, int count) const {
    std::ostringstream oss;
    oss << m_batchPrefix << "-" << dateStr << "-"
        << std::setfill('0') << std::setw(4) << count;
    return oss.str();
}

KeyProcessResult BucketIdManager::processKeyInput(int key) {
    std::lock_guard<std::mutex> lock(m_mutex);
    KeyProcessResult res;

    // Mask key to byte value if extended keycode passed
    int code = key & 0xFF;

    // Enter / Newline (10 = \n, 13 = \r) - marks end of barcode scanner transmission
    if (code == 10 || code == 13) {
        if (!m_inputBuffer.empty()) {
            m_activeBucketId = m_inputBuffer;
            m_lastScannedId = m_inputBuffer;
            res.scannedId = m_activeBucketId;
            res.scanTriggered = true;
            res.bufferChanged = true;
            m_inputBuffer.clear();
            std::cout << "[BucketIdManager] Barcode/QR scan received: " << m_activeBucketId << std::endl;
            saveState(m_sessionStatePath);
        } else {
            // Buffer empty, enter triggers scan using current active ID (or auto-generates if empty)
            if (m_activeBucketId.empty()) {
                m_activeBucketId = formatAutoId(getCurrentDateString(), m_counter);
            }
            res.scannedId = m_activeBucketId;
            res.scanTriggered = true;
        }
        return res;
    }

    // Backspace (8 or 127)
    if (code == 8 || code == 127) {
        if (!m_inputBuffer.empty()) {
            m_inputBuffer.pop_back();
            res.bufferChanged = true;
        }
        return res;
    }

    // Escape (27) - Clear current buffer
    if (code == 27) {
        if (!m_inputBuffer.empty()) {
            m_inputBuffer.clear();
            res.bufferChanged = true;
        }
        return res;
    }

    // Printable ASCII characters (32 to 126)
    if (code >= 32 && code <= 126) {
        m_inputBuffer.push_back(static_cast<char>(code));
        res.bufferChanged = true;
        return res;
    }

    return res;
}

std::string BucketIdManager::generateNextAutoId() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string today = getCurrentDateString();
    if (today == m_lastDate) {
        m_counter++;
    } else {
        m_lastDate = today;
        m_counter = 1;
    }

    m_activeBucketId = formatAutoId(m_lastDate, m_counter);
    m_inputBuffer.clear();
    saveState(m_sessionStatePath);
    std::cout << "[BucketIdManager] Generated auto ID: " << m_activeBucketId << std::endl;
    return m_activeBucketId;
}

void BucketIdManager::setManualBucketId(const std::string& id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!id.empty()) {
        m_activeBucketId = id;
        m_inputBuffer.clear();
        saveState(m_sessionStatePath);
        std::cout << "[BucketIdManager] Manual ID updated: " << m_activeBucketId << std::endl;
    }
}

std::string BucketIdManager::getActiveBucketId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeBucketId;
}

std::string BucketIdManager::getInputBuffer() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_inputBuffer;
}

std::string BucketIdManager::getLastScannedId() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_lastScannedId;
}

bool BucketIdManager::isBuffering() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_inputBuffer.empty();
}

void BucketIdManager::clearBuffer() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_inputBuffer.clear();
}

std::string BucketIdManager::getScannerStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_inputBuffer.empty()) {
        return "BUFFERING (" + std::to_string(m_inputBuffer.size()) + " chars)";
    }
    return "SCAN READY [USB HID]";
}

bool BucketIdManager::loadState(const std::string& filepath) {
    const std::string path = filepath.empty() ? m_sessionStatePath : filepath;
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    try {
        nlohmann::json j;
        file >> j;
        m_lastDate = j.value("last_date", getCurrentDateString());
        m_counter = j.value("counter", 1);
        m_activeBucketId = j.value("active_bucket_id", formatAutoId(m_lastDate, m_counter));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BucketIdManager] Warning reading session state: " << e.what() << std::endl;
        return false;
    }
}

bool BucketIdManager::saveState(const std::string& filepath) const {
    const std::string path = filepath.empty() ? m_sessionStatePath : filepath;
    try {
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }
        std::ofstream file(path);
        if (!file.is_open()) {
            return false;
        }
        nlohmann::json j;
        j["last_date"] = m_lastDate;
        j["counter"] = m_counter;
        j["active_bucket_id"] = m_activeBucketId;
        file << j.dump(4) << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[BucketIdManager] Warning saving session state: " << e.what() << std::endl;
        return false;
    }
}

void BucketIdManager::setSessionStatePath(const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sessionStatePath = path;
}

void BucketIdManager::setBatchPrefix(const std::string& prefix) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_batchPrefix = prefix;
}

} // namespace HypoInspector

