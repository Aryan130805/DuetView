/// Represents a detected or manually configured Windows host PC.
class PcDevice {
  final String id;
  final String name;
  final String address;
  final int port;
  final bool isReachable;
  final DateTime? lastSeen;
  final String osName;

  const PcDevice({
    required this.id,
    required this.name,
    required this.address,
    this.port = 52025,
    this.isReachable = true,
    this.lastSeen,
    this.osName = 'Windows PC',
  });

  Map<String, dynamic> toJson() => {
    'id': id,
    'name': name,
    'address': address,
    'port': port,
    'isReachable': isReachable,
    'lastSeen': lastSeen?.toIso8601String(),
    'osName': osName,
  };

  factory PcDevice.fromJson(Map<String, dynamic> json) => PcDevice(
    id: json['id'] as String? ?? json['address'] as String,
    name: json['name'] as String? ?? 'Windows PC',
    address: json['address'] as String,
    port: (json['port'] as num?)?.toInt() ?? 52025,
    isReachable: json['isReachable'] as bool? ?? true,
    lastSeen: json['lastSeen'] != null
        ? DateTime.tryParse(json['lastSeen'] as String)
        : null,
    osName: json['osName'] as String? ?? 'Windows PC',
  );

  PcDevice copyWith({
    String? id,
    String? name,
    String? address,
    int? port,
    bool? isReachable,
    DateTime? lastSeen,
    String? osName,
  }) {
    return PcDevice(
      id: id ?? this.id,
      name: name ?? this.name,
      address: address ?? this.address,
      port: port ?? this.port,
      isReachable: isReachable ?? this.isReachable,
      lastSeen: lastSeen ?? this.lastSeen,
      osName: osName ?? this.osName,
    );
  }

  @override
  bool operator ==(Object other) {
    if (identical(this, other)) return true;
    return other is PcDevice &&
        other.id == id &&
        other.address == address &&
        other.port == port;
  }

  @override
  int get hashCode => Object.hash(id, address, port);
}
