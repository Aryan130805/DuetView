import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

/// Handler type for testing and custom URL launching.
typedef UrlLaunchHandler = Future<bool> Function(String url);

/// Service providing robust, dependency-free external URL launching for DuetView.
///
/// Communicates through the native platform bridge (`com.secondscreen/decoder_control`)
/// to trigger standard Android ACTION_VIEW intents. Includes automatic error handling,
/// clipboard copy fallback, and mock injection for testing.
class UrlLauncherService {
  static const MethodChannel _channel =
      MethodChannel('com.secondscreen/decoder_control');

  /// Optional override handler for automated tests or custom handling.
  static UrlLaunchHandler? testHandler;

  /// Known project URLs for convenience and consistency across DuetView.
  static const String githubReleases =
      'https://github.com/Aryan130805/DuetView/releases';
  static const String developerProfile =
      'https://github.com/Aryan130805';
  static const String projectRepository =
      'https://github.com/Aryan130805/DuetView';

  /// Launches an external [url] in the system browser.
  ///
  /// If the URL cannot be opened, displays a user-friendly SnackBar with an option
  /// to copy the URL directly to the device clipboard.
  static Future<bool> openUrl(
    BuildContext context,
    String url, {
    String? title,
  }) async {
    bool success = false;

    try {
      if (testHandler != null) {
        success = await testHandler!(url);
      } else {
        final res = await _channel.invokeMethod<bool>('launchUrl', {'url': url});
        success = res ?? false;
      }
    } catch (e) {
      debugPrint('UrlLauncherService: error launching $url: $e');
      success = false;
    }

    if (!success && context.mounted) {
      _showFallbackSnackBar(context, url, title: title);
    }

    return success;
  }

  /// Displays a SnackBar with a fallback "Copy Link" action.
  static void _showFallbackSnackBar(
    BuildContext context,
    String url, {
    String? title,
  }) {
    final scaffoldMessenger = ScaffoldMessenger.maybeOf(context);
    if (scaffoldMessenger == null) return;

    scaffoldMessenger.hideCurrentSnackBar();
    scaffoldMessenger.showSnackBar(
      SnackBar(
        content: Text(
          title != null
              ? 'Could not open $title. You can copy the link instead.'
              : 'Could not open link in browser.',
        ),
        duration: const Duration(seconds: 5),
        backgroundColor: const Color(0xFF1E293B),
        action: SnackBarAction(
          label: 'Copy Link',
          textColor: const Color(0xFF38BDF8),
          onPressed: () async {
            await Clipboard.setData(ClipboardData(text: url));
            if (context.mounted) {
              final sm = ScaffoldMessenger.maybeOf(context);
              sm?.hideCurrentSnackBar();
              sm?.showSnackBar(
                SnackBar(
                  content: Text('Link copied to clipboard: $url'),
                  duration: const Duration(seconds: 3),
                  backgroundColor: const Color(0xFF0F172A),
                ),
              );
            }
          },
        ),
      ),
    );
  }
}

