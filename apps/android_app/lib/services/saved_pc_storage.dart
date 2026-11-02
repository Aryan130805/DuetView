import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/saved_pc.dart';

/// Manages persistence, deduplication, and lifecycle of saved host PCs.
class SavedPcStorage extends ChangeNotifier {
  static const String _storageKey = 'secondscreen_saved_pcs_v1';

  final List<SavedPc> _savedPcs = [];
  bool _isLoaded = false;

  bool get isLoaded => _isLoaded;
  List<SavedPc> get savedPcs => List.unmodifiable(_savedPcs);

  /// Loads saved PCs from SharedPreferences
  Future<void> load() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final rawList = prefs.getStringList(_storageKey);
      _savedPcs.clear();

      if (rawList != null) {
        for (final item in rawList) {
          try {
            final map = jsonDecode(item) as Map<String, dynamic>;
            final pc = SavedPc.fromMap(map);
            // Derived from current health checks rather than saved historical status
            _savedPcs.add(pc.copyWith(status: PcOnlineStatus.unknown));
          } catch (e) {
            debugPrint('SavedPcStorage: Failed to parse saved PC item: $e');
          }
        }
      }
    } catch (e) {
      debugPrint('SavedPcStorage: Error loading from SharedPreferences: $e');
    } finally {
      _isLoaded = true;
      notifyListeners();
    }
  }

  /// Adds or updates a saved PC. Prevents duplicates by stable PC ID.
  Future<void> savePc(SavedPc pc) async {
    final index = _savedPcs.indexWhere((p) => p.id == pc.id);

    if (index >= 0) {
      // Update existing PC: preserve custom user-edited name if unchanged, update endpoints and timestamp
      final existing = _savedPcs[index];

      // Merge endpoints preserving order
      final mergedEndpoints = List<String>.from(pc.endpoints);
      for (final ep in existing.endpoints) {
        if (!mergedEndpoints.contains(ep)) {
          mergedEndpoints.add(ep);
        }
      }

      _savedPcs[index] = existing.copyWith(
        name: pc.name.isNotEmpty ? pc.name : existing.name,
        endpoints: mergedEndpoints,
        port: pc.port,
        protocolVersion: pc.protocolVersion,
        lastSeen: pc.lastSeen,
        status: pc.status != PcOnlineStatus.unknown ? pc.status : existing.status,
        lastPingMs: pc.lastPingMs ?? existing.lastPingMs,
        lastChecked: pc.lastChecked ?? existing.lastChecked,
      );
    } else {
      _savedPcs.add(pc);
    }

    await _persist();
    notifyListeners();
  }

  /// Updates user-configurable display name for a saved PC
  Future<bool> updatePcName(String id, String newName) async {
    final index = _savedPcs.indexWhere((p) => p.id == id);
    if (index < 0) return false;

    _savedPcs[index] = _savedPcs[index].copyWith(name: newName.trim());
    await _persist();
    notifyListeners();
    return true;
  }

  /// Updates real-time online/offline reachability status
  Future<void> updatePcStatus(
    String id,
    PcOnlineStatus status, {
    int? pingMs,
    String? reachableEndpoint,
  }) async {
    final index = _savedPcs.indexWhere((p) => p.id == id);
    if (index < 0) return;

    final current = _savedPcs[index];
    List<String> endpoints = current.endpoints;

    // If a specific endpoint succeeded, move it to the front as primary
    if (reachableEndpoint != null && reachableEndpoint.isNotEmpty) {
      endpoints = List<String>.from(current.endpoints);
      endpoints.remove(reachableEndpoint);
      endpoints.insert(0, reachableEndpoint);
    }

    final endpointsChanged = reachableEndpoint != null &&
        reachableEndpoint.isNotEmpty &&
        (current.endpoints.isEmpty || current.endpoints.first != reachableEndpoint);

    _savedPcs[index] = current.copyWith(
      status: status,
      lastPingMs: pingMs ?? current.lastPingMs,
      lastChecked: DateTime.now(),
      endpoints: endpoints,
      lastSeen: (status == PcOnlineStatus.online || status == PcOnlineStatus.streaming)
          ? DateTime.now()
          : current.lastSeen,
    );

    // Persist when network endpoints change or active connection occurs
    if (endpointsChanged || status == PcOnlineStatus.streaming) {
      await _persist();
    }

    notifyListeners();
  }

  /// Removes a saved PC by identifier
  Future<bool> removePc(String id) async {
    final initialLen = _savedPcs.length;
    _savedPcs.removeWhere((p) => p.id == id);
    final removed = _savedPcs.length < initialLen;
    if (removed) {
      await _persist();
      notifyListeners();
    }
    return removed;
  }

  /// Clears all saved PCs
  Future<void> clearAll() async {
    _savedPcs.clear();
    await _persist();
    notifyListeners();
  }

  Future<void> _persist() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final stringList = _savedPcs.map((p) => jsonEncode(p.toMap())).toList();
      await prefs.setStringList(_storageKey, stringList);
    } catch (e) {
      debugPrint('SavedPcStorage: Failed to persist to SharedPreferences: $e');
    }
  }
}

