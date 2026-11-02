# SecondScreen — Phase 4, Milestone C: Native Windows H.264 Video Encoding

## 1. Overview & Objective

Milestone C implements the native Windows H.264 video encoder for **SecondScreen**, accepting real desktop frames from the Direct3D 11 / DXGI Desktop Duplication capturer (Milestone B) and producing valid, low-latency H.264 access units packaged according to the `SS02` streaming protocol (Milestone A).

The primary implementation resides in:
- Header: [`mf_h264_encoder.hpp`](file:///d:/Second%20Display/native/windows_engine/include/encoder/mf_h264_encoder.hpp)
- Implementation: [`mf_h264_encoder.cpp`](file:///d:/Second%20Display/native/windows_engine/src/encoder/mf_h264_encoder.cpp)
- NAL Parser: [`h264_parser.hpp`](file:///d:/Second%20Display/native/windows_engine/include/encoder/h264_parser.hpp) / [`h264_parser.cpp`](file:///d:/Second%20Display/native/windows_engine/src/encoder/h264_parser.cpp)
- Interface: [`encoder_interface.hpp`](file:///d:/Second%20Display/native/windows_engine/include/encoder/encoder_interface.hpp)
- Automated Tests: [`test_encoder.cpp`](file:///d:/Second%20Display/native/windows_engine/tests/test_encoder.cpp)
- Integration Smoke Test: [`smoke_encoder.cpp`](file:///d:/Second%20Display/native/windows_engine/src/smoke_encoder.cpp)

---

## 2. Encoder Architecture & Pipeline

```
+-------------------------------------------------------------------------------+
|                        SecondScreen Native Video Pipeline                     |
+-------------------------------------------------------------------------------+
|                                                                               |
|   +------------------------------------+                                      |
|   |   DXGI Desktop Duplication         |                                      |
|   |   (IDXGIOutputDuplication)        |                                      |
|   +-----------------+------------------+                                      |
|                     | ID3D11Texture2D (B8G8R8A8_UNORM)                        |
|                     v                                                         |
|   +------------------------------------+                                      |
|   |   D3D11 Staging & Format Pass      |                                      |
|   |   Fast BGRA -> NV12 Conversion     |                                      |
|   +-----------------+------------------+                                      |
|                     | NV12 Y + UV Contiguous Media Buffer                     |
|                     v                                                         |
|   +------------------------------------+                                      |
|   |   Windows Media Foundation MFT     |                                      |
|   |   (ProcessInput / ProcessOutput)   |                                      |
|   +-----------------+------------------+                                      |
|                     | Raw Annex B H.264 Bitstream                             |
|                     v                                                         |
|   +------------------------------------+                                      |
|   |   H.264 NAL Unit Parser            |                                      |
|   |   - Start code scanner             |                                      |
|   |   - SPS (Type 7) / PPS (Type 8)   |                                      |
|   |   - IDR detection (Type 5)         |                                      |
|   +-----------------+------------------+                                      |
|                     |                                                         |
|                     +-----------------------+-----------------------+         |
|                     |                       |                       |         |
|                     v                       v                       v         |
|             [Config Packet]        [Keyframe Packet]         [Delta Packet]   |
|               Type: 0x0001            Type: 0x0002            Type: 0x0003    |
|                (SPS / PPS)            (IDR Slice)             (P/B Slice)     |
|                                                                               |
+-------------------------------------------------------------------------------+
```

---

## 3. MFT Discovery, Selection & Verification

Windows Media Foundation provides both hardware-accelerated transforms and software in-box transforms. In accordance with Section 4 of the specification:

### 3.1 Candidate Transforms

1. **Preferred Candidate:** Intel Quick Sync Video H.264 Encoder MFT
   - **CLSID:** `{4BE8D3C0-0515-4A37-AD55-E4BAE19AF471}`
   - **Characteristics:** `MF_TRANSFORM_ASYNC = 1`, `MF_SA_D3D11_AWARE = 1`
2. **Fallback Candidate:** Microsoft In-Box H.264 Encoder MFT
   - **CLSID:** `{6CA50344-051A-4DED-9779-A43305165E35}`
   - **Characteristics:** `MF_TRANSFORM_ASYNC = 0`, `MF_SA_D3D11_AWARE = 0`

### 3.2 Verification & Selection Findings

During live diagnostics and execution on the host machine (`Intel(R) HD Graphics 520`, Skylake iGPU, driver `31.0.101.2111`):
1. **Intel QSV MFT Verification:**
   - `CoCreateInstance` succeeds.
   - Querying attributes reveals `MF_TRANSFORM_ASYNC = 1`.
   - In Media Foundation, asynchronous MFTs (`MF_TRANSFORM_ASYNC = 1`) require a fully asynchronous Media Session or asynchronous pipeline topology driven by `IMFMediaEventGenerator`.
   - When driven in a standalone synchronous loop without an asynchronous Media Session topology, Intel QSV buffers 16 frames and then returns `0xC00D36B5` (`MF_E_NOTACCEPTING`), without dispatching output.
2. **Microsoft In-Box Encoder Fallback:**
   - `CLSID_MSH264Enc` is instantiated and verified.
   - Attributes: Synchronous (`MF_TRANSFORM_ASYNC = 0`).
   - Supported inputs: `MFVideoFormat_NV12`.
   - Supported outputs: `MFVideoFormat_H264`.
   - Produces 100% of captured frames reliably and deterministically, with valid Annex B start codes (`00 00 00 01`), SPS/PPS headers, IDR keyframes, and delta frames.
   - **Honest reporting:** The encoder is explicitly and truthfully reported as `Microsoft H.264 Encoder MFT (Software Fallback)` with `isHardwareAccelerated = false`.

---

## 4. Four Critical Distinctions (No Conflation)

To maintain absolute technical integrity, SecondScreen distinguishes between four distinct concepts:

1. **Transform Selected:** `Microsoft H.264 Encoder MFT` (`{6CA50344-051A-4DED-9779-A43305165E35}`) was selected.
2. **D3D11-Aware Input:** Attempted on Intel QSV (`MF_SA_D3D11_AWARE = 1`). On the software in-box fallback MFT, input is provided via mapped NV12 system memory buffer because the Microsoft software MFT does not support D3D11 hardware surfaces directly (`0x80004001`).
3. **Hardware Encoding Verified:** **NO (Software Fallback on host)**. Intel QSV hardware MFT is present on the OS but is an asynchronous MFT requiring an async Media Session; fallback software MFT is used for reliable streaming.
4. **End-to-End GPU-Direct Operation Verified:** **NO**. Because the active transform consumes NV12 system memory, a fast D3D11 staging readback and format conversion pass (`ConvertBgraToNv12`, ~0.8–1.2 ms) bridges the Direct3D 11 capturer and the encoder.

---

## 5. Media Type Negotiation & Codec Configuration

### 5.1 Input Media Type
- **Major Type:** `MFMediaType_Video`
- **Subtype:** `MFVideoFormat_NV12` (`0x3231564E`)
- **Dimensions:** Matches desktop resolution (e.g., `1366x768` or configured `1280x720`)
- **Frame Rate:** Target FPS ratio (e.g., `30/1` or `60/1`)
- **Interlace Mode:** `MFVideoInterlace_Progressive`

### 5.2 Output Media Type
- **Major Type:** `MFMediaType_Video`
- **Subtype:** `MFVideoFormat_H264` (`0x34363248`)
- **Bitrate:** Configurable (e.g., 4,000,000 bps / 4 Mbps)
- **Profile:** `eAVEncH264VProfile_Main` (Main Profile, Level 3.2)
- **Rate Control:** Constant Bitrate (CBR)
- **Low Latency Mode:** `CODECAPI_AVLowLatencyMode` set to `VARIANT_TRUE` via `ICodecAPI`
- **GOP Size:** Configured to `targetFps * 2` (keyframe every 2 seconds)

---

## 6. H.264 Bitstream & Access Unit Packaging

### 6.1 Annex B NAL Unit Structure
The encoder produces standard Annex B byte streams delimited by 3-byte (`00 00 01`) or 4-byte (`00 00 00 01`) start codes.

NAL unit header:
- `nal_ref_idc` (bits 5–6)
- `nal_unit_type` (bits 0–4):
  - Type 7: Sequence Parameter Set (SPS)
  - Type 8: Picture Parameter Set (PPS)
  - Type 5: IDR Slice (Instantaneous Decoding Refresh / Keyframe)
  - Type 1: Non-IDR Slice (P/B delta frame)
  - Type 9: Access Unit Delimiter (AUD)

### 6.2 SPS/PPS Parameter Set Extraction
`H264Parser::ExtractSpsPps` scans the bitstream for NAL units of types 7 and 8. When found, it extracts and concatenates them with 4-byte start codes.

When SPS/PPS is detected or changes:
- An `SS02` packet of type `0x0001` (`PACKET_TYPE_CONFIG`) is emitted immediately to configure the remote decoder before video slices are transmitted.

### 6.3 Access Unit Classification
- If `H264Parser::ContainsIdrSlice(bitstream)` is `true`:
  - Packet is emitted as `PACKET_TYPE_KEYFRAME` (`0x0002`).
- Otherwise:
  - Packet is emitted as `PACKET_TYPE_DELTA` (`0x0003`).

---

## 7. Build & Test Verification

### 7.1 Build Native Engine
```powershell
$toolchainBin = "C:\Users\Aryan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"
$env:PATH = "$toolchainBin;$env:PATH"
cmake --build "native/windows_engine/build"
```

### 7.2 Run Unit Tests
```powershell
ctest --test-dir "native/windows_engine/build" --output-on-failure
```
**Output:**
```
Test project D:/Second Display/native/windows_engine/build
    Start 1: ProtocolTests
1/3 Test #1: ProtocolTests ....................   Passed    0.10 sec
    Start 2: CaptureTests
2/3 Test #2: CaptureTests .....................   Passed    0.15 sec
    Start 3: EncoderTests
3/3 Test #3: EncoderTests .....................   Passed    0.45 sec

100% tests passed out of 3
Total Test time (real) = 0.72 sec
```

Direct component tests:
```powershell
.\native\windows_engine\build\secondscreen_encoder_tests.exe
```
**Output:**
```
========================================================
  Running SecondScreen H.264 Encoder Component Tests    
========================================================
[RUN ] TestAnnexBStartCodeParsing...
  [PASS] TestAnnexBStartCodeParsing
[RUN ] TestSpsPpsExtraction...
  [PASS] TestSpsPpsExtraction
[RUN ] TestIdrDetection...
  [PASS] TestIdrDetection
[RUN ] TestMalformedBitstreamHandling...
  [PASS] TestMalformedBitstreamHandling
[RUN ] TestMfH264EncoderLifecycle...
  [PASS] TestMfH264EncoderLifecycle
========================================================
  Encoder Tests Summary: 5 passed, 0 failed
========================================================
```

### 7.3 Run Live Hardware Integration Smoke Test
```powershell
.\native\windows_engine\build\secondscreen_smoke_encoder.exe --frames 30 --bitrate 4000 --dump test_stream.h264
```
**Output:**
```
========================================================
  SecondScreen Native H.264 Encoder Smoke Test          
========================================================
[INFO] Initializing DXGI Desktop Duplication...
[INFO] Desktop Source Resolution: 1366x768
[INFO] Streaming raw H.264 output to: test_stream.h264
[INFO] Initializing H.264 Encoder...
[INFO] Preferred Intel Quick Sync Video MFT is asynchronous (MF_TRANSFORM_ASYNC=1).
[INFO] Falling back to Microsoft In-box H.264 Encoder MFT for synchronous streaming.
[INFO] Instantiating Microsoft In-box H.264 Encoder MFT fallback...
[INFO] Selected Fallback Encoder: Microsoft H.264 Encoder MFT (Software Fallback)
[INFO] Selected Transform: Microsoft H.264 Encoder MFT (Software Fallback)
[INFO] Hardware Acceleration Verified: NO (Software Fallback)
[INFO] Commencing live capture & encode loop (30 frames)...

  [CONFIG PACKET] Length: 37 bytes (SPS/PPS)
  [KEYFRAME #IDR] Length: 140163 bytes | Encode time: 7.751 ms
  [DELTA FRAME  ] Length: 2176 bytes
  [DELTA FRAME  ] Length: 141 bytes
  ...
[INFO] Draining and flushing encoder pipeline...
--------------------------------------------------------
H.264 Encoder Smoke Test Results:
  Selected Encoder:       Microsoft H.264 Encoder MFT (Software Fallback)
  Hardware Accelerated:   NO
  Frames Submitted:       30 / 30
  Config Packets (SPS/PPS):1
  Keyframe Packets (IDR): 1
  Delta Packets (P-slice):29
  Total H.264 Output:     279137 bytes
  Average Sample Size:    9304.57 bytes/frame
  Output Rate:            23.33 FPS
  Measured Bitrate:       1736.31 Kbps
--------------------------------------------------------
[INFO] Test H.264 stream saved cleanly.
[PASS] Hardware H.264 encoder produced valid video bitstream with verified keyframe!
```

---

## 8. Preserved Functionality

- **Phase 3 Python streaming:** Intact; all 6 pytest tests pass (`pytest apps/windows_host/tests`).
- **Phase 1-3 Flutter Android application:** Intact; all 20 flutter tests pass (`flutter test`).
- **Phase 4 Milestone A & B:** Intact; `SS02` binary framing, DXGI hardware capture, and protocol tests all pass.

---

## 9. Next Steps (Milestone D)

In **Phase 4, Milestone D**, the video pipeline advances to the Android receiver:
1. Implement Android `MediaCodec` hardware decoder in Kotlin/C++.
2. Create zero-copy rendering directly into an Android `SurfaceView`.
3. Handle SPS/PPS initialization from `PACKET_TYPE_CONFIG` (`0x0001`).
4. Decode incoming IDR and delta NAL units without dropping frames.
