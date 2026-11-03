import 'package:flutter/material.dart';

/// Screen width breakpoints aligned with Material 3 adaptive design guidelines:
/// - Compact: < 600 dp (Standard mobile phones, small portrait screens, folded foldables)
/// - Medium: 600 dp .. 839 dp (Small tablets, 7"-8" slates, unfolded foldables, wide landscapes)
/// - Expanded: >= 840 dp (Large tablets 10"+, desktop viewports, wide split-screen windows)
enum WindowSizeClass {
  compact,
  medium,
  expanded;

  /// Returns the [WindowSizeClass] corresponding to a given logical [width].
  static WindowSizeClass fromWidth(double width) {
    if (width < 600) return WindowSizeClass.compact;
    if (width < 840) return WindowSizeClass.medium;
    return WindowSizeClass.expanded;
  }
}

/// Helpful layout extensions on [BuildContext] for responsive queries.
extension ResponsiveContextExtensions on BuildContext {
  /// Current logical screen width.
  double get screenWidth => MediaQuery.sizeOf(this).width;

  /// Current logical screen height.
  double get screenHeight => MediaQuery.sizeOf(this).height;

  /// Current [WindowSizeClass] based on screen width.
  WindowSizeClass get windowSizeClass =>
      WindowSizeClass.fromWidth(MediaQuery.sizeOf(this).width);

  /// True if current width is compact (< 600 dp).
  bool get isCompact => windowSizeClass == WindowSizeClass.compact;

  /// True if current width is medium (600 dp .. 839 dp).
  bool get isMedium => windowSizeClass == WindowSizeClass.medium;

  /// True if current width is expanded (>= 840 dp).
  bool get isExpanded => windowSizeClass == WindowSizeClass.expanded;

  /// True if in landscape orientation.
  bool get isLandscape =>
      MediaQuery.orientationOf(this) == Orientation.landscape;

  /// True if in portrait orientation.
  bool get isPortrait =>
      MediaQuery.orientationOf(this) == Orientation.portrait;
}

/// Ensures content scales comfortably across mobile phones, foldables, and wide tablets.
///
/// Automatically constrains content width and applies comfortable padding
/// based on the current window size class unless explicitly overridden.
class ResponsiveContainer extends StatelessWidget {
  final Widget child;
  final double? maxWidth;
  final EdgeInsetsGeometry? padding;

  const ResponsiveContainer({
    super.key,
    required this.child,
    this.maxWidth,
    this.padding,
  });

  /// Computes default maximum width according to [sizeClass].
  static double defaultMaxWidth(WindowSizeClass sizeClass) {
    switch (sizeClass) {
      case WindowSizeClass.compact:
        return 640.0;
      case WindowSizeClass.medium:
        return 740.0;
      case WindowSizeClass.expanded:
        return 920.0;
    }
  }

  /// Computes default content padding according to [sizeClass].
  static EdgeInsets defaultPadding(WindowSizeClass sizeClass) {
    switch (sizeClass) {
      case WindowSizeClass.compact:
        return const EdgeInsets.all(16.0);
      case WindowSizeClass.medium:
        return const EdgeInsets.symmetric(horizontal: 24.0, vertical: 20.0);
      case WindowSizeClass.expanded:
        return const EdgeInsets.symmetric(horizontal: 32.0, vertical: 24.0);
    }
  }

  @override
  Widget build(BuildContext context) {
    final sizeClass = context.windowSizeClass;
    final effectiveMaxWidth = maxWidth ?? defaultMaxWidth(sizeClass);
    final effectivePadding = padding ?? defaultPadding(sizeClass);

    return Center(
      child: ConstrainedBox(
        constraints: BoxConstraints(maxWidth: effectiveMaxWidth),
        child: Padding(
          padding: effectivePadding,
          child: child,
        ),
      ),
    );
  }
}

/// Renders different UI layouts based on the active [WindowSizeClass].
class AdaptiveLayout extends StatelessWidget {
  final WidgetBuilder compact;
  final WidgetBuilder? medium;
  final WidgetBuilder? expanded;

  const AdaptiveLayout({
    super.key,
    required this.compact,
    this.medium,
    this.expanded,
  });

  @override
  Widget build(BuildContext context) {
    final sizeClass = context.windowSizeClass;
    switch (sizeClass) {
      case WindowSizeClass.expanded:
        if (expanded != null) return expanded!(context);
        if (medium != null) return medium!(context);
        return compact(context);
      case WindowSizeClass.medium:
        if (medium != null) return medium!(context);
        return compact(context);
      case WindowSizeClass.compact:
        return compact(context);
    }
  }
}

/// Adaptive two-column container that stacks vertically on compact screens
/// and places columns side-by-side on medium and expanded displays.
class AdaptiveTwoColumn extends StatelessWidget {
  final Widget left;
  final Widget right;
  final double spacing;
  final int leftFlex;
  final int rightFlex;
  final CrossAxisAlignment crossAxisAlignment;

  const AdaptiveTwoColumn({
    super.key,
    required this.left,
    required this.right,
    this.spacing = 16.0,
    this.leftFlex = 1,
    this.rightFlex = 1,
    this.crossAxisAlignment = CrossAxisAlignment.start,
  });

  @override
  Widget build(BuildContext context) {
    if (context.isCompact) {
      return Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          left,
          SizedBox(height: spacing),
          right,
        ],
      );
    }

    return Row(
      crossAxisAlignment: crossAxisAlignment,
      children: [
        Expanded(flex: leftFlex, child: left),
        SizedBox(width: spacing),
        Expanded(flex: rightFlex, child: right),
      ],
    );
  }
}
