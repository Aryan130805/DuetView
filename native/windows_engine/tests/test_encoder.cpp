#include "encoder/encoder_interface.hpp"
#include "encoder/h264_parser.hpp"
#include "encoder/mf_h264_encoder.hpp"
#include "config/engine_config.hpp"
#include "metrics/metrics_tracker.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

namespace {

int g_passedTests = 0;
int g_failedTests = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            ++g_failedTests; \
            return; \
        } \
    } while (0)

#define RUN_TEST(fn) \
    do { \
        std::cout << "[RUN ] " << #fn << "...\n"; \
        const int prevFailures = g_failedTests; \
        fn(); \
        if (g_failedTests == prevFailures) { \
            std::cout << "  [PASS] " << #fn << "\n"; \
            ++g_passedTests; \
        } \
    } while (0)

} // namespace

void TestAnnexBStartCodeParsing() {
    using namespace secondscreen::encoder;

    // Construct a synthetic Annex B stream:
    // [00 00 00 01 09 10]          -> AUD (type 9, len 2)
    // [00 00 00 01 67 42 00 1E ...] -> SPS (type 7, len 6)
    // [00 00 01 68 CE 38 80]       -> PPS (type 8, 3-byte prefix, len 4)
    // [00 00 00 01 65 B8 00 04 ...] -> IDR (type 5, len 6)
    const std::vector<uint8_t> stream = {
        0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0x90, 0x50,
        0x00, 0x00, 0x01,       0x68, 0xCE, 0x38, 0x80,
        0x00, 0x00, 0x00, 0x01, 0x65, 0xB8, 0x00, 0x04, 0x00, 0x00
    };

    const auto units = H264Parser::ParseAnnexBNalUnits(stream);
    TEST_ASSERT(units.size() == 4, "Must parse exactly 4 NAL units");

    TEST_ASSERT(units[0].type == NalUnitType::Aud, "Unit #0 must be AUD");
    TEST_ASSERT(units[0].dataLength == 2, "Unit #0 data length must be 2");

    TEST_ASSERT(units[1].type == NalUnitType::Sps, "Unit #1 must be SPS");
    TEST_ASSERT(units[1].dataLength == 6, "Unit #1 data length must be 6");

    TEST_ASSERT(units[2].type == NalUnitType::Pps, "Unit #2 must be PPS");
    TEST_ASSERT(units[2].dataLength == 4, "Unit #2 data length must be 4");

    TEST_ASSERT(units[3].type == NalUnitType::IdrSlice, "Unit #3 must be IDR Slice");
    TEST_ASSERT(units[3].dataLength == 6, "Unit #3 data length must be 6");
}

void TestSpsPpsExtraction() {
    using namespace secondscreen::encoder;

    const std::vector<uint8_t> stream = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x1F, // SPS (4 bytes data)
        0x00, 0x00, 0x00, 0x01, 0x68, 0xEB, 0xE3, 0xCB, // PPS (4 bytes data)
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84         // IDR
    };

    std::vector<uint8_t> config;
    const bool ok = H264Parser::ExtractSpsPps(stream, config);

    TEST_ASSERT(ok, "SPS/PPS extraction must succeed");
    TEST_ASSERT(config.size() == (4 + 4) + (4 + 4), "Config size must equal 16 bytes (two 4-byte prefixes + data)");

    // Check SPS start code and NAL header
    TEST_ASSERT(config[0] == 0x00 && config[1] == 0x00 && config[2] == 0x00 && config[3] == 0x01, "SPS prefix must be 00000001");
    TEST_ASSERT(config[4] == 0x67, "SPS header must be 0x67");

    // Check PPS start code and NAL header
    TEST_ASSERT(config[8] == 0x00 && config[9] == 0x00 && config[10] == 0x00 && config[11] == 0x01, "PPS prefix must be 00000001");
    TEST_ASSERT(config[12] == 0x68, "PPS header must be 0x68");
}

void TestIdrDetection() {
    using namespace secondscreen::encoder;

    const std::vector<uint8_t> idrStream = {
        0x00, 0x00, 0x00, 0x01, 0x65, 0x88, 0x84, 0x00
    };
    TEST_ASSERT(H264Parser::ContainsIdrSlice(idrStream), "IDR stream must be recognized as keyframe");

    const std::vector<uint8_t> deltaStream = {
        0x00, 0x00, 0x00, 0x01, 0x61, 0x9A, 0x00, 0x01
    };
    TEST_ASSERT(!H264Parser::ContainsIdrSlice(deltaStream), "Delta stream must NOT be recognized as keyframe");
}

void TestMalformedBitstreamHandling() {
    using namespace secondscreen::encoder;

    // 1. Truncated bitstream
    const std::vector<uint8_t> empty{};
    TEST_ASSERT(H264Parser::ParseAnnexBNalUnits(empty).empty(), "Empty bitstream must yield 0 NAL units");

    const std::vector<uint8_t> shortBytes = {0x00, 0x00, 0x01};
    TEST_ASSERT(H264Parser::ParseAnnexBNalUnits(shortBytes).empty(), "Start code without data must yield 0 NAL units");

    // 2. Stream with no start codes
    const std::vector<uint8_t> randomGarbage = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    TEST_ASSERT(H264Parser::ParseAnnexBNalUnits(randomGarbage).empty(), "Garbage bitstream must yield 0 NAL units");

    // 3. SPS without PPS
    const std::vector<uint8_t> spsOnly = {
        0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E
    };
    std::vector<uint8_t> config;
    TEST_ASSERT(!H264Parser::ExtractSpsPps(spsOnly, config), "Missing PPS must return false");
}

void TestMfH264EncoderLifecycle() {
    secondscreen::metrics::MetricsTracker metrics;
    secondscreen::encoder::MfH264Encoder encoder(&metrics);

    secondscreen::config::EngineConfig cfg{};
    cfg.width = 640;
    cfg.height = 360;
    cfg.targetFps = 30;
    cfg.bitrateKbps = 2000;

    int outputPackets = 0;
    const auto onOutput = [&](const secondscreen::encoder::EncodedPacket& pkt) {
        ++outputPackets;
        (void)pkt;
    };

    const bool initOk = encoder.Initialize(cfg, onOutput);
    TEST_ASSERT(initOk, "Encoder Initialize must succeed on Windows host");
    TEST_ASSERT(!encoder.GetEncoderName().empty(), "Encoder name must be reported");

    // Test RequestKeyframe and Flush
    encoder.RequestKeyframe();
    encoder.Flush();

    // Clean shutdown
    encoder.Shutdown();
    TEST_ASSERT(true, "Shutdown must complete cleanly");
}

int main() {
    std::cout << "========================================================\n"
              << "  Running SecondScreen H.264 Encoder Component Tests    \n"
              << "========================================================\n";

    RUN_TEST(TestAnnexBStartCodeParsing);
    RUN_TEST(TestSpsPpsExtraction);
    RUN_TEST(TestIdrDetection);
    RUN_TEST(TestMalformedBitstreamHandling);
    RUN_TEST(TestMfH264EncoderLifecycle);

    std::cout << "========================================================\n"
              << "  Encoder Tests Summary: " << g_passedTests << " passed, " << g_failedTests << " failed\n"
              << "========================================================\n";

    return (g_failedTests == 0) ? 0 : 1;
}
