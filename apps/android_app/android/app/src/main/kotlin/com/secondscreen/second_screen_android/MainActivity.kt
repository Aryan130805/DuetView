package com.secondscreen.second_screen_android

import com.secondscreen.second_screen_android.decoder.H264Decoder
import com.secondscreen.second_screen_android.platform.SecondScreenNativeBridge
import com.secondscreen.second_screen_android.platform.VideoSurfaceFactory
import com.secondscreen.second_screen_android.platform.VideoSurfacePlatformView
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine

class MainActivity : FlutterActivity() {

    private lateinit var decoder: H264Decoder
    private lateinit var nativeBridge: SecondScreenNativeBridge

    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)

        // 1. Instantiate native H.264 MediaCodec decoder
        decoder = H264Decoder()

        // 2. Setup MethodChannel bridge for decoder control
        nativeBridge = SecondScreenNativeBridge(flutterEngine.dartExecutor.binaryMessenger, decoder, this)

        // 3. Register native SurfaceView PlatformViewFactory
        flutterEngine
            .platformViewsController
            .registry
            .registerViewFactory(
                VideoSurfacePlatformView.VIEW_TYPE,
                VideoSurfaceFactory(decoder)
            )
    }

    override fun onDestroy() {
        decoder.release()
        super.onDestroy()
    }
}
