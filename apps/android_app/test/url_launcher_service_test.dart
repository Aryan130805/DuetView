import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/services/url_launcher_service.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  tearDown(() {
    UrlLauncherService.testHandler = null;
  });

  group('UrlLauncherService Tests', () {
    test('Targets correct official GitHub repository and release URLs', () {
      expect(
        UrlLauncherService.githubReleases,
        equals('https://github.com/Aryan130805/DuetView/releases'),
      );
      expect(
        UrlLauncherService.developerProfile,
        equals('https://github.com/Aryan130805'),
      );
      expect(
        UrlLauncherService.projectRepository,
        equals('https://github.com/Aryan130805/DuetView'),
      );
    });

    testWidgets('Invokes launch handler successfully when openUrl is called', (
      tester,
    ) async {
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
                onPressed: () => UrlLauncherService.openUrl(
                  context,
                  UrlLauncherService.githubReleases,
                ),
                child: const Text('Open'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('Open'));
      await tester.pump();

      expect(launchedUrl, equals('https://github.com/Aryan130805/DuetView/releases'));
      expect(find.byType(SnackBar), findsNothing);
    });

    testWidgets('Displays fallback SnackBar with Copy Link action when launch fails', (
      tester,
    ) async {
      String? clipboardText;
      tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
        SystemChannels.platform,
        (MethodCall call) async {
          if (call.method == 'Clipboard.setData') {
            clipboardText = (call.arguments as Map)['text'] as String?;
            return null;
          } else if (call.method == 'Clipboard.getData') {
            return {'text': clipboardText};
          }
          return null;
        },
      );
      addTearDown(() {
        tester.binding.defaultBinaryMessenger.setMockMethodCallHandler(
          SystemChannels.platform,
          null,
        );
      });

      UrlLauncherService.testHandler = (url) async => false;

      await tester.pumpWidget(
        MaterialApp(
          home: Scaffold(
            body: Builder(
              builder: (context) => ElevatedButton(
                onPressed: () => UrlLauncherService.openUrl(
                  context,
                  'https://github.com/Aryan130805',
                  title: 'Developer Profile',
                ),
                child: const Text('Open'),
              ),
            ),
          ),
        ),
      );

      await tester.tap(find.text('Open'));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 500));

      expect(find.byType(SnackBar), findsOneWidget);
      expect(
        find.text('Could not open Developer Profile. You can copy the link instead.'),
        findsOneWidget,
      );
      expect(find.text('Copy Link'), findsOneWidget);

      // Tap Copy Link
      await tester.tap(find.text('Copy Link'));
      await tester.pump();
      await tester.pump(const Duration(milliseconds: 300));

      final clipboardData = await Clipboard.getData(Clipboard.kTextPlain);
      expect(clipboardData?.text, equals('https://github.com/Aryan130805'));

      // Dismiss snackbars to ensure no pending timers
      ScaffoldMessenger.of(tester.element(find.byType(Scaffold))).clearSnackBars();
      await tester.pump();
    });
  });
}

