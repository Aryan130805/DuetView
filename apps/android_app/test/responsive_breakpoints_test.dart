import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:second_screen_android/widgets/responsive_container.dart';

void main() {
  group('WindowSizeClass & Responsive Breakpoints Tests', () {
    test('Correctly classifies width into WindowSizeClass', () {
      expect(WindowSizeClass.fromWidth(320), equals(WindowSizeClass.compact));
      expect(WindowSizeClass.fromWidth(412), equals(WindowSizeClass.compact));
      expect(WindowSizeClass.fromWidth(599), equals(WindowSizeClass.compact));

      expect(WindowSizeClass.fromWidth(600), equals(WindowSizeClass.medium));
      expect(WindowSizeClass.fromWidth(768), equals(WindowSizeClass.medium));
      expect(WindowSizeClass.fromWidth(839), equals(WindowSizeClass.medium));

      expect(WindowSizeClass.fromWidth(840), equals(WindowSizeClass.expanded));
      expect(WindowSizeClass.fromWidth(1024), equals(WindowSizeClass.expanded));
      expect(WindowSizeClass.fromWidth(1920), equals(WindowSizeClass.expanded));
    });

    test('Provides appropriate default max widths per size class', () {
      expect(
        ResponsiveContainer.defaultMaxWidth(WindowSizeClass.compact),
        equals(640.0),
      );
      expect(
        ResponsiveContainer.defaultMaxWidth(WindowSizeClass.medium),
        equals(740.0),
      );
      expect(
        ResponsiveContainer.defaultMaxWidth(WindowSizeClass.expanded),
        equals(920.0),
      );
    });

    testWidgets('ResponsiveContextExtensions detect size class in widget tree', (
      tester,
    ) async {
      late WindowSizeClass sizeClass;
      late bool isCompact;
      late bool isMedium;
      late bool isExpanded;

      Widget buildTestWidget(double width, double height) {
        return MediaQuery(
          data: MediaQueryData(size: Size(width, height)),
          child: Directionality(
            textDirection: TextDirection.ltr,
            child: Builder(
              builder: (context) {
                sizeClass = context.windowSizeClass;
                isCompact = context.isCompact;
                isMedium = context.isMedium;
                isExpanded = context.isExpanded;
                return const SizedBox();
              },
            ),
          ),
        );
      }

      // Compact (phone 360x780)
      await tester.pumpWidget(buildTestWidget(360, 780));
      expect(sizeClass, equals(WindowSizeClass.compact));
      expect(isCompact, isTrue);
      expect(isMedium, isFalse);
      expect(isExpanded, isFalse);

      // Medium (tablet 768x1024)
      await tester.pumpWidget(buildTestWidget(768, 1024));
      expect(sizeClass, equals(WindowSizeClass.medium));
      expect(isCompact, isFalse);
      expect(isMedium, isTrue);
      expect(isExpanded, isFalse);

      // Expanded (large tablet 1024x768)
      await tester.pumpWidget(buildTestWidget(1024, 768));
      expect(sizeClass, equals(WindowSizeClass.expanded));
      expect(isCompact, isFalse);
      expect(isMedium, isFalse);
      expect(isExpanded, isTrue);
    });

    testWidgets('AdaptiveTwoColumn stacks vertically on compact and side-by-side on wide', (
      tester,
    ) async {
      Widget buildTest(double width) {
        return MaterialApp(
          home: MediaQuery(
            data: MediaQueryData(size: Size(width, 800)),
            child: const Scaffold(
              body: AdaptiveTwoColumn(
                left: Text('LeftColumn'),
                right: Text('RightColumn'),
              ),
            ),
          ),
        );
      }

      // Compact: Column
      await tester.pumpWidget(buildTest(400));
      expect(find.text('LeftColumn'), findsOneWidget);
      expect(find.text('RightColumn'), findsOneWidget);
      expect(find.byType(Column), findsWidgets);

      // Expanded: Row
      await tester.pumpWidget(buildTest(900));
      expect(find.text('LeftColumn'), findsOneWidget);
      expect(find.text('RightColumn'), findsOneWidget);
      expect(find.byType(Row), findsWidgets);
    });
  });
}

