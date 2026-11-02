/* 
 * QR Code generator library (C++)
 * 
 * Copyright (c) Project Nayuki. (MIT License)
 * https://www.nayuki.io/page/qr-code-generator-library
 */

#include "pairing/qrcodegen.hpp"

#include <algorithm>
#include <climits>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

using std::int8_t;
using std::uint8_t;
using std::size_t;
using std::vector;

namespace qrcodegen {

QrSegment::QrSegment(Mode md, int numCh, const vector<bool> &dt) :
    mode(md),
    numChars(numCh),
    data(dt) {
    if (numCh < 0)
        throw std::domain_error("Invalid value");
}

QrSegment::QrSegment(Mode md, int numCh, vector<bool> &&dt) :
    mode(md),
    numChars(numCh),
    data(std::move(dt)) {
    if (numCh < 0)
        throw std::domain_error("Invalid value");
}

QrSegment::Mode QrSegment::getMode() const {
    return mode;
}

int QrSegment::getNumChars() const {
    return numChars;
}

const vector<bool> &QrSegment::getData() const {
    return data;
}

static void appendBits(std::uint32_t val, int len, vector<bool> &bb) {
    for (int i = len - 1; i >= 0; i--)
        bb.push_back(((val >> i) & 1) != 0);
}

QrSegment QrSegment::makeBytes(const vector<uint8_t> &data) {
    if (data.size() > static_cast<unsigned int>(INT_MAX))
        throw std::length_error("Data too long");
    vector<bool> bb;
    for (uint8_t b : data)
        appendBits(b, 8, bb);
    return QrSegment(Mode::BYTE, static_cast<int>(data.size()), std::move(bb));
}

vector<QrSegment> QrSegment::makeSegments(const char *text) {
    vector<uint8_t> bytes;
    for (; *text != '\0'; text++)
        bytes.push_back(static_cast<uint8_t>(*text));
    vector<QrSegment> result;
    result.push_back(makeBytes(bytes));
    return result;
}

int QrSegment::getTotalBits(const vector<QrSegment> &segs, int version) {
    int result = 0;
    for (const QrSegment &seg : segs) {
        int ccbits = 8;
        if (seg.mode == Mode::BYTE) {
            ccbits = (version < 10) ? 8 : 16;
        } else if (seg.mode == Mode::NUMERIC) {
            ccbits = (version < 10) ? 10 : (version < 27 ? 12 : 14);
        } else if (seg.mode == Mode::ALPHANUMERIC) {
            ccbits = (version < 10) ? 9 : (version < 27 ? 11 : 13);
        }
        result += 4 + ccbits + static_cast<int>(seg.data.size());
    }
    return result;
}

// QrCode tables
static const int8_t ECC_CODEWORDS_PER_BLOCK[4][41] = {
    // Version: (0), 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40
    {-1,  7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // Low
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28}, // Medium
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // Quartile
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30}, // High
};

static const int8_t NUM_ERROR_CORRECTION_BLOCKS[4][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8,  9,  9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};

static int getNumDataCodewords(int ver, QrCode::Ecc ecl) {
    return (ver * 4 + 17) * (ver * 4 + 17) / 8
        - ECC_CODEWORDS_PER_BLOCK[static_cast<int>(ecl)][ver]
        * NUM_ERROR_CORRECTION_BLOCKS[static_cast<int>(ecl)][ver];
}

QrCode QrCode::encodeText(const char *text, Ecc ecl) {
    vector<QrSegment> segs = QrSegment::makeSegments(text);
    return encodeSegments(segs, ecl);
}

QrCode QrCode::encodeSegments(const vector<QrSegment> &segs, Ecc ecl, int minVersion, int maxVersion, int mask, bool) {
    int version = minVersion;
    int dataUsedBits = 0;
    for (;; version++) {
        int dataCapacityBits = getNumDataCodewords(version, ecl) * 8;
        dataUsedBits = QrSegment::getTotalBits(segs, version);
        if (dataUsedBits <= dataCapacityBits)
            break;
        if (version >= maxVersion)
            throw std::length_error("Data too long for QR Code");
    }

    vector<bool> bb;
    for (const QrSegment &seg : segs) {
        appendBits(1 << static_cast<int>(seg.getMode()), 4, bb);
        int ccbits = (seg.getMode() == QrSegment::Mode::BYTE) ? (version < 10 ? 8 : 16) : 8;
        appendBits(static_cast<uint32_t>(seg.getNumChars()), ccbits, bb);
        bb.insert(bb.end(), seg.getData().begin(), seg.getData().end());
    }

    int dataCapacityBits = getNumDataCodewords(version, ecl) * 8;
    appendBits(0, std::min(4, dataCapacityBits - static_cast<int>(bb.size())), bb);
    appendBits(0, (8 - static_cast<int>(bb.size()) % 8) % 8, bb);

    for (uint8_t padByte = 0xEC; static_cast<int>(bb.size()) < dataCapacityBits; padByte ^= 0xEC ^ 0x11)
        appendBits(padByte, 8, bb);

    vector<uint8_t> dataCodewords(bb.size() / 8);
    for (size_t i = 0; i < bb.size(); i++)
        dataCodewords[i >> 3] |= (bb[i] ? 1 : 0) << (7 - (i & 7));

    return QrCode(version, ecl, dataCodewords, mask);
}

QrCode::QrCode(int ver, Ecc ecl, const vector<uint8_t> &dataCodewords, int msk) :
    version(ver),
    size(ver * 4 + 17),
    errorCorrectionLevel(ecl),
    modules(size, vector<bool>(size, false)),
    isFunction(size, vector<bool>(size, false)) {

    drawFunctionPatterns();
    const vector<uint8_t> allCodewords = addEccAndInterleave(dataCodewords);
    drawCodewords(allCodewords);

    if (msk < 0) {
        long minPenalty = LONG_MAX;
        int bestMask = 0;
        for (int i = 0; i < 8; i++) {
            applyMask(i);
            drawFormatBits(i);
            long penalty = getPenaltyScore();
            if (penalty < minPenalty) {
                minPenalty = penalty;
                bestMask = i;
            }
            applyMask(i);
        }
        mask = bestMask;
    } else {
        mask = msk;
    }
    applyMask(mask);
    drawFormatBits(mask);
    isFunction.clear();
}

int QrCode::getVersion() const { return version; }
int QrCode::getSize() const { return size; }
QrCode::Ecc QrCode::getErrorCorrectionLevel() const { return errorCorrectionLevel; }
int QrCode::getMask() const { return mask; }
bool QrCode::getModule(int x, int y) const { return (0 <= x && x < size && 0 <= y && y < size) && modules[y][x]; }

void QrCode::drawFunctionPatterns() {
    for (int i = 0; i < size; i++) {
        setFunctionModule(6, i, i % 2 == 0);
        setFunctionModule(i, 6, i % 2 == 0);
    }
    drawFinderPattern(3, 3);
    drawFinderPattern(size - 4, 3);
    drawFinderPattern(3, size - 4);

    if (version >= 2) {
        static const int ALIGNMENT_PATTERN_BASE[41] = {
            0, 0, 18, 22, 26, 30, 34, 22, 24, 26, 28, 30, 32, 34, 26, 28, 30, 32, 34, 36,
            28, 30, 32, 34, 36, 38, 30, 32, 34, 36, 38, 40, 32, 34, 36, 38, 40, 42, 34, 36, 38
        };
        int numAlign = version / 7 + 2;
        int step = (version == 32) ? 26 : (version * 4 + numAlign * 2 + 1) / (numAlign * 2 - 2) * 2;
        vector<int> pos(numAlign);
        pos[0] = 6;
        for (int i = numAlign - 1, p = size - 7; i >= 1; i--, p -= step)
            pos[i] = p;

        for (int i = 0; i < numAlign; i++) {
            for (int j = 0; j < numAlign; j++) {
                if (!((i == 0 && j == 0) || (i == 0 && j == numAlign - 1) || (i == numAlign - 1 && j == 0)))
                    drawAlignmentPattern(pos[i], pos[j]);
            }
        }
    }

    drawFormatBits(0);
    drawVersion();
}

void QrCode::drawFormatBits(int msk) {
    int data = static_cast<int>(errorCorrectionLevel) ^ 1;
    data = (data << 3) | msk;
    int rem = data;
    for (int i = 0; i < 10; i++)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    int bits = ((data << 10) | rem) ^ 0x5412;

    for (int i = 0; i <= 5; i++)
        setFunctionModule(8, i, getBit(bits, i));
    setFunctionModule(8, 7, getBit(bits, 6));
    setFunctionModule(8, 8, getBit(bits, 7));
    setFunctionModule(7, 8, getBit(bits, 8));
    for (int i = 9; i < 15; i++)
        setFunctionModule(14 - i, 8, getBit(bits, i));

    for (int i = 0; i < 8; i++)
        setFunctionModule(size - 1 - i, 8, getBit(bits, i));
    for (int i = 8; i < 15; i++)
        setFunctionModule(8, size - 15 + i, getBit(bits, i));
    setFunctionModule(8, size - 8, true);
}

void QrCode::drawVersion() {
    if (version < 7)
        return;
    int rem = version;
    for (int i = 0; i < 12; i++)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    long bits = (static_cast<long>(version) << 12) | rem;
    for (int i = 0; i < 18; i++) {
        bool bit = getBit(bits, i);
        int a = size - 11 + i % 3;
        int b = i / 3;
        setFunctionModule(a, b, bit);
        setFunctionModule(b, a, bit);
    }
}

void QrCode::drawFinderPattern(int x, int y) {
    for (int dy = -4; dy <= 4; dy++) {
        for (int dx = -4; dx <= 4; dx++) {
            int dist = std::max(std::abs(dx), std::abs(dy));
            int xx = x + dx, yy = y + dy;
            if (0 <= xx && xx < size && 0 <= yy && yy < size)
                setFunctionModule(xx, yy, dist != 2 && dist != 4);
        }
    }
}

void QrCode::drawAlignmentPattern(int x, int y) {
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++)
            setFunctionModule(x + dx, y + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }
}

void QrCode::setFunctionModule(int x, int y, bool isBlack) {
    modules[y][x] = isBlack;
    isFunction[y][x] = true;
}

static uint8_t reedSolomonMultiply(uint8_t x, uint8_t y) {
    int z = 0;
    for (int i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 8) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return static_cast<uint8_t>(z);
}

static vector<uint8_t> reedSolomonComputeDivisor(int degree) {
    vector<uint8_t> result(degree, 0);
    result[degree - 1] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; i++) {
        for (size_t j = 0; j < result.size(); j++) {
            result[j] = reedSolomonMultiply(result[j], root);
            if (j + 1 < result.size())
                result[j] ^= result[j + 1];
        }
        root = reedSolomonMultiply(root, 0x02);
    }
    return result;
}

static vector<uint8_t> reedSolomonComputeRemainder(const vector<uint8_t> &data, const vector<uint8_t> &divisor) {
    vector<uint8_t> result(divisor.size(), 0);
    for (uint8_t b : data) {
        uint8_t factor = b ^ result[0];
        result.erase(result.begin());
        result.push_back(0);
        for (size_t i = 0; i < divisor.size(); i++)
            result[i] ^= reedSolomonMultiply(divisor[i], factor);
    }
    return result;
}

vector<uint8_t> QrCode::addEccAndInterleave(const vector<uint8_t> &data) const {
    int numBlocks = NUM_ERROR_CORRECTION_BLOCKS[static_cast<int>(errorCorrectionLevel)][version];
    int blockEccLen = ECC_CODEWORDS_PER_BLOCK[static_cast<int>(errorCorrectionLevel)][version];
    int rawCodewords = (version * 4 + 17) * (version * 4 + 17) / 8;
    int numShortBlocks = numBlocks - rawCodewords % numBlocks;
    int shortBlockLen = rawCodewords / numBlocks;

    vector<vector<uint8_t>> blocks;
    const vector<uint8_t> rsDivisor = reedSolomonComputeDivisor(blockEccLen);
    for (int i = 0, k = 0; i < numBlocks; i++) {
        int blockLen = shortBlockLen - blockEccLen + (i >= numShortBlocks ? 1 : 0);
        vector<uint8_t> dat(data.begin() + k, data.begin() + (k + blockLen));
        k += blockLen;
        vector<uint8_t> ecc = reedSolomonComputeRemainder(dat, rsDivisor);
        dat.insert(dat.end(), ecc.begin(), ecc.end());
        blocks.push_back(std::move(dat));
    }

    vector<uint8_t> result;
    for (size_t i = 0; i < blocks[0].size(); i++) {
        for (size_t j = 0; j < blocks.size(); j++) {
            if (i < blocks[j].size())
                result.push_back(blocks[j][i]);
        }
    }
    return result;
}

void QrCode::drawCodewords(const vector<uint8_t> &data) {
    size_t i = 0;
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;
        for (int vert = 0; vert < size; vert++) {
            for (int j = 0; j < 2; j++) {
                int x = right - j;
                bool upwards = ((right + 1) & 2) == 0;
                int y = upwards ? size - 1 - vert : vert;
                if (!isFunction[y][x] && i < data.size() * 8) {
                    modules[y][x] = getBit(data[i >> 3], 7 - (i & 7));
                    i++;
                }
            }
        }
    }
}

void QrCode::applyMask(int msk) {
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool invert = false;
            switch (msk) {
                case 0:  invert = (x + y) % 2 == 0;                    break;
                case 1:  invert = y % 2 == 0;                          break;
                case 2:  invert = x % 3 == 0;                          break;
                case 3:  invert = (x + y) % 3 == 0;                    break;
                case 4:  invert = (x / 3 + y / 2) % 2 == 0;            break;
                case 5:  invert = x * y % 2 + x * y % 3 == 0;          break;
                case 6:  invert = (x * y % 2 + x * y % 3) % 2 == 0;    break;
                case 7:  invert = ((x + y) % 2 + x * y % 3) % 2 == 0;  break;
                default: break;
            }
            modules[y][x] = modules[y][x] ^ (invert && !isFunction[y][x]);
        }
    }
}

long QrCode::getPenaltyScore() const {
    long result = 0;
    for (int y = 0; y < size; y++) {
        bool runColor = false;
        int runX = 0;
        for (int x = 0; x < size; x++) {
            if (modules[y][x] == runColor) {
                runX++;
                if (runX == 5) result += 3;
                else if (runX > 5) result++;
            } else {
                runColor = modules[y][x];
                runX = 1;
            }
        }
    }
    return result;
}

bool QrCode::getBit(long x, int i) {
    return ((x >> i) & 1) != 0;
}

std::string QrCode::toSvgString(int border) const {
    std::ostringstream sb;
    sb << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    sb << "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" viewBox=\"0 0 ";
    sb << (size + border * 2) << " " << (size + border * 2) << "\" stroke=\"none\">\n";
    sb << "\t<rect width=\"100%\" height=\"100%\" fill=\"#FFFFFF\"/>\n";
    sb << "\t<path d=\"";
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (getModule(x, y)) {
                if (x != 0 || y != 0)
                    sb << " ";
                sb << "M" << (x + border) << "," << (y + border) << "h1v1h-1z";
            }
        }
    }
    sb << "\" fill=\"#000000\"/>\n";
    sb << "</svg>\n";
    return sb.str();
}

} // namespace qrcodegen

