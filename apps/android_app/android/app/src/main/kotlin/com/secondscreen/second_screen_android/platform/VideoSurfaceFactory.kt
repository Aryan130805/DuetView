package com.secondscreen.second_screen_android.platform

import android.content.Context
import com.secondscreen.second_screen_android.decoder.H264Decoder
import io.flutter.plugin.common.StandardMessageCodec
import io.flutter.plugin.platform.PlatformView
import io.flutter.plugin.platform.PlatformViewFactory

/**
 * Factory for creating native VideoSurfacePlatformView instances inside Flutter.
 */
class VideoSurfaceFactory(
    private val decoder: H264Decoder
) : PlatformViewFactory(StandardMessageCodec.INSTANCE) {

    override fun create(context: Context, viewId: Int, args: Any?): PlatformView {
        return VideoSurfacePlatformView(context, viewId, decoder)
    }
}
