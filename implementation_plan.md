# Implementation Plan
## Benchtop Cal Hypo Sample Quality & Moisture Station (Ubuntu POC -> RPi 5 Transfer)

### Phase 1: Workspace Scaffolding & Build Configuration
- [ ] Initialize repository structure:
  - config/ (settings.json, session_state.json)
  - data/ (sample_images/, inspections/, batch_log.csv)
  - include/ (Config.hpp, CaptureSource.hpp, SampleAnalyzer.hpp, BucketIdManager.hpp, ReportLogger.hpp, UI.hpp)
  - src/ (Config.cpp, CaptureSource.cpp, SampleAnalyzer.cpp, BucketIdManager.cpp, ReportLogger.cpp, UI.cpp, main.cpp)
  - CMakeLists.txt
- [ ] Configure CMakeLists.txt (OpenCV, Threads, nlohmann_json, optional libcamera flags).

---

### Phase 2: Bucket ID Ingestion Subsystem (BucketIdManager)
- [ ] Define BucketIdManager class:
  - std::string getActiveBucketId()
  - void setManualBucketId(const std::string& id)
  - std::string generateNextAutoId()
  - bool processKeyInput(char keyChar): Accumulate keystrokes from USB Barcode Scanner (HID emulation terminating with '\n').
  - void saveState() / void loadState(): Persist batch auto-increment counter in config/session_state.json.

---

### Phase 3: Ingestion & Capture Abstraction
- [ ] Define abstract interface ICaptureSource:
  - virtual bool initialize() = 0
  - virtual bool captureSample(cv::Mat& outFrame) = 0
  - virtual void close() = 0
- [ ] Implement StaticFileSource (Ubuntu Desktop POC):
  - Ingests pre-loaded images or test video frames.
- [ ] Implement PiCamera3Source (Raspberry Pi 5):
  - V4L2/libcamera capture with manual focus lock and fixed 500us shutter speed.

---

### Phase 4: Granule Morphology & NIR Moisture Pipeline
- [ ] Implement SampleAnalyzer::segmentTrayROI(const cv::Mat& input).
- [ ] Implement SampleAnalyzer::computeMorphology(const cv::Mat& roi):
  - Gaussian blur + Adaptive Gaussian Thresholding.
  - Distance Transform + Watershed for touching granule separation.
  - Calculate D10, D50, D90 and Clumping index (d > 4.5 mm, circularity < 0.65).
- [ ] Implement SampleAnalyzer::estimateMoisture(...):
  - Evaluates masked 850nm NIR reflectance I_mean.
  - Runs regression: Moisture = alpha - (beta * I_mean) + (gamma * clumpRatio).

---

### Phase 5: Reporting & CSV Logging Engine
- [ ] Implement ReportLogger class:
  - Appends record to data/batch_log.csv:
    Timestamp,BucketID,D10_mm,D50_mm,D90_mm,ClumpCount,MeanIR,MoisturePct,Verdict
  - Verifies file exists; writes header line if new file.
  - Calls std::ofstream::flush() after every entry to prevent data loss on power cuts.
  - Exports inspection snapshot to data/inspections/<BucketID>_<timestamp>.jpg.

---

### Phase 6: Operator GUI Dashboard & Scanner Hooks
- [ ] Implement UI class:
  - Header Banner: Displays Active Bucket ID: [ ... ] with input cursor or "SCAN READY" status.
  - Visual Overlay: Green circles for valid granules, red boxes for clumps.
  - Telemetry Sidebar: Moisture %, D50, Clump count, PASS/FAIL badge.
  - Event loop: Ingests barcode scanner HID buffer seamlessly in cv::waitKey().

---

### Phase 7: Verification & RPi 5 Deployment
- [ ] Desktop verification on Ubuntu with simulated barcode scan inputs.
- [ ] Deploy to Raspberry Pi 5 with USB Barcode Scanner + Camera Module 3 NoIR.
