# Antigravity Task Prompt

## Goal
Implement a standalone C++ (C++17) benchtop quality inspection application for Calcium Hypochlorite grab samples, using OpenCV and CMake on Ubuntu Linux, as specified in spec_requirements.md and implementation_plan.md.

## Key Architectural Updates (Bucket ID & Benchtop Flow)
1. Multi-Modal Bucket ID Management (BucketIdManager):
   - Support USB Barcode / QR Scanner via HID keyboard emulation. Buffer characters in the GUI event loop until a newline ('\n' / Enter) is received, then update active Bucket ID and trigger the scan.
   - Support auto-incrementing fallback: BATCH-<YYYYMMDD>-<0001>.
   - Persist counter state in config/session_state.json.
2. Moisture & Morphology Processing (SampleAnalyzer):
   - 850nm IR reflectance (I_mean) + Agglomeration ratio (d > 4.5 mm, circularity < 0.65).
   - Formula: Moisture % = alpha - (beta * I_mean) + (gamma * clumpRatio).
   - Pass window: 5.5% <= Moisture <= 10.0%.
3. Audit Trail Logging (ReportLogger):
   - Save every test run to data/batch_log.csv (Timestamp,BucketID,D10_mm,D50_mm,D90_mm,ClumpCount,MeanIR,MoisturePct,Verdict).
   - Save annotated image to data/inspections/<BucketID>_<timestamp>.jpg.
4. Desktop Simulation:
   - Abstract capture via ICaptureSource (StaticFileSource for Ubuntu, PiCamera3Source for RPi 5).
   - Generate synthetic sample plates (dry_sample.png, wet_sample.png) on the first run if test images are missing.

## Deliverables Required
1. CMakeLists.txt configured with OpenCV 4, Threads, and C++17 standard.
2. Complete headers and implementations:
   - include/Config.hpp & src/Config.cpp
   - include/BucketIdManager.hpp & src/BucketIdManager.cpp
   - include/CaptureSource.hpp & src/CaptureSource.cpp
   - include/SampleAnalyzer.hpp & src/SampleAnalyzer.cpp
   - include/ReportLogger.hpp & src/ReportLogger.cpp
   - include/UI.hpp & src/UI.cpp
   - src/main.cpp
3. config/settings.json and default config/session_state.json.

Build the project, verify compilation on Ubuntu, and ensure the binary runs cleanly.
