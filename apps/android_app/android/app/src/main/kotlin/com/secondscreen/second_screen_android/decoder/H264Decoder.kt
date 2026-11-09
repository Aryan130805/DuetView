package com.secondscreen.second_screen_android.decoder

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat
import android.os.Build
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import java.util.concurrent.atomic.AtomicReference
import kotlin.concurrent.thread

/**
 * Native Android H.264 (video/avc) hardware/software decoder for SecondScreen.
 *
 * Implements low-latency MediaCodec decoding directly to a native SurfaceView Surface.
 * Handles SS02 Annex B access-unit submission, presentation timestamps, keyframe tracking,
 * and dynamic surface lifecycle transitions without blocking the Flutter UI thread.
 */
class H264Decoder(
    private var listener: Listener? = null
) {
    companion object {
        private const val TAG = "SecondScreenH264Decoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC
        private const val DEQUEUE_TIMEOUT_US = 2_000L // 2ms low-latency timeout
    }

    interface Listener {
        fun onDecoderStateChanged(state: State)
        fun onFrameDecoded(width: Int, height: Int, timestampUs: Long)
        fun onKeyframeRequested()
        fun onDecoderError(error: String, isFatal: Boolean)
    }

    enum class State {
        UNINITIALIZED,
        INITIALIZING,
        WAITING_FOR_CONFIG,
        CONFIGURED,
        WAITING_FOR_KEYFRAME,
        DECODING,
        FLUSHING,
        ERROR,
        RELEASED
    }

    data class DecoderCapabilities(
        val codecName: String,
        val isHardwareAccelerated: Boolean,
        val maxWidth: Int,
        val maxHeight: Int,
        val isSupported: Boolean
    )

    data class Metrics(
        val codecName: String,
        val isHardwareAccelerated: Boolean,
        val state: State,
        val configuredWidth: Int,
        val configuredHeight: Int,
        val submittedFrames: Long,
        val decodedFrames: Long,
        val outputFps: Double,
        val decodeErrors: Long,
        val keyframeWaitCount: Long
    )

    // State management
    private val currentState = AtomicReference(State.UNINITIALIZED)
    private var codec: MediaCodec? = null
    private var activeSurface: Surface? = null
    private val isSurfaceReady = AtomicBoolean(false)

    // Stream dimensions and codec configuration
    private var streamWidth: Int = 1280
    private var streamHeight: Int = 720
    private var cachedSps: ByteArray? = null
    private var cachedPps: ByteArray? = null
    private var selectedCodecName: String = "Unknown"
    private var isHardware: Boolean = false

    // Worker thread for output buffer draining
    private val isRunning = AtomicBoolean(false)
    private var outputDrainThread: Thread? = null

    // Metrics counters
    private val submittedFrameCount = AtomicLong(0)
    private val decodedFrameCount = AtomicLong(0)
    private val errorCount = AtomicLong(0)
    private val keyframeWaitCount = AtomicLong(0)
    private var lastKeyframeRequestTimeMs: Long = 0L
    private var lastFpsCalculationTime = System.nanoTime()
    private var framesSinceLastFps = 0L
    private var currentOutputFps: Double = 0.0

    val state: State get() = currentState.get()

    /**
     * Sets or updates the decoder lifecycle listener.
     * Immediately dispatches current state to the newly registered listener.
     */
    fun setListener(newListener: Listener?) {
        this.listener = newListener
        newListener?.onDecoderStateChanged(currentState.get())
    }

    /**
     * Discovers a suitable H.264 decoder on the Android system, preferring hardware acceleration.
     */
    fun probeCapabilities(): DecoderCapabilities {
        val codecList = MediaCodecList(MediaCodecList.REGULAR_CODECS)
        var selectedInfo: MediaCodecInfo? = null
        var foundHardware = false

        for (info in codecList.codecInfos) {
            if (info.isEncoder) continue
            val types = info.supportedTypes
            for (type in types) {
                if (type.equals(MIME_TYPE, ignoreCase = true)) {
                    val isHw = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                        info.isHardwareAccelerated
                    } else {
                        // Heuristic for API < 29
                        !info.name.startsWith("OMX.google.", ignoreCase = true) &&
                        !info.name.startsWith("c2.android.", ignoreCase = true)
                    }

                    if (isHw) {
                        selectedInfo = info
                        foundHardware = true
                        break
                    } else if (selectedInfo == null) {
                        selectedInfo = info
                    }
                }
            }
            if (foundHardware) break
        }

        if (selectedInfo == null) {
            return DecoderCapabilities(
                codecName = "None",
                isHardwareAccelerated = false,
                maxWidth = 0,
                maxHeight = 0,
                isSupported = false
            )
        }

        val caps = selectedInfo.getCapabilitiesForType(MIME_TYPE)
        val videoCaps = caps.videoCapabilities
        val maxW = videoCaps?.supportedWidths?.upper ?: 1920
        val maxH = videoCaps?.supportedHeights?.upper ?: 1080

        return DecoderCapabilities(
            codecName = selectedInfo.name,
            isHardwareAccelerated = foundHardware,
            maxWidth = maxW,
            maxHeight = maxH,
            isSupported = true
        )
    }

    /**
     * Initializes the decoder in preparation for configuration and stream frames.
     */
    @Synchronized
    fun initialize(width: Int = 1280, height: Int = 720): Boolean {
        if (state == State.DECODING || state == State.WAITING_FOR_CONFIG) {
            Log.w(TAG, "Decoder already initialized or active. Resetting first.")
            reset()
        }

        transitionState(State.INITIALIZING)
        streamWidth = width
        streamHeight = height
        submittedFrameCount.set(0)
        decodedFrameCount.set(0)
        errorCount.set(0)
        keyframeWaitCount.set(0)
        lastFpsCalculationTime = System.nanoTime()
        framesSinceLastFps = 0

        val caps = probeCapabilities()
        if (!caps.isSupported) {
            val errMsg = "No compatible H.264 (video/avc) decoder found on this device!"
            Log.e(TAG, errMsg)
            transitionState(State.ERROR)
            listener?.onDecoderError(errMsg, isFatal = true)
            return false
        }

        selectedCodecName = caps.codecName
        isHardware = caps.isHardwareAccelerated
        Log.i(TAG, "Selected decoder: $selectedCodecName (Hardware: $isHardware, Max: ${caps.maxWidth}x${caps.maxHeight})")

        transitionState(State.WAITING_FOR_CONFIG)
        return true
    }

    /**
     * Attaches or updates the native Surface for direct video rendering.
     */
    @Synchronized
    fun setSurface(surface: Surface?) {
        Log.i(TAG, "setSurface called with: $surface")
        activeSurface = surface
        isSurfaceReady.set(surface != null && surface.isValid)

        if (surface != null && surface.isValid) {
            val c = codec
            if (c != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                try {
                    c.setOutputSurface(surface)
                    Log.i(TAG, "Dynamic setOutputSurface succeeded.")
                } catch (e: Exception) {
                    Log.w(TAG, "Dynamic setOutputSurface failed (${e.message}). Reconfiguring codec.")
                    reconfigureInternal()
                }
            } else if (cachedSps != null && cachedPps != null && codec == null) {
                // Surface arrived after config
                configureCodec(cachedSps!!, cachedPps!!)
            }

            // Request fresh keyframe when a valid surface is attached/reattached
            listener?.onKeyframeRequested()
        }
    }

    /**
     * Ingests SS02 PACKET_TYPE_CONFIG (0x0001) payload containing SPS/PPS parameter sets.
     */
    @Synchronized
    fun feedConfig(payload: ByteArray): Boolean {
        Log.i(TAG, "feedConfig: received ${payload.size} bytes")
        val config = H264ConfigParser.parseConfig(payload)
        if (config == null) {
            Log.e(TAG, "Failed to parse SPS/PPS from config payload!")
            listener?.onDecoderError("Malformed SS02 CONFIG packet: missing SPS or PPS", isFatal = false)
            return false
        }

        cachedSps = config.sps
        cachedPps = config.pps

        return configureCodec(config.sps, config.pps)
    }

    /**
     * Configures and starts the MediaCodec instance with SPS (csd-0) and PPS (csd-1).
     */
    @Synchronized
    private fun configureCodec(sps: ByteArray, pps: ByteArray): Boolean {
        try {
            stopDrainLoop()
            codec?.release()
            codec = null

            val format = MediaFormat.createVideoFormat(MIME_TYPE, streamWidth, streamHeight)
            format.setByteBuffer("csd-0", ByteBuffer.wrap(sps))
            format.setByteBuffer("csd-1", ByteBuffer.wrap(pps))

            // Low-latency hints
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
            }
            format.setInteger(MediaFormat.KEY_PRIORITY, 0) // Realtime priority

            val newCodec = MediaCodec.createByCodecName(selectedCodecName)
            val surf = activeSurface
            if (surf != null && surf.isValid) {
                newCodec.configure(format, surf, null, 0)
            } else {
                Log.w(TAG, "Surface not ready during configureCodec. Configuring with null surface.")
                newCodec.configure(format, null, null, 0)
            }

            newCodec.start()
            codec = newCodec

            transitionState(State.CONFIGURED)
            transitionState(State.WAITING_FOR_KEYFRAME)

            // Start background draining thread
            startDrainLoop()

            Log.i(TAG, "MediaCodec configured and started successfully.")
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to configure MediaCodec: ${e.message}", e)
            transitionState(State.ERROR)
            listener?.onDecoderError("MediaCodec configuration failed: ${e.message}", isFatal = true)
            return false
        }
    }

    /**
     * Submits an encoded H.264 access unit (KEYFRAME or DELTA) into the decoder.
     */
    fun feedAccessUnit(data: ByteArray, timestampUs: Long, isKeyframeHint: Boolean = false): Boolean {
        var currentStateVal = currentState.get()
        if (currentStateVal != State.DECODING && currentStateVal != State.WAITING_FOR_KEYFRAME) {
            if (currentStateVal == State.WAITING_FOR_CONFIG) {
                // Check if this access unit contains inline SPS/PPS (common for WMF IDR keyframes)
                val inlineConfig = H264ConfigParser.parseConfig(data)
                if (inlineConfig != null) {
                    Log.i(TAG, "Found inline SPS/PPS in access unit! Auto-configuring decoder...")
                    if (feedConfig(data)) {
                        currentStateVal = currentState.get()
                    } else {
                        return false
                    }
                } else {
                    Log.w(TAG, "Discarding frame: waiting for codec configuration.")
                    return false
                }
            } else if (currentStateVal == State.ERROR || currentStateVal == State.RELEASED) {
                return false
            }
        }

        val isIdr = isKeyframeHint || H264ConfigParser.containsIdrSlice(data)

        // Keyframe synchronization rule
        if (currentStateVal == State.WAITING_FOR_KEYFRAME) {
            if (!isIdr) {
                keyframeWaitCount.incrementAndGet()
                val now = System.currentTimeMillis()
                if (now - lastKeyframeRequestTimeMs >= 1000L) {
                    lastKeyframeRequestTimeMs = now
                    Log.i(TAG, "Requesting IDR Keyframe from upstream host (cooldown active)...")
                    listener?.onKeyframeRequested()
                }
                return false
            }
            transitionState(State.DECODING)
            Log.i(TAG, "Received IDR Keyframe! Transitioned to DECODING state.")
        }

        val c = codec ?: return false

        try {
            val inIndex = c.dequeueInputBuffer(DEQUEUE_TIMEOUT_US)
            if (inIndex >= 0) {
                val inputBuffer: ByteBuffer? = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                    c.getInputBuffer(inIndex)
                } else {
                    @Suppress("DEPRECATION")
                    c.inputBuffers[inIndex]
                }

                if (inputBuffer == null || inputBuffer.capacity() < data.size) {
                    Log.e(TAG, "Input buffer too small: capacity=${inputBuffer?.capacity()} needed=${data.size}")
                    errorCount.incrementAndGet()
                    return false
                }

                inputBuffer.clear()
                inputBuffer.put(data)

                val flags = if (isIdr) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
                c.queueInputBuffer(inIndex, 0, data.size, timestampUs, flags)
                submittedFrameCount.incrementAndGet()
                return true
            } else {
                // Input buffer busy
                return false
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error queuing input buffer: ${e.message}", e)
            errorCount.incrementAndGet()
            handleDecoderException(e)
            return false
        }
    }

    /**
     * Background thread loop to drain and render decoded output buffers to the surface.
     */
    private fun startDrainLoop() {
        if (isRunning.getAndSet(true)) return

        outputDrainThread = thread(name = "SecondScreen-DecoderDrain", isDaemon = true) {
            val bufferInfo = MediaCodec.BufferInfo()
            while (isRunning.get()) {
                val c = codec ?: break
                try {
                    val outIndex = c.dequeueOutputBuffer(bufferInfo, DEQUEUE_TIMEOUT_US)
                    when {
                        outIndex >= 0 -> {
                            // Valid decoded frame ready
                            val render = isSurfaceReady.get()
                            c.releaseOutputBuffer(outIndex, render)

                            val totalDecoded = decodedFrameCount.incrementAndGet()
                            framesSinceLastFps++

                            // Update FPS every 500ms
                            val now = System.nanoTime()
                            val elapsedSec = (now - lastFpsCalculationTime) / 1_000_000_000.0
                            if (elapsedSec >= 0.5) {
                                currentOutputFps = framesSinceLastFps / elapsedSec
                                framesSinceLastFps = 0
                                lastFpsCalculationTime = now
                            }

                            listener?.onFrameDecoded(streamWidth, streamHeight, bufferInfo.presentationTimeUs)
                        }
                        outIndex == MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                            val newFormat = c.outputFormat
                            val newW = newFormat.getInteger(MediaFormat.KEY_WIDTH)
                            val newH = newFormat.getInteger(MediaFormat.KEY_HEIGHT)
                            Log.i(TAG, "Decoder output format changed: ${newW}x${newH}")
                            streamWidth = newW
                            streamHeight = newH
                        }
                        outIndex == MediaCodec.INFO_TRY_AGAIN_LATER -> {
                            // Normal timeout when no frame is ready
                        }
                    }
                } catch (e: IllegalStateException) {
                    if (isRunning.get()) {
                        Log.e(TAG, "Decoder drain exception: ${e.message}")
                        errorCount.incrementAndGet()
                        handleDecoderException(e)
                    }
                    break
                } catch (e: Exception) {
                    if (isRunning.get()) {
                        Log.e(TAG, "Unexpected drain exception: ${e.message}")
                        errorCount.incrementAndGet()
                    }
                    break
                }
            }
        }
    }

    private fun stopDrainLoop() {
        isRunning.set(false)
        outputDrainThread?.interrupt()
        try {
            outputDrainThread?.join(200)
        } catch (_: InterruptedException) {}
        outputDrainThread = null
    }

    private fun handleDecoderException(e: Exception) {
        transitionState(State.ERROR)
        listener?.onDecoderError("Decoder error: ${e.message}", isFatal = false)
        // Request keyframe after recovery
        listener?.onKeyframeRequested()
    }

    private fun reconfigureInternal() {
        val sps = cachedSps
        val pps = cachedPps
        if (sps != null && pps != null) {
            configureCodec(sps, pps)
        }
    }

    @Synchronized
    fun flush() {
        if (state == State.DECODING) {
            transitionState(State.FLUSHING)
            try {
                codec?.flush()
            } catch (e: Exception) {
                Log.w(TAG, "Flush error: ${e.message}")
            }
            transitionState(State.WAITING_FOR_KEYFRAME)
        }
    }

    @Synchronized
    fun reset() {
        stopDrainLoop()
        try {
            codec?.stop()
            codec?.release()
        } catch (_: Exception) {}
        codec = null
        transitionState(State.UNINITIALIZED)
    }

    @Synchronized
    fun release() {
        reset()
        activeSurface = null
        isSurfaceReady.set(false)
        cachedSps = null
        cachedPps = null
        transitionState(State.RELEASED)
    }

    fun getMetrics(): Metrics {
        return Metrics(
            codecName = selectedCodecName,
            isHardwareAccelerated = isHardware,
            state = currentState.get(),
            configuredWidth = streamWidth,
            configuredHeight = streamHeight,
            submittedFrames = submittedFrameCount.get(),
            decodedFrames = decodedFrameCount.get(),
            outputFps = currentOutputFps,
            decodeErrors = errorCount.get(),
            keyframeWaitCount = keyframeWaitCount.get()
        )
    }

    private fun transitionState(newState: State) {
        val old = currentState.getAndSet(newState)
        if (old != newState) {
            Log.d(TAG, "State transition: $old -> $newState")
            listener?.onDecoderStateChanged(newState)
        }
    }
}
