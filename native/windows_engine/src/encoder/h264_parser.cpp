#include "encoder/h264_parser.hpp"

namespace secondscreen::encoder {

namespace {

bool IsStartCode3(const uint8_t* p) noexcept {
    return p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x01;
}

bool IsStartCode4(const uint8_t* p) noexcept {
    return p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00 && p[3] == 0x01;
}

} // namespace

std::vector<NalUnit> H264Parser::ParseAnnexBNalUnits(std::span<const uint8_t> bitstream) noexcept {
    std::vector<NalUnit> units;
    if (bitstream.size() < 4) {
        return units;
    }

    const uint8_t* data = bitstream.data();
    const size_t size = bitstream.size();

    size_t offset = 0;
    while (offset + 3 <= size) {
        size_t prefixLen = 0;
        if (offset + 4 <= size && IsStartCode4(data + offset)) {
            prefixLen = 4;
        } else if (IsStartCode3(data + offset)) {
            prefixLen = 3;
        } else {
            ++offset;
            continue;
        }

        const size_t nalStart = offset;
        const size_t headerPos = offset + prefixLen;

        if (headerPos >= size) {
            break;
        }

        const uint8_t rawByte = data[headerPos];
        const uint8_t rawType = rawByte & 0x1FU;

        // Find next start code or end of stream
        size_t nextStart = size;
        for (size_t next = headerPos + 1; next + 3 <= size; ++next) {
            if ((next + 4 <= size && IsStartCode4(data + next)) || IsStartCode3(data + next)) {
                nextStart = next;
                break;
            }
        }

        NalUnit unit{};
        unit.startOffset = nalStart;
        unit.headerOffset = headerPos;
        unit.dataLength = nextStart - headerPos;
        unit.rawType = rawType;
        unit.type = (rawType <= static_cast<uint8_t>(NalUnitType::Filler))
                        ? static_cast<NalUnitType>(rawType)
                        : NalUnitType::Unspecified;

        units.push_back(unit);
        offset = nextStart;
    }

    return units;
}

bool H264Parser::ContainsIdrSlice(std::span<const uint8_t> bitstream) noexcept {
    const auto units = ParseAnnexBNalUnits(bitstream);
    for (const auto& u : units) {
        if (u.type == NalUnitType::IdrSlice) {
            return true;
        }
    }
    return false;
}

bool H264Parser::ExtractSpsPps(std::span<const uint8_t> bitstream,
                               std::vector<uint8_t>& outConfig) noexcept {
    const auto units = ParseAnnexBNalUnits(bitstream);

    const NalUnit* spsUnit = nullptr;
    const NalUnit* ppsUnit = nullptr;

    for (const auto& u : units) {
        if (u.type == NalUnitType::Sps && !spsUnit) {
            spsUnit = &u;
        } else if (u.type == NalUnitType::Pps && !ppsUnit) {
            ppsUnit = &u;
        }
        if (spsUnit && ppsUnit) break;
    }

    if (!spsUnit || !ppsUnit) {
        return false;
    }

    outConfig.clear();
    outConfig.reserve(spsUnit->dataLength + ppsUnit->dataLength + 8);

    // Canonical 4-byte start code for SPS
    outConfig.push_back(0x00);
    outConfig.push_back(0x00);
    outConfig.push_back(0x00);
    outConfig.push_back(0x01);
    outConfig.insert(outConfig.end(),
                     bitstream.data() + spsUnit->headerOffset,
                     bitstream.data() + spsUnit->headerOffset + spsUnit->dataLength);

    // Canonical 4-byte start code for PPS
    outConfig.push_back(0x00);
    outConfig.push_back(0x00);
    outConfig.push_back(0x00);
    outConfig.push_back(0x01);
    outConfig.insert(outConfig.end(),
                     bitstream.data() + ppsUnit->headerOffset,
                     bitstream.data() + ppsUnit->headerOffset + ppsUnit->dataLength);

    return true;
}

} // namespace secondscreen::encoder
