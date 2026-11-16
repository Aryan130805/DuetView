# ===========================================================================
# DuetView Android ProGuard / R8 Optimization Rules
# ===========================================================================

# 1. Flutter Engine & Framework
-keep class io.flutter.** { *; }
-dontwarn io.flutter.**
-keep class io.flutter.plugin.** { *; }
-keep class io.flutter.plugins.** { *; }
-keep class io.flutter.util.** { *; }
-keep class io.flutter.view.** { *; }
-keep class io.flutter.embedding.** { *; }

# 2. DuetView Native Engine & MediaCodec Bridge
-keep class com.secondscreen.second_screen_android.** { *; }
-keepclassmembers class com.secondscreen.second_screen_android.** { *; }

# Preserve Android Hardware Video Decoders & Surface Holders
-keep class android.media.MediaCodec { *; }
-keep class android.media.MediaFormat { *; }
-keep class android.media.MediaCodecInfo { *; }
-keep class android.view.Surface { *; }
-keep class android.view.SurfaceView { *; }
-keep class android.view.SurfaceHolder { *; }
-keep class android.view.SurfaceHolder$Callback { *; }

# 3. Third-party Plugins
# SharedPreferences
-keep class io.flutter.plugins.sharedpreferences.** { *; }

# Mobile Scanner (CameraX / MLKit QR Scanning)
-keep class dev.steenbakker.mobile_scanner.** { *; }
-dontwarn dev.steenbakker.mobile_scanner.**
-keep class androidx.camera.** { *; }
-dontwarn androidx.camera.**
-keep class com.google.mlkit.** { *; }
-dontwarn com.google.mlkit.**

# 4. Debugging & Google Play Crash Reporting
# Retain line numbers and source file names for readable Play Console stack traces
-keepattributes SourceFile,LineNumberTable
-keepattributes *Annotation*
-keepattributes Signature
-keepattributes InnerClasses,EnclosingMethod

# Suppress harmless transitive warnings
-dontwarn okio.**
-dontwarn javax.annotation.**
-dontwarn org.codehaus.mojo.animal_sniffer.**
