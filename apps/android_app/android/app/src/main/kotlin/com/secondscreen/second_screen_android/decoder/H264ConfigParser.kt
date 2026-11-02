package com.secondscreen.second_screen_android.decoder

import java.nio.ByteBuffer

/**
 * H.264 Annex B start-code scanner and parameter-set extractor.
 *
 * Implements compliant parsing of SS02 PACKET_TYPE_CONFIG (0x0001) payloads,
 * locating Sequence Parameter Sets (SPS, NAL Type 7) and Picture Parameter Sets (PPS, NAL Type 8)
 * and normalizing them into standard 4-byte Annex B buffers for Android MediaCodec csd-0 and csd-1.
 */
object H264ConfigParser {

    const val NAL_TYPE_SLICE = 1
    const val NAL_TYPE_IDR = 5
    const val NAL_TYPE_SEI = 6
    const val NAL_TYPE_SPS = 7
    const val NAL_TYPE_PPS = 8
    const val NAL_TYPE_AUD = 9

    private val START_CODE_4 = byteArrayOf(0x00, 0x00, 0x00, 0x01)

    data class NalUnit(
        val type: Int,
        val startCodeOffset: Int,
        val startCodeLength: Int,
        val payloadOffset: Int,
        val totalLength: Int
    )

    data class CodecConfig(
        val sps: ByteArray,
        val pps: ByteArray
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as CodecConfig
            return sps.contentEquals(other.sps) && pps.contentEquals(other.pps)
        }

        override fun hashCode(): Int {
            var result = sps.contentHashCode()
            result = 31 * result + pps.contentHashCode()
            return result
        }
    }

    /**
     * Scans arbitrary byte buffers for 3-byte and 4-byte Annex B start codes.
     */
    fun findNalUnits(data: ByteArray, offset: Int = 0, length: Int = data.size): List<NalUnit> {
        val result = mutableListOf<NalUnit>()
        val end = (offset + length).coerceAtMost(data.size)
        var i = offset

        data class StartCodeMatch(val offset: Int, val len: Int)
        val matches = mutableListOf<StartCodeMatch>()

        while (i <= end - 3) {
            if (data[i] == 0.toByte() && data[i + 1] == 0.toByte()) {
                if (data[i + 2] == 1.toByte()) {
                    matches.add(StartCodeMatch(i, 3))
                    i += 3
                    continue
                } else if (i <= end - 4 && data[i + 2] == 0.toByte() && data[i + 3] == 1.toByte()) {
                    matches.add(StartCodeMatch(i, 4))
                    i += 4
                    continue
                }
            }
            i++
        }

        for (idx in matches.indices) {
            val curr = matches[idx]
            val nextOffset = if (idx + 1 < matches.size) matches[idx + 1].offset else end
            val totalLen = nextOffset - curr.offset
            val payloadOff = curr.offset + curr.len

            if (payloadOff < end) {
                val headerByte = data[payloadOff].toInt() and 0xFF
                val nalType = headerByte and 0x1F
                result.add(
                    NalUnit(
                        type = nalType,
                        startCodeOffset = curr.offset,
                        startCodeLength = curr.len,
                        payloadOffset = payloadOff,
                        totalLength = totalLen
                    )
                )
            }
        }

        return result
    }

    /**
     * Extracts canonical SPS and PPS buffers (each prefixed with 00 00 00 01) from an SS02 CONFIG payload.
     * Returns null if either SPS or PPS is missing or malformed.
     */
    fun parseConfig(payload: ByteArray): CodecConfig? {
        if (payload.isEmpty()) return null

        val nals = findNalUnits(payload)
        var spsBytes: ByteArray? = null
        var ppsBytes: ByteArray? = null

        for (nal in nals) {
            when (nal.type) {
                NAL_TYPE_SPS -> {
                    spsBytes = normalizeTo4ByteStartCode(payload, nal)
                }
                NAL_TYPE_PPS -> {
                    ppsBytes = normalizeTo4ByteStartCode(payload, nal)
                }
            }
        }

        return if (spsBytes != null && ppsBytes != null) {
            CodecConfig(spsBytes, ppsBytes)
        } else {
            null
        }
    }

    /**
     * Inspects an encoded access unit to determine whether it contains an IDR slice (Keyframe).
     */
    fun containsIdrSlice(data: ByteArray): Boolean {
        val nals = findNalUnits(data)
        for (nal in nals) {
            if (nal.type == NAL_TYPE_IDR) {
                return true
            }
        }
        return false
    }

    /**
     * Formats an isolated NAL unit into a buffer prefixed with a canonical 4-byte start code (00 00 00 01).
     */
    private fun normalizeTo4ByteStartCode(data: ByteArray, nal: NalUnit): ByteArray {
        val nalDataLength = nal.totalLength - nal.startCodeLength
        val out = ByteArray(4 + nalDataLength)
        System.arraycopy(START_CODE_4, 0, out, 0, 4)
        System.arraycopy(data, nal.payloadOffset, out, 4, nalDataLength)
        return out
    }
}
