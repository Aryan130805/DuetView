import 'dart:async';
import 'dart:convert';
import 'dart:io';

import '../models/saved_pc.dart';
import 'frame_accumulator.dart';

/// Parsed data extracted from a SecondScreen pairing QR code.
class QrPairingPayload {
  final int version;
  final String id;
  final String name;
  final List<String> endpoints;
  final int port;
  final int pairPort;
  final String token;
  final int expirySec;

  const QrPairingPayload({
    required this.version,
    required this.id,
    required this.name,
    required this.endpoints,
    required this.port,
    int? pairPort,
    required this.token,
    required this.expirySec,
  }) : pairPort = pairPort ?? port;

  bool get isExpired {
    final nowSec = DateTime.now().millisecondsSinceEpoch ~/ 1000;
    return nowSec > expirySec;
  }
}

/// Result of QR payload syntax and schema validation.
class QrValidationResult {
  final bool isValid;
  final String? errorMessage;
  final QrPairingPayload? payload;

  const QrValidationResult({
    required this.isValid,
    this.errorMessage,
    this.payload,
  });

  factory QrValidationResult.success(QrPairingPayload payload) {
    return QrValidationResult(isValid: true, payload: payload);
  }

  factory QrValidationResult.failure(String message) {
    return QrValidationResult(isValid: false, errorMessage: message);
  }
}

/// Result of the network pairing handshake with the Windows host PC.
class PairingHandshakeResult {
  final bool isSuccess;
  final String? errorMessage;
  final String? reachableEndpoint;
  final SavedPc? savedPc;

  const PairingHandshakeResult({
    required this.isSuccess,
    this.errorMessage,
    this.reachableEndpoint,
    this.savedPc,
  });

  factory PairingHandshakeResult.success({
    required SavedPc savedPc,
    required String reachableEndpoint,
  }) {
    return PairingHandshakeResult(
      isSuccess: true,
      savedPc: savedPc,
      reachableEndpoint: reachableEndpoint,
    );
  }

  factory PairingHandshakeResult.failure(String message) {
    return PairingHandshakeResult(
      isSuccess: false,
      errorMessage: message,
    );
  }
}

typedef SocketConnector = Future<Socket> Function(
  dynamic host,
  int port, {
  Duration? timeout,
});

/// Service responsible for validating pairing QR codes and performing
/// the authenticated pairing handshake over TCP.
class QrPairingService {
  /// Validates and parses raw QR code content
  static QrValidationResult validate(String rawQr) {
    final trimmed = rawQr.trim();
    if (trimmed.isEmpty) {
      return QrValidationResult.failure('QR code content is empty.');
    }

    try {
      final map = jsonDecode(trimmed);
      if (map is! Map<String, dynamic>) {
        return QrValidationResult.failure('QR code is not a valid JSON object.');
      }

      // 1. Version check
      final version = (map['v'] as num?)?.toInt() ?? 0;
      if (version < 1) {
        return QrValidationResult.failure(
          'Unsupported QR schema version ($version). Please update DuetView.',
        );
      }

      // 2. PC ID check
      final id = map['id'] as String? ?? '';
      if (id.isEmpty) {
        return QrValidationResult.failure('Invalid QR payload: Missing PC Identifier.');
      }

      // 3. Name check
      final name = map['name'] as String? ?? 'Windows PC';

      // 4. Port check
      final port = (map['port'] as num?)?.toInt() ?? 0;
      if (port <= 0 || port > 65535) {
        return QrValidationResult.failure('Invalid QR payload: Illegal TCP port ($port).');
      }
      final pairPort = (map['pair_port'] as num?)?.toInt() ?? port;

      // 5. Token check
      final token = map['token'] as String? ?? '';
      if (token.isEmpty) {
        return QrValidationResult.failure('Invalid QR payload: Missing pairing token.');
      }

      // 6. Endpoints check
      List<String> endpoints = [];
      if (map['endpoints'] is List) {
        endpoints = (map['endpoints'] as List).map((e) => e.toString().trim()).toList();
      }
      endpoints.removeWhere((e) => e.isEmpty);
      if (endpoints.isEmpty) {
        return QrValidationResult.failure('Invalid QR payload: No IP endpoints provided.');
      }

      // 7. Expiry check
      final expirySec = (map['exp'] as num?)?.toInt() ?? 0;
      final payload = QrPairingPayload(
        version: version,
        id: id,
        name: name,
        endpoints: endpoints,
        port: port,
        pairPort: pairPort,
        token: token,
        expirySec: expirySec,
      );

      if (payload.isExpired) {
        return QrValidationResult.failure(
          'This pairing QR code has expired (tokens are valid for 5 minutes). Please press "r" or tap "Regenerate QR" on your Windows PC and scan again.',
        );
      }

      return QrValidationResult.success(payload);
    } on FormatException {
      return QrValidationResult.failure(
        'Malformed QR code. Make sure to scan the DuetView Windows pairing QR code.',
      );
    } catch (e) {
      return QrValidationResult.failure('Failed to parse QR code: $e');
    }
  }

  /// Connects to candidate endpoints and performs the SS02 pairing handshake
  static Future<PairingHandshakeResult> pair(
    QrPairingPayload payload, {
    Duration timeoutPerEndpoint = const Duration(milliseconds: 2500),
    SocketConnector? socketConnector,
  }) async {
    if (payload.isExpired) {
      return PairingHandshakeResult.failure(
        'Pairing token has expired (tokens are valid for 5 minutes). Please generate a new QR code on your PC.',
      );
    }

    final connector = socketConnector ?? Socket.connect;
    String? lastError;

    for (final endpoint in payload.endpoints) {
      try {
        final result = await _attemptPairOnEndpoint(
          endpoint,
          payload,
          timeoutPerEndpoint,
          connector,
        );
        if (result.isSuccess) {
          return result;
        } else {
          lastError = result.errorMessage;
          if (result.errorMessage?.startsWith('Pairing rejected:') == true) {
            return result;
          }
        }
      } catch (e) {
        lastError = 'Could not reach $endpoint:${payload.port} ($e)';
      }
    }

    return PairingHandshakeResult.failure(
      lastError ??
          'Could not reach the Windows PC on any of its addresses: ${payload.endpoints.join(', ')}.\n'
          'Ensure both devices are on the same Wi-Fi and Windows Firewall permits ports ${payload.port} and ${payload.pairPort}.',
    );
  }

  static Future<PairingHandshakeResult> _attemptPairOnEndpoint(
    String endpoint,
    QrPairingPayload payload,
    Duration timeout,
    SocketConnector connector,
  ) async {
    // Try dedicated pairPort first, then fallback to stream port if different
    final candidatePorts = <int>[payload.pairPort];
    if (payload.port != payload.pairPort && !candidatePorts.contains(payload.port)) {
      candidatePorts.add(payload.port);
    }

    String? lastPortError;
    for (final port in candidatePorts) {
      final res = await _attemptPairOnEndpointAndPort(
        endpoint,
        port,
        payload,
        timeout,
        connector,
      );
      if (res.isSuccess) {
        return res;
      }
      lastPortError = res.errorMessage;
      if (res.errorMessage?.startsWith('Pairing rejected:') == true) {
        return res;
      }
    }

    return PairingHandshakeResult.failure(
      lastPortError ?? 'Could not connect to $endpoint on pairing ports ${candidatePorts.join('/')}.',
    );
  }

  static Future<PairingHandshakeResult> _attemptPairOnEndpointAndPort(
    String endpoint,
    int port,
    QrPairingPayload payload,
    Duration timeout,
    SocketConnector connector,
  ) async {
    Socket? socket;
    try {
      socket = await connector(endpoint, port, timeout: timeout);

      final completer = Completer<PairingHandshakeResult>();

      final accumulator = FrameAccumulator(
        onSs02Packet: (packet) {
          if (packet.type == Ss02PacketType.pairResponse) {
            try {
              final jsonStr = utf8.decode(packet.payload);
              final map = jsonDecode(jsonStr) as Map<String, dynamic>;
              final statusCode = (map['status'] as num?)?.toInt() ?? 1;

              if (statusCode == 0) {
                // Success
                final verifiedPc = SavedPc(
                  id: payload.id,
                  name: payload.name,
                  endpoints: [endpoint, ...payload.endpoints.where((e) => e != endpoint)],
                  port: payload.port,
                  protocolVersion: 2,
                  lastSeen: DateTime.now(),
                  status: PcOnlineStatus.online,
                );
                completer.complete(
                  PairingHandshakeResult.success(
                    savedPc: verifiedPc,
                    reachableEndpoint: endpoint,
                  ),
                );
              } else if (statusCode == 2) {
                completer.complete(
                  PairingHandshakeResult.failure(
                    'Pairing rejected: The pairing token has expired on the host (tokens are valid for 5 minutes). Please press "r" or tap "Regenerate QR" on Windows.',
                  ),
                );
              } else if (statusCode == 1) {
                completer.complete(
                  PairingHandshakeResult.failure(
                    'Pairing rejected: Host refused authentication token. The token may be invalid or already consumed.',
                  ),
                );
              } else {
                completer.complete(
                  PairingHandshakeResult.failure(
                    'Pairing rejected: Host returned error code $statusCode.',
                  ),
                );
              }
            } catch (e) {
              completer.complete(
                PairingHandshakeResult.failure('Failed to parse host response: $e'),
              );
            }
          }
        },
        onFrame: (frame, timestamp) {},
        onError: (err) {
          if (!completer.isCompleted) {
            completer.complete(PairingHandshakeResult.failure(err));
          }
        },
      );

      socket.listen(
        (data) => accumulator.addChunk(data),
        onError: (e) {
          if (!completer.isCompleted) {
            completer.complete(PairingHandshakeResult.failure('Socket error: $e'));
          }
        },
        cancelOnError: true,
      );

      // Send pairing request packet
      final pairReqPacket = createPairRequestPacket(
        sequenceNumber: 1,
        timestampUs: DateTime.now().microsecondsSinceEpoch,
        token: payload.token,
        clientName: 'Android DuetView',
      );

      socket.add(pairReqPacket);
      await socket.flush();

      final result = await completer.future.timeout(
        timeout,
        onTimeout: () => PairingHandshakeResult.failure('Pairing handshake timed out on $endpoint:$port.'),
      );

      return result;
    } catch (e) {
      return PairingHandshakeResult.failure('Could not connect to $endpoint:$port ($e)');
    } finally {
      try {
        socket?.destroy();
      } catch (_) {}
    }
  }
}

