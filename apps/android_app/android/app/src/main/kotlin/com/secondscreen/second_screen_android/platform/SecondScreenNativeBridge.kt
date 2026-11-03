package com.secondscreen.second_screen_android.platform

import android.app.Activity
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.WindowManager
import com.secondscreen.second_screen_android.decoder.H264Decoder
import io.flutter.plugin.common.BinaryMessenger
import io.flutter.plugin.common.MethodCall
import io.flutter.plugin.common.MethodChannel
import kotlin.concurrent.thread

/**
 * Platform Channel Bridge connecting Flutter with the native Android H264Decoder.
 *
 * Channel: com.secondscreen/decoder_control
 */
class SecondScreenNativeBridge(
    messenger: BinaryMessenger,
    val decoder: H264Decoder,
    private val activity: Activity? = null
) : MethodChannel.MethodCallHandler, H264Decoder.Listener {

    companion object {
        private const val TAG = "SecondScreenNativeBridge"
        const val CHANNEL_NAME = "com.secondscreen/decoder_control"
    }

    private val channel = MethodChannel(messenger, CHANNEL_NAME)
    private val mainHandler = Handler(Looper.getMainLooper())

    init {
        channel.setMethodCallHandler(this)
        decoder.setListener(this)
    }

    override fun onMethodCall(call: MethodCall, result: MethodChannel.Result) {
        when (call.method) {
            "getDecoderCapabilities" -> {
                val caps = decoder.probeCapabilities()
                result.success(
                    mapOf(
                        "codecName" to caps.codecName,
                        "isHardwareAccelerated" to caps.isHardwareAccelerated,
                        "maxWidth" to caps.maxWidth,
                        "maxHeight" to caps.maxHeight,
                        "isSupported" to caps.isSupported
                    )
                )
            }
            "initializeDecoder" -> {
                val width = call.argument<Int>("width") ?: 1280
                val height = call.argument<Int>("height") ?: 720
                val success = decoder.initialize(width, height)
                result.success(success)
            }
            "feedConfigPacket" -> {
                val data = call.argument<ByteArray>("data")
                if (data != null && data.isNotEmpty()) {
                    val success = decoder.feedConfig(data)
                    result.success(success)
                } else {
                    result.error("INVALID_ARGUMENT", "Config packet data must not be empty", null)
                }
            }
            "feedAccessUnit" -> {
                val data = call.argument<ByteArray>("data")
                val timestampUs = call.argument<Long>("timestampUs") ?: 0L
                val isKeyframe = call.argument<Boolean>("isKeyframe") ?: false

                if (data != null && data.isNotEmpty()) {
                    val success = decoder.feedAccessUnit(data, timestampUs, isKeyframe)
                    result.success(success)
                } else {
                    result.error("INVALID_ARGUMENT", "Access unit data must not be empty", null)
                }
            }
            "resetDecoder" -> {
                decoder.reset()
                result.success(true)
            }
            "setKeepScreenOn" -> {
                val enable = call.argument<Boolean>("enable") ?: false
                activity?.let { act ->
                    mainHandler.post {
                        if (enable) {
                            act.window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        } else {
                            act.window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
                        }
                    }
                }
                result.success(true)
            }
            "openTetheringSettings" -> {
                activity?.let { act ->
                    try {
                        val intent = android.content.Intent().apply {
                            action = "android.settings.TETHER_SETTINGS"
                            flags = android.content.Intent.FLAG_ACTIVITY_NEW_TASK
                        }
                        act.startActivity(intent)
                        result.success(true)
                    } catch (e: Exception) {
                        try {
                            val intent = android.content.Intent().apply {
                                action = android.provider.Settings.ACTION_WIRELESS_SETTINGS
                                flags = android.content.Intent.FLAG_ACTIVITY_NEW_TASK
                            }
                            act.startActivity(intent)
                            result.success(true)
                        } catch (e2: Exception) {
                            result.success(false)
                        }
                    }
                } ?: result.success(false)
            }
            "launchUrl" -> {
                val urlString = call.argument<String>("url")
                if (urlString.isNullOrBlank()) {
                    result.success(false)
                } else {
                    activity?.let { act ->
                        try {
                            val uri = android.net.Uri.parse(urlString)
                            val intent = android.content.Intent(android.content.Intent.ACTION_VIEW, uri).apply {
                                flags = android.content.Intent.FLAG_ACTIVITY_NEW_TASK
                            }
                            act.startActivity(intent)
                            result.success(true)
                        } catch (e: Exception) {
                            Log.e(TAG, "Failed to launch url: $urlString", e)
                            result.success(false)
                        }
                    } ?: result.success(false)
                }
            }
            "getMetrics" -> {
                val m = decoder.getMetrics()
                result.success(
                    mapOf(
                        "codecName" to m.codecName,
                        "isHardwareAccelerated" to m.isHardwareAccelerated,
                        "state" to m.state.name,
                        "configuredWidth" to m.configuredWidth,
                        "configuredHeight" to m.configuredHeight,
                        "submittedFrames" to m.submittedFrames,
                        "decodedFrames" to m.decodedFrames,
                        "outputFps" to m.outputFps,
                        "decodeErrors" to m.decodeErrors,
                        "keyframeWaitCount" to m.keyframeWaitCount
                    )
                )
            }
            "runSmokeTest" -> {
                val width = call.argument<Int>("width") ?: 1280
                val height = call.argument<Int>("height") ?: 720
                val targetFrames = call.argument<Int>("frames") ?: 15

                // Run smoke test in background to avoid blocking main thread
                thread(name = "SecondScreen-SmokeTest") {
                    val smokeResult = executeSmokeTest(width, height, targetFrames)
                    mainHandler.post {
                        result.success(smokeResult)
                    }
                }
            }
            else -> result.notImplemented()
        }
    }

    override fun onDecoderStateChanged(state: H264Decoder.State) {
        mainHandler.post {
            channel.invokeMethod("onStateChanged", mapOf("state" to state.name))
        }
    }

    override fun onFrameDecoded(width: Int, height: Int, timestampUs: Long) {
        // High frequency frame notification - avoid spamming Flutter channel on every frame
    }

    override fun onKeyframeRequested() {
        mainHandler.post {
            channel.invokeMethod("onKeyframeRequested", null)
        }
    }

    override fun onDecoderError(error: String, isFatal: Boolean) {
        mainHandler.post {
            channel.invokeMethod("onDecoderError", mapOf("error" to error, "isFatal" to isFatal))
        }
    }

    /**
     * Executes a standalone H.264 decoder smoke test feeding a valid synthetic Annex B stream.
     */
    private fun executeSmokeTest(width: Int, height: Int, frames: Int): Map<String, Any> {
        Log.i(TAG, "Starting standalone H.264 decoder smoke test ($width x $height, $frames frames)...")

        // 1. Initialize
        val initOk = decoder.initialize(width, height)
        if (!initOk) {
            return mapOf(
                "success" to false,
                "error" to "Decoder initialization failed"
            )
        }

        // 2. Canonical SPS and PPS for 1280x720 Main Profile Level 3.2
        val testConfigPayload = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x67, 0x4D, 0x40, 0x20.toByte(),
            0x96.toByte(), 0x56, 0x02, 0xB0.toByte(), 0x30, 0xF3.toByte(),
            0x70, 0x80.toByte(), 0x00, 0x00, 0x03, 0x00, 0x80.toByte(),
            0x00, 0x00, 0x1E, 0x06, 0xD0.toByte(), 0x44, 0x22, 0x9C.toByte(),
            0x00, 0x00, 0x00, 0x01, 0x68, 0xCE.toByte(), 0x3C, 0x80.toByte()
        )

        val configOk = decoder.feedConfig(testConfigPayload)
        if (!configOk) {
            return mapOf(
                "success" to false,
                "error" to "Decoder rejected test SPS/PPS configuration"
            )
        }

        // 3. Test IDR Slice (Keyframe)
        val testIdrPayload = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x65, 0x88.toByte(), 0x84.toByte(), 0x00,
            0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x10
        )

        var submitted = 0
        var timestampUs = 0L

        // Feed IDR Keyframe first
        if (decoder.feedAccessUnit(testIdrPayload, timestampUs, isKeyframeHint = true)) {
            submitted++
        }

        // 4. Test Delta P-slices
        val testDeltaPayload = byteArrayOf(
            0x00, 0x00, 0x00, 0x01, 0x41, 0x9A.toByte(), 0x24.toByte(), 0x00,
            0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x20
        )

        for (i in 1 until frames) {
            timestampUs += 33333L // ~30 FPS interval
            if (decoder.feedAccessUnit(testDeltaPayload, timestampUs, isKeyframeHint = false)) {
                submitted++
            }
            try {
                Thread.sleep(16)
            } catch (_: InterruptedException) {}
        }

        // Wait a short duration for decoder to process
        try {
            Thread.sleep(200)
        } catch (_: InterruptedException) {}

        val metrics = decoder.getMetrics()
        Log.i(TAG, "Smoke test finished: submitted=$submitted, decoded=${metrics.decodedFrames}, errors=${metrics.decodeErrors}")

        return mapOf(
            "success" to (metrics.decodeErrors == 0L && metrics.state == H264Decoder.State.DECODING),
            "codecName" to metrics.codecName,
            "isHardwareAccelerated" to metrics.isHardwareAccelerated,
            "submittedFrames" to submitted,
            "decodedFrames" to metrics.decodedFrames,
            "decodeErrors" to metrics.decodeErrors,
            "state" to metrics.state.name
        )
    }
}
