enum StreamResolution {
  res720p(1280, 720, '1280 × 720 (HD)'),
  res1080p(1920, 1080, '1920 × 1080 (FHD)');

  final int width;
  final int height;
  final String label;
  const StreamResolution(this.width, this.height, this.label);

  String get key => '${width}x$height';

  static StreamResolution fromKey(String? key) {
    switch (key) {
      case '1280x720':
        return StreamResolution.res720p;
      case '1920x1080':
      default:
        return StreamResolution.res1080p;
    }
  }
}

enum StreamFrameRate {
  fps30(30, '30 FPS (Power Saver)'),
  fps60(60, '60 FPS (Smooth)'),
  fps90(90, '90 FPS (Smooth 90Hz)'),
  fps120(120, '120 FPS (Ultra 120Hz)');

  final int fps;
  final String label;
  const StreamFrameRate(this.fps, this.label);

  static StreamFrameRate fromFps(int? fps) {
    if (fps == 30) return StreamFrameRate.fps30;
    if (fps == 90) return StreamFrameRate.fps90;
    if (fps == 120) return StreamFrameRate.fps120;
    return StreamFrameRate.fps60;
  }
}

enum ConnectionTransport {
  wifi('Wi-Fi (Local Network)'),
  usb('USB Cable (Tethering / Wired)');

  final String label;
  const ConnectionTransport(this.label);

  static ConnectionTransport fromString(String? val) {
    if (val == 'usb') return ConnectionTransport.usb;
    return ConnectionTransport.wifi;
  }
}

class StreamSettings {
  final StreamResolution resolution;
  final StreamFrameRate frameRate;
  final ConnectionTransport transport;
  final bool autoReconnect;
  final bool fullScreen;
  final int defaultPort;
  final String lastHostAddress;

  const StreamSettings({
    this.resolution = StreamResolution.res1080p,
    this.frameRate = StreamFrameRate.fps60,
    this.transport = ConnectionTransport.wifi,
    this.autoReconnect = true,
    this.fullScreen = true,
    this.defaultPort = 52025,
    this.lastHostAddress = '',
  });

  Map<String, dynamic> toJson() => {
    'resolution': resolution.key,
    'frameRate': frameRate.fps,
    'transport': transport.name,
    'autoReconnect': autoReconnect,
    'fullScreen': fullScreen,
    'defaultPort': defaultPort,
    'lastHostAddress': lastHostAddress,
  };

  factory StreamSettings.fromJson(Map<String, dynamic> json) => StreamSettings(
    resolution: StreamResolution.fromKey(json['resolution'] as String?),
    frameRate: StreamFrameRate.fromFps(json['frameRate'] as int?),
    transport: ConnectionTransport.fromString(json['transport'] as String?),
    autoReconnect: json['autoReconnect'] as bool? ?? true,
    fullScreen: json['fullScreen'] as bool? ?? true,
    defaultPort: (json['defaultPort'] as num?)?.toInt() ?? 52025,
    lastHostAddress: json['lastHostAddress'] as String? ?? '',
  );

  StreamSettings copyWith({
    StreamResolution? resolution,
    StreamFrameRate? frameRate,
    ConnectionTransport? transport,
    bool? autoReconnect,
    bool? fullScreen,
    int? defaultPort,
    String? lastHostAddress,
  }) {
    return StreamSettings(
      resolution: resolution ?? this.resolution,
      frameRate: frameRate ?? this.frameRate,
      transport: transport ?? this.transport,
      autoReconnect: autoReconnect ?? this.autoReconnect,
      fullScreen: fullScreen ?? this.fullScreen,
      defaultPort: defaultPort ?? this.defaultPort,
      lastHostAddress: lastHostAddress ?? this.lastHostAddress,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is StreamSettings &&
        other.resolution == resolution &&
        other.frameRate == frameRate &&
        other.transport == transport &&
        other.autoReconnect == autoReconnect &&
        other.fullScreen == fullScreen &&
        other.defaultPort == defaultPort &&
        other.lastHostAddress == lastHostAddress;
  }

  @override
  int get hashCode => Object.hash(
    resolution,
    frameRate,
    transport,
    autoReconnect,
    fullScreen,
    defaultPort,
    lastHostAddress,
  );
}
