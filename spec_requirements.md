# Specification & System Requirements
## Project: Calcium Hypochlorite Benchtop Sample Inspection & Moisture Estimation System

### 1. System Overview
A benchtop quality control inspection station for Calcium Hypochlorite granules.
The operator extracts a grab sample from a packaged bucket, places the sample tray into the benchtop chamber, assigns or scans the Bucket ID, and triggers an optical scan.

The system utilizes a Raspberry Pi Camera Module 3 NoIR (Sony IMX708) paired with an 850nm Infrared LED Ring Illuminator inside an enclosed light chamber. The software analyzes granule size distribution (D10, D50, D90), identifies wet clumping/agglomeration, and correlates 850nm NIR surface reflectance to compute an estimated moisture percentage.

The software is built with Modern C++ (C++17) and OpenCV, running natively on Ubuntu Linux (Desktop POC) and portable to Raspberry Pi 5 (ARM64).

---

### 2. Functional Requirements

#### 2.1 Bucket ID Ingestion Workflow (Multi-Modal)
1. Primary: USB Barcode / QR Scanner (HID Keyboard Emulation)
   - Plugged into USB, reads pre-printed barcode/QR stickers on buckets.
   - Scanners send characters followed by '\n' (Enter key).
   - Scanning instantly updates the active Bucket ID and arms the optical capture.
2. Secondary: Auto-Incrementing Counter with Batch Prefix
   - Fallback if buckets lack barcodes: BATCH-<YYYYMMDD>-<0001>.
   - Persists sequence index across restarts in config/session_state.json.
3. Tertiary: Touchscreen / Manual Keypad Input
   - Allows operator to press [EDIT ID] on the GUI to override or append lot/shift metadata.

#### 2.2 Sample Capture & Ingestion Abstraction
* Stationary Imaging Workflow:
  * Sample dish placed inside lightbox drawer.
  * Trigger methods: Barcode scan completion ('\n'), GUI button click, physical button press, or SPACE key.
  * Captures a stabilized multi-frame average (3-5 frames) to suppress high-ISO sensor noise.
* Source Abstraction:
  * Mode A (Desktop POC): Loads pre-recorded sample images/videos from data/sample_images/.
  * Mode B (Raspberry Pi 5): Direct capture via libcamera / V4L2 from Camera Module 3 NoIR.
* Camera Module 3 NoIR Controls:
  * Fixed manual focal distance (--lens-position locked) to disable VCM autofocus hunt.
  * Fixed shutter speed (<= 500us) and fixed analog gain.
  * Resolution: 1536x864 or 1920x1080 binned.

#### 2.3 Granule Morphological Processing (Size & Distribution)
* Preprocessing:
  * Grayscale conversion (extracting raw 850nm IR reflectance channel).
  * Circular Region of Interest (ROI) masking to isolate the sample dish.
  * Gaussian smoothing (5x5 kernel) + Adaptive Gaussian Thresholding.
* Separation & Sizing:
  * Distance Transform + Watershed segmentation to separate packed granule contacts.
  * Equivalent circular diameter: d = 2 * sqrt(Area / pi) * scale_factor.
  * Compute cumulative percentiles: D10, D50 (median), and D90.
* Agglomeration & Wet Cake Filter:
  * Contours with d > CLUMP_THRESHOLD_MM (default: 4.5 mm) and circularity (4*pi*Area / Perimeter^2 < 0.65) are flagged as wet agglomerates.

#### 2.4 NIR Reflectance & Moisture Calibration Engine
* Reflectance: Compute mean pixel intensity I_sample across masked granule surface.
* Moisture Regression:
  Moisture % = alpha - (beta * I_sample) + (gamma * (Clump_Area / Total_Granule_Area))
  Coefficients (alpha, beta, gamma) are loaded from config/settings.json.
* Quality Thresholds (Hydrated Cal Hypo UN 2880):
  * Safe Window: 5.5% to 10.0% moisture.
  * Critical Warning: < 5.5% (Under-dry: Spontaneous deflagration risk).
  * Rejection: > 10.0% (Over-wet: Degradation, caking, drum bulging).

#### 2.5 Audit Logging & Operator Display
* Audit Trail CSV (data/batch_log.csv):
  * Schema: Timestamp, BucketID, D10_mm, D50_mm, D90_mm, ClumpCount, MeanIR, MoisturePct, Verdict
  * Automatically writes a flush record immediately upon scan completion.
  * Saves annotated screenshot to data/inspections/<BucketID>_<Timestamp>.jpg.
* Operator GUI Dashboard:
  * Top Header: Active Bucket ID (prominently displayed) + Barcode Scanner connection status indicator.
  * Main Viewport: Segmented granules with green circles and red clump polygons.
  * Measurement Cards: Moisture %, D50, Clump Count, PASS/FAIL Banner.

---

### 3. Non-Functional & Portability Requirements
* Language & Standard: C++17, CMake build system.
* Target Platforms: Ubuntu 22.04/24.04 LTS (x86_64) and Raspberry Pi OS (ARM64).
* Dependencies: OpenCV 4.x (core, imgproc, highgui, videoio), nlohmann_json.
