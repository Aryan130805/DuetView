package com.secondscreen.second_screen_android.decoder

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class H264ConfigParserTest {

    @Test
    fun testAnnexBStartCodeParsing() {
        // Stream containing a 4-byte start code and a 3-byte start code
        val stream = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, // SPS (4-byte)
            0x00, 0x00, 0x01, 0x68, 0xCE.toByte(), 0x38, 0x80.toByte() // PPS (3-byte)
        )

        val nals = H264ConfigParser.findNalUnits(stream)
        assertEquals("Should find exactly 2 NAL units", 2, nals.size)

        assertEquals("First NAL should be SPS (type 7)", H264ConfigParser.NAL_TYPE_SPS, nals[0].type)
        assertEquals("First start code length should be 4", 4, nals[0].startCodeLength)

        assertEquals("Second NAL should be PPS (type 8)", H264ConfigParser.NAL_TYPE_PPS, nals[1].type)
        assertEquals("Second start code length should be 3", 3, nals[1].startCodeLength)
    }

    @Test
    fun testValidSpsPpsExtraction() {
        val configPayload = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x40, 0x20, 0x96.toByte(), 0x56,
            0x00, 0x00, 0x00, 0x01, 0x68, 0xCE.toByte(), 0x3C, 0x80.toByte()
        )

        val config = H264ConfigParser.parseConfig(configPayload)
        assertNotNull("Config should not be null", config)

        // Verify SPS starts with 00 00 00 01 67
        assertEquals(0x00.toByte(), config!!.sps[0])
        assertEquals(0x00.toByte(), config.sps[1])
        assertEquals(0x00.toByte(), config.sps[2])
        assertEquals(0x01.toByte(), config.sps[3])
        assertEquals(0x67.toByte(), config.sps[4])

        // Verify PPS starts with 00 00 00 01 68
        assertEquals(0x00.toByte(), config.pps[0])
        assertEquals(0x00.toByte(), config.pps[1])
        assertEquals(0x00.toByte(), config.pps[2])
        assertEquals(0x01.toByte(), config.pps[3])
        assertEquals(0x68.toByte(), config.pps[4])
    }

    @Test
    fun testMissingPpsRejection() {
        // Payload with only SPS, missing PPS
        val spsOnly = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x40, 0x20
        )

        val config = H264ConfigParser.parseConfig(spsOnly)
        assertNull("Config must be rejected when PPS is missing", config)
    }

    @Test
    fun testMissingSpsRejection() {
        // Payload with only PPS, missing SPS
        val ppsOnly = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x68, 0xCE.toByte(), 0x3C, 0x80.toByte()
        )

        val config = H264ConfigParser.parseConfig(ppsOnly)
        assertNull("Config must be rejected when SPS is missing", config)
    }

    @Test
    fun testMalformedOrEmptyPayload() {
        assertNull("Empty payload must return null", H264ConfigParser.parseConfig(byteArrayOf()))

        val randomBytes = byteArrayOf(0x12, 0x34, 0x56, 0x78, 0x9A.toByte())
        assertNull("Random bytes without start codes must return null", H264ConfigParser.parseConfig(randomBytes))
    }

    @Test
    fun testIdrDetection() {
        // Access unit containing AUD (type 9) followed by IDR slice (type 5)
        val idrFrame = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
            0x00, 0x00, 0x00, 0x01, 0x65, 0x88.toByte(), 0x84.toByte(), 0x00
        )
        assertTrue("Should detect IDR slice", H264ConfigParser.containsIdrSlice(idrFrame))

        // Access unit containing non-IDR P-slice (type 1)
        val deltaFrame = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x09, 0x10,
            0x00, 0x00, 0x00, 0x01, 0x41, 0x9A.toByte(), 0x24.toByte(), 0x00
        )
        assertFalse("Should not flag P-slice as IDR", H264ConfigParser.containsIdrSlice(deltaFrame))
    }

    @Test
    fun testStartCodeNormalization() {
        // 3-byte start code for SPS: 00 00 01 67
        val threeByteConfig = byteArrayOf(
            0x00, 0x00, 0x01, 0x67, 0x4D, 0x40,
            0x00, 0x00, 0x01, 0x68, 0xCE.toByte()
        )

        val config = H264ConfigParser.parseConfig(threeByteConfig)
        assertNotNull(config)

        // Ensure SPS is normalized to 4-byte start code
        val expectedSps = byteArrayOf(0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x40)
        assertArrayEquals("SPS should have 4-byte start code", expectedSps, config!!.sps)

        // Ensure PPS is normalized to 4-byte start code
        val expectedPps = byteArrayOf(0x00, 0x00, 0x00, 0x01, 0x68, 0xCE.toByte())
        assertArrayEquals("PPS should have 4-byte start code", expectedPps, config.pps)
    }

    @Test
    fun testComplexAccessUnitWithAudAndSei() {
        // Stream containing AUD (9), SPS (7), PPS (8), SEI (6), and IDR (5)
        val complexStream = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x09, 0x10, // AUD
            0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x40, 0x1F, // SPS
            0x00, 0x00, 0x00, 0x01, 0x68, 0xCE.toByte(), 0x38, // PPS
            0x00, 0x00, 0x00, 0x01, 0x06, 0x05, 0xFF.toByte(), // SEI
            0x00, 0x00, 0x00, 0x01, 0x65, 0x88.toByte() // IDR
        )

        val nals = H264ConfigParser.findNalUnits(complexStream)
        assertEquals("Should identify all 5 NAL units", 5, nals.size)
        assertEquals(H264ConfigParser.NAL_TYPE_AUD, nals[0].type)
        assertEquals(H264ConfigParser.NAL_TYPE_SPS, nals[1].type)
        assertEquals(H264ConfigParser.NAL_TYPE_PPS, nals[2].type)
        assertEquals(H264ConfigParser.NAL_TYPE_SEI, nals[3].type)
        assertEquals(H264ConfigParser.NAL_TYPE_IDR, nals[4].type)

        val config = H264ConfigParser.parseConfig(complexStream)
        assertNotNull("Should extract SPS and PPS from stream containing multiple NAL types", config)
        assertTrue("Should detect IDR in stream", H264ConfigParser.containsIdrSlice(complexStream))
    }

    @Test
    fun testPartialOrCorruptStartCodes() {
        // Bytes resembling start codes but invalid
        val corruptBytes = byteArrayOf(
            0x00, 0x00, 0x02, 0x01, // 00 00 02 is not a start code
            0x00, 0x01, 0x00, 0x00,
            0x00, 0x00, 0x00 // trailing zeroes without '1'
        )
        val nals = H264ConfigParser.findNalUnits(corruptBytes)
        assertTrue("No NAL units should be found in corrupted start code sequence", nals.isEmpty())
        assertNull("Config parsing must fail on corrupt data", H264ConfigParser.parseConfig(corruptBytes))
    }

    @Test
    fun testCodecConfigEqualityAndHashCode() {
        val sps1 = byteArrayOf(0, 0, 0, 1, 0x67, 1)
        val pps1 = byteArrayOf(0, 0, 0, 1, 0x68, 2)
        val config1 = H264ConfigParser.CodecConfig(sps1, pps1)

        val sps2 = byteArrayOf(0, 0, 0, 1, 0x67, 1)
        val pps2 = byteArrayOf(0, 0, 0, 1, 0x68, 2)
        val config2 = H264ConfigParser.CodecConfig(sps2, pps2)

        val sps3 = byteArrayOf(0, 0, 0, 1, 0x67, 9)
        val config3 = H264ConfigParser.CodecConfig(sps3, pps1)

        assertEquals("Equal byte contents must produce equal CodecConfig", config1, config2)
        assertEquals("Equal CodecConfigs must share hashCodes", config1.hashCode(), config2.hashCode())
        assertFalse("Different byte contents must not be equal", config1 == config3)
    }
}
