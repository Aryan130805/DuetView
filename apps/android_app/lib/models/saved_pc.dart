import 'dart:convert';

/// High-level online/offline status classification for saved host PCs.
enum PcOnlineStatus {
  online,
  offline,
  streaming,
  unknown;

  String get displayName {
    switch (this) {
      case PcOnlineStatus.online:
        return 'Online';
      case PcOnlineStatus.offline:
        return 'Offline';
      case PcOnlineStatus.streaming:
        return 'Streaming';
      case PcOnlineStatus.unknown:
        return 'Unknown';
    }
  }

  String get accessibilityLabel {
    switch (this) {
      case PcOnlineStatus.online:
        return 'PC is online and responsive';
      case PcOnlineStatus.offline:
        return 'PC is unreachable or offline';
      case PcOnlineStatus.streaming:
        return 'PC is actively streaming';
      case PcOnlineStatus.unknown:
        return 'Checking PC connection status';
    }
  }
}

/// Represents a persistent saved Windows host PC.
class SavedPc {
  final String id;
  final String name;
  final List<String> endpoints;
  final int port;
  final int protocolVersion;
  final DateTime lastSeen;
  final PcOnlineStatus status;
  final int? lastPingMs;
  final DateTime? lastChecked;
  final String osName;

  const SavedPc({
    required this.id,
    required this.name,
    required this.endpoints,
    this.port = 52025,
    this.protocolVersion = 2,
    required this.lastSeen,
    this.status = PcOnlineStatus.unknown,
    this.lastPingMs,
    this.lastChecked,
    this.osName = 'Windows PC',
  });

  /// Primary reachable address (first available endpoint)
  String get primaryAddress => endpoints.isNotEmpty ? endpoints.first : '';

  Map<String, dynamic> toMap() => {
    'id': id,
    'name': name,
    'endpoints': endpoints,
    'port': port,
    'protocolVersion': protocolVersion,
    'lastSeen': lastSeen.toIso8601String(),
    'status': status.name,
    'lastPingMs': lastPingMs,
    'lastChecked': lastChecked?.toIso8601String(),
    'osName': osName,
  };

  factory SavedPc.fromMap(Map<String, dynamic> map) {
    List<String> parsedEndpoints = [];
    if (map['endpoints'] is List) {
      parsedEndpoints = (map['endpoints'] as List).map((e) => e.toString()).toList();
    } else if (map['address'] is String) {
      parsedEndpoints = [map['address'] as String];
    }

    return SavedPc(
      id: map['id'] as String? ?? (parsedEndpoints.isNotEmpty ? parsedEndpoints.first : 'unknown'),
      name: map['name'] as String? ?? 'Windows PC',
      endpoints: parsedEndpoints,
      port: (map['port'] as num?)?.toInt() ?? 52025,
      protocolVersion: (map['protocolVersion'] as num?)?.toInt() ?? 2,
      lastSeen: map['lastSeen'] != null
          ? DateTime.tryParse(map['lastSeen'] as String) ?? DateTime.now()
          : DateTime.now(),
      status: PcOnlineStatus.values.firstWhere(
        (e) => e.name == map['status'],
        orElse: () => PcOnlineStatus.unknown,
      ),
      lastPingMs: (map['lastPingMs'] as num?)?.toInt(),
      lastChecked: map['lastChecked'] != null
          ? DateTime.tryParse(map['lastChecked'] as String)
          : null,
      osName: map['osName'] as String? ?? 'Windows PC',
    );
  }

  String toJson() => jsonEncode(toMap());
  factory SavedPc.fromJson(String source) => SavedPc.fromMap(jsonDecode(source) as Map<String, dynamic>);

  SavedPc copyWith({
    String? id,
    String? name,
    List<String>? endpoints,
    int? port,
    int? protocolVersion,
    DateTime? lastSeen,
    PcOnlineStatus? status,
    int? lastPingMs,
    DateTime? lastChecked,
    String? osName,
  }) {
    return SavedPc(
      id: id ?? this.id,
      name: name ?? this.name,
      endpoints: endpoints ?? this.endpoints,
      port: port ?? this.port,
      protocolVersion: protocolVersion ?? this.protocolVersion,
      lastSeen: lastSeen ?? this.lastSeen,
      status: status ?? this.status,
      lastPingMs: lastPingMs ?? this.lastPingMs,
      lastChecked: lastChecked ?? this.lastChecked,
      osName: osName ?? this.osName,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is SavedPc && other.id == id;
  }

  @override
  int get hashCode => id.hashCode;
}

