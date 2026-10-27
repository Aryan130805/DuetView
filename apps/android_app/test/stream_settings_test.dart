import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/models/stream_settings.dart';

void main() {
  group('StreamSettings Tests', () {
    test('default settings have sensible values', () {
      const settings = StreamSettings();
      expect(settings.resolution, StreamResolution.res1080p);
      expect(settings.frameRate, StreamFrameRate.fps60);
      expect(settings.transport, ConnectionTransport.wifi);
      expect(settings.autoReconnect, isTrue);
      expect(settings.fullScreen, isTrue);
      expect(settings.defaultPort, 52025);
      expect(settings.lastHostAddress, isEmpty);
    });

    test('toJson and fromJson serialize accurately', () {
      const original = StreamSettings(
        resolution: StreamResolution.res720p,
        frameRate: StreamFrameRate.fps30,
        transport: ConnectionTransport.wifi,
        autoReconnect: false,
        fullScreen: false,
        defaultPort: 54321,
        lastHostAddress: '192.168.1.150',
      );

      final json = original.toJson();
      final restored = StreamSettings.fromJson(json);

      expect(restored.resolution, StreamResolution.res720p);
      expect(restored.frameRate, StreamFrameRate.fps30);
      expect(restored.transport, ConnectionTransport.wifi);
      expect(restored.autoReconnect, isFalse);
      expect(restored.fullScreen, isFalse);
      expect(restored.defaultPort, 54321);
      expect(restored.lastHostAddress, '192.168.1.150');
      expect(restored, equals(original));
    });

    test('copyWith updates specified fields only', () {
      const original = StreamSettings();
      final updated = original.copyWith(
        resolution: StreamResolution.res720p,
        lastHostAddress: '10.0.0.5',
      );

      expect(updated.resolution, StreamResolution.res720p);
      expect(updated.lastHostAddress, '10.0.0.5');
      expect(updated.frameRate, original.frameRate);
      expect(updated.defaultPort, original.defaultPort);
    });
  });
}
