#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace secondscreen::encoder {

enum class NalUnitType : uint8_t {
    Unspecified = 0,
    NonIdrSlice = 1,
    SliceDataA = 2,
    SliceDataB = 3,
    SliceDataC = 4,
    IdrSlice = 5,
    Sei = 6,
    Sps = 7,
    Pps = 8,
    Aud = 9,
    EndOfSequence = 10,
    EndOfStream = 11,
    Filler = 12
};

struct NalUnit {
    size_t startOffset{0}; // Start of prefix (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
    size_t headerOffset{0}; // Start of NAL header byte
    size_t dataLength{0};   // Length of NAL unit excluding next start code
    NalUnitType type{NalUnitType::Unspecified};
    uint8_t rawType{0};
};

class H264Parser {
public:
    /// Scans Annex B bitstream and extracts all discrete NAL units.
    [[nodiscard]] static std::vector<NalUnit> ParseAnnexBNalUnits(std::span<const uint8_t> bitstream) noexcept;

    /// Checks if access unit contains an IDR slice (type 5).
    [[nodiscard]] static bool ContainsIdrSlice(std::span<const uint8_t> bitstream) noexcept;

    /// Extracts SPS (7) and PPS (8) NAL units into an Annex B formatted configuration block.
    /// Returns true if both SPS and PPS are present, false otherwise.
    [[nodiscard]] static bool ExtractSpsPps(std::span<const uint8_t> bitstream,
                                            std::vector<uint8_t>& outConfig) noexcept;
};

} // namespace secondscreen::encoder
