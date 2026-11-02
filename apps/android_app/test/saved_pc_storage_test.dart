import 'package:flutter_test/flutter_test.dart';
import 'package:shared_preferences/shared_preferences.dart';
import 'package:second_screen_android/models/saved_pc.dart';
import 'package:second_screen_android/services/saved_pc_storage.dart';
import 'package:second_screen_android/services/pc_health_service.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  setUp(() {
    SharedPreferences.setMockInitialValues({});
  });

  group('SavedPcStorage Persistence & Deduplication Tests', () {
    test('Saves and loads PCs from storage with full metadata', () async {
      final storage = SavedPcStorage();
      await storage.load();
      expect(storage.savedPcs, isEmpty);

      final now = DateTime(2026, 10, 30, 22, 0);
      final pc1 = SavedPc(
        id: 'pc-1001',
        name: 'Office PC',
        endpoints: ['192.168.1.50', '10.0.0.5'],
        port: 52025,
        protocolVersion: 2,
        lastSeen: now,
        status: PcOnlineStatus.online,
        lastPingMs: 8,
      );

      await storage.savePc(pc1);
      expect(storage.savedPcs.length, equals(1));
      expect(storage.savedPcs.first.name, equals('Office PC'));
      expect(storage.savedPcs.first.endpoints.length, equals(2));
      expect(storage.savedPcs.first.port, equals(52025));
      expect(storage.savedPcs.first.protocolVersion, equals(2));

      // Create new storage instance to verify persistence across restarts
      final storage2 = SavedPcStorage();
      await storage2.load();
      expect(storage2.savedPcs.length, equals(1));
      expect(storage2.savedPcs.first.id, equals('pc-1001'));
      expect(storage2.savedPcs.first.name, equals('Office PC'));
      expect(storage2.savedPcs.first.primaryAddress, equals('192.168.1.50'));
      expect(
        storage2.savedPcs.first.endpoints,
        containsAll(['192.168.1.50', '10.0.0.5']),
      );
    });

    test('Prevents duplicate entries when same PC ID is scanned again with updated IP', () async {
      final storage = SavedPcStorage();
      await storage.load();

      final pcInitial = SavedPc(
        id: 'pc-unique-1',
        name: 'Studio Workstation',
        endpoints: ['192.168.1.100'],
        port: 52025,
        lastSeen: DateTime(2026, 1, 1),
      );
      await storage.savePc(pcInitial);
      expect(storage.savedPcs.length, equals(1));

      // Re-scan same PC after IP changed via DHCP to 192.168.1.155
      final pcUpdated = SavedPc(
        id: 'pc-unique-1',
        name: 'Studio Workstation',
        endpoints: ['192.168.1.155'],
        port: 52025,
        lastSeen: DateTime(2026, 10, 30),
      );
      await storage.savePc(pcUpdated);

      // Must NOT create a second PC entry
      expect(storage.savedPcs.length, equals(1));
      final saved = storage.savedPcs.first;
      expect(saved.id, equals('pc-unique-1'));
      // New endpoint is prioritized and known endpoints are merged
      expect(saved.endpoints, containsAll(['192.168.1.155', '192.168.1.100']));
      expect(saved.lastSeen, equals(DateTime(2026, 10, 30)));
    });

    test('Renaming PC preserves stable ID and updates display name', () async {
      final storage = SavedPcStorage();
      await storage.load();

      final pc = SavedPc(
        id: 'pc-edit-name',
        name: 'Desktop-Default',
        endpoints: ['10.0.0.2'],
        lastSeen: DateTime.now(),
      );
      await storage.savePc(pc);

      final success = await storage.updatePcName(
        'pc-edit-name',
        'My Gaming Rig',
      );
      expect(success, isTrue);
      expect(storage.savedPcs.first.name, equals('My Gaming Rig'));
      expect(storage.savedPcs.first.id, equals('pc-edit-name'));
    });

    test(
      'Offline PC remains in saved list and updates status correctly',
      () async {
        final storage = SavedPcStorage();
        await storage.load();

        final pc = SavedPc(
          id: 'pc-status-check',
          name: 'Laptop',
          endpoints: ['192.168.1.80'],
          status: PcOnlineStatus.unknown,
          lastSeen: DateTime.now(),
        );
        await storage.savePc(pc);

        // Transition to online with ping
        await storage.updatePcStatus(
          'pc-status-check',
          PcOnlineStatus.online,
          pingMs: 12,
        );
        expect(storage.savedPcs.first.status, equals(PcOnlineStatus.online));
        expect(storage.savedPcs.first.lastPingMs, equals(12));

        // Transition to offline (must remain in storage!)
        await storage.updatePcStatus('pc-status-check', PcOnlineStatus.offline);
        expect(storage.savedPcs.length, equals(1));
        expect(storage.savedPcs.first.status, equals(PcOnlineStatus.offline));
        expect(storage.savedPcs.first.id, equals('pc-status-check'));
      },
    );

    test('Endpoint update prioritizes reachable address', () async {
      final storage = SavedPcStorage();
      await storage.load();

      final pc = SavedPc(
        id: 'pc-multi-nic',
        name: 'Server PC',
        endpoints: ['192.168.1.10', '192.168.2.20'],
        status: PcOnlineStatus.unknown,
        lastSeen: DateTime.now(),
      );
      await storage.savePc(pc);
      expect(storage.savedPcs.first.primaryAddress, equals('192.168.1.10'));

      // 192.168.2.20 responds
      await storage.updatePcStatus(
        'pc-multi-nic',
        PcOnlineStatus.online,
        pingMs: 5,
        reachableEndpoint: '192.168.2.20',
      );

      expect(storage.savedPcs.first.primaryAddress, equals('192.168.2.20'));
      expect(
        storage.savedPcs.first.endpoints,
        containsAll(['192.168.2.20', '192.168.1.10']),
      );
    });

    test('Removes saved PC correctly', () async {
      final storage = SavedPcStorage();
      await storage.load();

      final pc1 = SavedPc(
        id: 'pc-a',
        name: 'PC A',
        endpoints: ['1.1.1.1'],
        lastSeen: DateTime.now(),
      );
      final pc2 = SavedPc(
        id: 'pc-b',
        name: 'PC B',
        endpoints: ['2.2.2.2'],
        lastSeen: DateTime.now(),
      );
      await storage.savePc(pc1);
      await storage.savePc(pc2);
      expect(storage.savedPcs.length, equals(2));

      final removed = await storage.removePc('pc-a');
      expect(removed, isTrue);
      expect(storage.savedPcs.length, equals(1));
      expect(storage.savedPcs.first.id, equals('pc-b'));
    });

    test('Maintains active streaming exclusion in PcHealthService', () async {
      final storage = SavedPcStorage();
      await storage.load();

      final pc = SavedPc(
        id: 'pc-streaming-test',
        name: 'Live Stream PC',
        endpoints: ['192.168.1.99'],
        lastSeen: DateTime.now(),
      );
      await storage.savePc(pc);

      final healthService = PcHealthService(
        storage: storage,
        checkInterval: const Duration(milliseconds: 100),
      );

      // Mark as streaming
      healthService.setActivelyStreamingPc('pc-streaming-test');
      expect(storage.savedPcs.first.status, equals(PcOnlineStatus.streaming));

      // Reset streaming
      healthService.setActivelyStreamingPc(null);
      healthService.stop();
    });

    test('Initializes loaded PCs with status unknown so status is derived from live health checks', () async {
      // Simulate historical stored state where PC was saved as online
      final historicalJson = '{"id":"pc-hist","name":"Historical PC","endpoints":["192.168.1.77"],"port":52025,"status":"online","lastSeen":"2026-10-30T10:00:00.000"}';
      SharedPreferences.setMockInitialValues({
        'secondscreen_saved_pcs_v1': [historicalJson],
      });

      final storage = SavedPcStorage();
      await storage.load();

      expect(storage.savedPcs.length, equals(1));
      final loadedPc = storage.savedPcs.first;
      expect(loadedPc.id, equals('pc-hist'));
      // Must NOT be falsely online on startup before live health check verifies it
      expect(loadedPc.status, equals(PcOnlineStatus.unknown));
    });

    test('Persists updated IP endpoint across storage reload', () async {
      final storage1 = SavedPcStorage();
      await storage1.load();

      final pc = SavedPc(
        id: 'pc-dhcp',
        name: 'DHCP PC',
        endpoints: ['192.168.1.10', '192.168.1.20'],
        lastSeen: DateTime.now(),
      );
      await storage1.savePc(pc);
      expect(storage1.savedPcs.first.primaryAddress, equals('192.168.1.10'));

      // IP 192.168.1.20 responds and is moved to front
      await storage1.updatePcStatus(
        'pc-dhcp',
        PcOnlineStatus.streaming,
        reachableEndpoint: '192.168.1.20',
      );
      expect(storage1.savedPcs.first.primaryAddress, equals('192.168.1.20'));

      // Reload into fresh storage instance to verify durability
      final storage2 = SavedPcStorage();
      await storage2.load();
      expect(storage2.savedPcs.length, equals(1));
      expect(storage2.savedPcs.first.primaryAddress, equals('192.168.1.20'));
    });

    test('Deserializes legacy JSON schema with address string', () async {
      final legacyJson = '{"id":"pc-legacy","name":"Legacy Windows PC","address":"192.168.1.123","port":52025,"lastSeen":"2026-10-25T12:00:00.000"}';
      SharedPreferences.setMockInitialValues({
        'secondscreen_saved_pcs_v1': [legacyJson],
      });

      final storage = SavedPcStorage();
      await storage.load();

      expect(storage.savedPcs.length, equals(1));
      final pc = storage.savedPcs.first;
      expect(pc.id, equals('pc-legacy'));
      expect(pc.name, equals('Legacy Windows PC'));
      expect(pc.primaryAddress, equals('192.168.1.123'));
      expect(pc.endpoints, equals(['192.168.1.123']));
      expect(pc.status, equals(PcOnlineStatus.unknown));
    });
  });
}
