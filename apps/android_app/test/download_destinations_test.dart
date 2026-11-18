import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/screens/guide_screen.dart';
import 'package:second_screen_android/services/url_launcher_service.dart';
import 'package:second_screen_android/widgets/download_pc_guide_sheet.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  tearDown(() {
    UrlLauncherService.testHandler = null;
  });

  group('DuetView PC Download Destinations Tests', () {
    test('UrlLauncherService.officialWebsite is https://duet-view.vercel.app/', () {
      expect(UrlLauncherService.officialWebsite, equals('https://duet-view.vercel.app/'));
    });

    testWidgets('GuideScreen Download PC button launches official website', (tester) async {
      String? launchedUrl;
      UrlLauncherService.testHandler = (url) async {
        launchedUrl = url;
        return true;
      };

      await tester.pumpWidget(
        const MaterialApp(
          home: GuideScreen(),
        ),
      );
      await tester.pumpAndSettle();

      // Find Download PC button in GuideScreen
      final downloadPcBtn = find.text('Download PC');
      expect(downloadPcBtn, findsOneWidget);

      await tester.scrollUntilVisible(downloadPcBtn, 200);
      await tester.tap(downloadPcBtn);
      await tester.pumpAndSettle();

      expect(launchedUrl, equals('https://duet-view.vercel.app/'));
      expect(launchedUrl, isNot(contains('github.com/Aryan130805/DuetView/releases')));
    });

    testWidgets('DownloadPcGuideSheet Visit Official Website button launches official website', (tester) async {
      String? launchedUrl;
      UrlLauncherService.testHandler = (url) async {
        launchedUrl = url;
        return true;
      };

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => ElevatedButton(
                onPressed: () => DownloadPcGuideSheet.show(context),
                child: const Text('Show Sheet'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('Show Sheet'));
      await tester.pumpAndSettle();

      // Verify sheet and buttons
      expect(find.byType(DownloadPcGuideSheet), findsOneWidget);
      expect(find.text('Close'), findsOneWidget);
      expect(find.text('Visit Official Website'), findsOneWidget);
      expect(find.text('Open GitHub Releases'), findsNothing);

      // Tap Visit Official Website
      await tester.tap(find.text('Visit Official Website'));
      await tester.pumpAndSettle();

      expect(launchedUrl, equals('https://duet-view.vercel.app/'));
      expect(launchedUrl, isNot(contains('github.com/Aryan130805/DuetView/releases')));
    });

    testWidgets('DownloadPcGuideSheet Close button dismisses popup sheet', (tester) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => ElevatedButton(
                onPressed: () => DownloadPcGuideSheet.show(context),
                child: const Text('Show Sheet'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('Show Sheet'));
      await tester.pumpAndSettle();

      expect(find.byType(DownloadPcGuideSheet), findsOneWidget);

      // Tap Close
      await tester.tap(find.text('Close'));
      await tester.pumpAndSettle();

      expect(find.byType(DownloadPcGuideSheet), findsNothing);
    });
  });
}
