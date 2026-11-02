package com.secondscreen.second_screen_android.platform

import android.content.Context
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.View
import com.secondscreen.second_screen_android.decoder.H264Decoder
import io.flutter.plugin.platform.PlatformView

/**
 * Native Android PlatformView providing a SurfaceView for direct hardware-accelerated
 * H.264 video rendering from MediaCodec into Flutter.
 */
class VideoSurfacePlatformView(
    context: Context,
    private val viewId: Int,
    private val decoder: H264Decoder
) : PlatformView, SurfaceHolder.Callback {

    companion object {
        private const val TAG = "SecondScreenVideoSurface"
        const val VIEW_TYPE = "com.secondscreen/video_surface"
    }

    private val surfaceView: SurfaceView = SurfaceView(context).apply {
        holder.addCallback(this@VideoSurfacePlatformView)
    }

    override fun getView(): View = surfaceView

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "Native Surface created for viewId=$viewId. Attaching to H264Decoder.")
        decoder.setSurface(holder.surface)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "Native Surface changed: width=$width, height=$height, format=$format")
        decoder.setSurface(holder.surface)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "Native Surface destroyed for viewId=$viewId. Detaching from H264Decoder.")
        decoder.setSurface(null)
    }

    override fun dispose() {
        Log.i(TAG, "Disposing VideoSurfacePlatformView viewId=$viewId")
        surfaceView.holder.removeCallback(this)
        decoder.setSurface(null)
    }
}
