import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/connection_service.dart';
import '../services/frame_accumulator.dart';

/// Interaction mode for Android touch input
enum TouchInputMode {
  /// Touching directly clicks at the tapped desktop coordinate
  direct,

  /// Moving finger glides the pointer across the desktop like a trackpad
  trackpad,
}

/// Overlay that intercepts touch and gesture events on the streamed desktop viewport,
/// translates them to normalized [0.0, 1.0] coordinates accounting for aspect ratio and letterboxing,
/// and transmits SS02 input packets to the Windows host.
class TouchInputOverlay extends StatefulWidget {
  final Widget child;
  final ConnectionService connectionService;
  final double desktopAspectRatio;
  final bool enabled;
  final TouchInputMode inputMode;
  final ValueChanged<TouchInputMode>? onInputModeChanged;
  final VoidCallback? onToggleControls;

  const TouchInputOverlay({
    super.key,
    required this.child,
    required this.connectionService,
    required this.desktopAspectRatio,
    this.enabled = true,
    this.inputMode = TouchInputMode.direct,
    this.onInputModeChanged,
    this.onToggleControls,
  });

  @override
  State<TouchInputOverlay> createState() => _TouchInputOverlayState();
}

class _TouchInputOverlayState extends State<TouchInputOverlay> {
  static const double _kTouchSlop = 8.0;

  // Trackpad mode cursor position in normalized [0.0, 1.0] coordinates
  double _trackpadNormX = 0.5;
  double _trackpadNormY = 0.5;

  // Multi-touch tracking
  final Map<int, Offset> _activePointers = {};
  final Map<int, Offset> _pointerDownPositions = {};
  Offset? _scrollLastFocalPoint;

  bool _isDragging = false;
  bool _isLongPress = false;
  Timer? _longPressTimer;

  // Double-tap detection for direct mode
  DateTime? _lastTapTime;
  Offset? _lastTapPos;

  // Touch ripple indicator for direct mode
  Offset? _ripplePosition;
  double _rippleOpacity = 0.0;
  Timer? _rippleTimer;

  void _triggerRipple(Offset localPos) {
    _rippleTimer?.cancel();
    setState(() {
      _ripplePosition = localPos;
      _rippleOpacity = 1.0;
    });
    _rippleTimer = Timer(const Duration(milliseconds: 280), () {
      if (mounted) {
        setState(() {
          _rippleOpacity = 0.0;
        });
      }
    });
  }

  /// Calculates the letterboxed desktop rectangle within the parent container
  Rect _calculateDesktopRect(Size containerSize) {
    final double ar = widget.desktopAspectRatio > 0 ? widget.desktopAspectRatio : 16 / 9;
    final double containerAr = containerSize.width / containerSize.height;

    double renderedWidth;
    double renderedHeight;
    double offsetX = 0;
    double offsetY = 0;

    if (containerAr > ar) {
      // Container is wider than desktop: pillarboxed (bars on left/right)
      renderedHeight = containerSize.height;
      renderedWidth = containerSize.height * ar;
      offsetX = (containerSize.width - renderedWidth) / 2;
    } else {
      // Container is taller than desktop: letterboxed (bars on top/bottom)
      renderedWidth = containerSize.width;
      renderedHeight = containerSize.width / ar;
      offsetY = (containerSize.height - renderedHeight) / 2;
    }

    return Rect.fromLTWH(offsetX, offsetY, renderedWidth, renderedHeight);
  }

  /// Converts a local widget position to normalized [0.0, 1.0] desktop coordinates.
  /// Returns null if the touch is within the unused letterbox/pillarbox margins.
  Offset? _toNormalizedCoordinates(Offset localPos, Size containerSize) {
    final rect = _calculateDesktopRect(containerSize);

    if (localPos.dx < rect.left ||
        localPos.dx > rect.right ||
        localPos.dy < rect.top ||
        localPos.dy > rect.bottom) {
      return null; // Touch in letterbox margin: ignore
    }

    final normX = ((localPos.dx - rect.left) / rect.width).clamp(0.0, 1.0);
    final normY = ((localPos.dy - rect.top) / rect.height).clamp(0.0, 1.0);
    return Offset(normX, normY);
  }

  void _performLeftClick() {
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.move,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.buttonDown,
      button: PointerButton.left,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.buttonUp,
      button: PointerButton.left,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
  }

  void _performRightClick() {
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.move,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.buttonDown,
      button: PointerButton.right,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
    widget.connectionService.sendPointerEvent(
      type: PointerEventType.buttonUp,
      button: PointerButton.right,
      normalizedX: _trackpadNormX,
      normalizedY: _trackpadNormY,
    );
  }

  @override
  void dispose() {
    _longPressTimer?.cancel();
    _rippleTimer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return LayoutBuilder(
      builder: (context, constraints) {
        final containerSize = Size(constraints.maxWidth, constraints.maxHeight);
        final desktopRect = _calculateDesktopRect(containerSize);

        return Listener(
          behavior: HitTestBehavior.opaque,
          onPointerDown: (event) {
            _activePointers[event.pointer] = event.localPosition;
            _pointerDownPositions[event.pointer] = event.localPosition;

            // 3-finger tap toggles UI controls
            if (_activePointers.length == 3) {
              widget.onToggleControls?.call();
              return;
            }

            if (!widget.enabled) return;

            final norm = _toNormalizedCoordinates(event.localPosition, containerSize);
            if (norm == null) return;

            if (widget.inputMode == TouchInputMode.direct && _activePointers.length == 1) {
              _isLongPress = false;
              _isDragging = false;

              // Long-press timer (500ms) triggers right-click
              _longPressTimer?.cancel();
              _longPressTimer = Timer(const Duration(milliseconds: 500), () {
                _isLongPress = true;
                HapticFeedback.mediumImpact();
                widget.connectionService.sendPointerEvent(
                  type: PointerEventType.move,
                  normalizedX: norm.dx,
                  normalizedY: norm.dy,
                );
                widget.connectionService.sendPointerEvent(
                  type: PointerEventType.buttonDown,
                  button: PointerButton.right,
                  normalizedX: norm.dx,
                  normalizedY: norm.dy,
                );
                widget.connectionService.sendPointerEvent(
                  type: PointerEventType.buttonUp,
                  button: PointerButton.right,
                  normalizedX: norm.dx,
                  normalizedY: norm.dy,
                );
              });
            }
          },
          onPointerMove: (event) {
            _activePointers[event.pointer] = event.localPosition;

            if (!widget.enabled) return;

            // Two-finger scrolling
            if (_activePointers.length == 2) {
              _longPressTimer?.cancel();
              if (_isDragging) {
                _isDragging = false;
                widget.connectionService.sendPointerEvent(
                  type: PointerEventType.buttonUp,
                  button: PointerButton.left,
                  normalizedX: _trackpadNormX,
                  normalizedY: _trackpadNormY,
                );
              }

              final pointers = _activePointers.values.toList();
              final focalPoint = (pointers[0] + pointers[1]) / 2;

              if (_scrollLastFocalPoint != null) {
                final delta = focalPoint - _scrollLastFocalPoint!;
                final deltaX = (delta.dx * 3).toInt();
                final deltaY = (delta.dy * 3).toInt();

                final normPos = _toNormalizedCoordinates(focalPoint, containerSize) ??
                    Offset(_trackpadNormX, _trackpadNormY);

                if (deltaX.abs() > 1 || deltaY.abs() > 1) {
                  widget.connectionService.sendScrollEvent(
                    normalizedX: normPos.dx,
                    normalizedY: normPos.dy,
                    deltaX: deltaX,
                    deltaY: deltaY,
                  );
                }
              }
              _scrollLastFocalPoint = focalPoint;
              return;
            }

            _scrollLastFocalPoint = null;

            if (_activePointers.length == 1) {
              final norm = _toNormalizedCoordinates(event.localPosition, containerSize);

              if (widget.inputMode == TouchInputMode.direct) {
                if (norm == null) return;
                final downPos = _pointerDownPositions[event.pointer];

                if (!_isDragging) {
                  if (downPos != null && (event.localPosition - downPos).distance > _kTouchSlop) {
                    // Finger moved beyond touch slop threshold -> start drag
                    _longPressTimer?.cancel();
                    _isDragging = true;

                    final downNorm = _toNormalizedCoordinates(downPos, containerSize) ?? norm;
                    widget.connectionService.sendPointerEvent(
                      type: PointerEventType.move,
                      normalizedX: downNorm.dx,
                      normalizedY: downNorm.dy,
                    );
                    widget.connectionService.sendPointerEvent(
                      type: PointerEventType.buttonDown,
                      button: PointerButton.left,
                      normalizedX: downNorm.dx,
                      normalizedY: downNorm.dy,
                    );
                    widget.connectionService.sendPointerEvent(
                      type: PointerEventType.move,
                      normalizedX: norm.dx,
                      normalizedY: norm.dy,
                    );
                  }
                } else {
                  // Actively dragging
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.move,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                }
              } else if (widget.inputMode == TouchInputMode.trackpad) {
                // Glide pointer with smooth acceleration
                final deltaX = (event.delta.dx / desktopRect.width) * 1.15;
                final deltaY = (event.delta.dy / desktopRect.height) * 1.15;
                setState(() {
                  _trackpadNormX = (_trackpadNormX + deltaX).clamp(0.0, 1.0);
                  _trackpadNormY = (_trackpadNormY + deltaY).clamp(0.0, 1.0);
                });
                widget.connectionService.sendPointerEvent(
                  type: PointerEventType.move,
                  normalizedX: _trackpadNormX,
                  normalizedY: _trackpadNormY,
                );
              }
            }
          },
          onPointerUp: (event) {
            _activePointers.remove(event.pointer);
            final downPos = _pointerDownPositions.remove(event.pointer);
            _longPressTimer?.cancel();
            _scrollLastFocalPoint = null;

            if (!widget.enabled) return;

            final norm = _toNormalizedCoordinates(event.localPosition, containerSize);

            if (widget.inputMode == TouchInputMode.direct) {
              if (_isDragging) {
                _isDragging = false;
                if (norm != null) {
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.buttonUp,
                    button: PointerButton.left,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                }
              } else if (!_isLongPress && norm != null) {
                // Clean tap detected
                final now = DateTime.now();
                final isDoubleTap = _lastTapTime != null &&
                    now.difference(_lastTapTime!) < const Duration(milliseconds: 320) &&
                    _lastTapPos != null &&
                    (event.localPosition - _lastTapPos!).distance < 25;

                _triggerRipple(event.localPosition);

                if (isDoubleTap) {
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.move,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.doubleClick,
                    button: PointerButton.left,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                  _lastTapTime = null;
                  _lastTapPos = null;
                } else {
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.move,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.buttonDown,
                    button: PointerButton.left,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                  widget.connectionService.sendPointerEvent(
                    type: PointerEventType.buttonUp,
                    button: PointerButton.left,
                    normalizedX: norm.dx,
                    normalizedY: norm.dy,
                  );
                  _lastTapTime = now;
                  _lastTapPos = event.localPosition;
                }
              }
            } else if (widget.inputMode == TouchInputMode.trackpad && _activePointers.isEmpty) {
              // Trackpad tap-to-click
              if (downPos != null && (event.localPosition - downPos).distance < 6) {
                _performLeftClick();
              }
            }
          },
          onPointerCancel: (event) {
            _activePointers.remove(event.pointer);
            _pointerDownPositions.remove(event.pointer);
            _longPressTimer?.cancel();
            _scrollLastFocalPoint = null;
            if (_isDragging) {
              _isDragging = false;
              widget.connectionService.sendPointerEvent(
                type: PointerEventType.buttonUp,
                button: PointerButton.left,
                normalizedX: _trackpadNormX,
                normalizedY: _trackpadNormY,
              );
            }
          },
          child: Stack(
            clipBehavior: Clip.none,
            children: [
              // 1. Viewport child
              widget.child,

              // 2. Direct Touch Ripple feedback
              if (_ripplePosition != null && _rippleOpacity > 0.0)
                Positioned(
                  left: _ripplePosition!.dx - 24,
                  top: _ripplePosition!.dy - 24,
                  child: IgnorePointer(
                    child: AnimatedOpacity(
                      duration: const Duration(milliseconds: 250),
                      opacity: _rippleOpacity,
                      child: Container(
                        width: 48,
                        height: 48,
                        decoration: BoxDecoration(
                          shape: BoxShape.circle,
                          color: Colors.white.withValues(alpha: 0.35),
                          border: Border.all(color: Colors.white, width: 2),
                          boxShadow: [
                            BoxShadow(
                              color: Colors.white.withValues(alpha: 0.5),
                              blurRadius: 10,
                              spreadRadius: 2,
                            ),
                          ],
                        ),
                      ),
                    ),
                  ),
                ),

              // 3. Trackpad Mode on-screen pointer cursor
              if (widget.inputMode == TouchInputMode.trackpad && widget.enabled)
                Positioned(
                  left: desktopRect.left + (_trackpadNormX * desktopRect.width) - 2,
                  top: desktopRect.top + (_trackpadNormY * desktopRect.height) - 2,
                  child: IgnorePointer(
                    child: Container(
                      decoration: const BoxDecoration(
                        boxShadow: [
                          BoxShadow(
                            color: Colors.black45,
                            blurRadius: 4,
                            offset: Offset(1, 2),
                          ),
                        ],
                      ),
                      child: const Icon(
                        Icons.navigation,
                        color: Colors.white,
                        size: 20,
                      ),
                    ),
                  ),
                ),

              // 4. Dedicated Trackpad Controls & Click Buttons (Trackpad mode only)
              if (widget.inputMode == TouchInputMode.trackpad && widget.enabled)
                Positioned(
                  left: 16,
                  right: 16,
                  bottom: 16,
                  child: Center(
                    child: Container(
                      padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                      decoration: BoxDecoration(
                        color: const Color(0xE60F172A),
                        borderRadius: BorderRadius.circular(16),
                        border: Border.all(color: const Color(0xFF334155)),
                        boxShadow: const [
                          BoxShadow(
                            color: Colors.black54,
                            blurRadius: 12,
                            offset: Offset(0, 4),
                          ),
                        ],
                      ),
                      child: Row(
                        mainAxisSize: MainAxisSize.min,
                        children: [
                          // Left Click Button
                          ElevatedButton.icon(
                            style: ElevatedButton.styleFrom(
                              backgroundColor: const Color(0xFF1E293B),
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
                              shape: RoundedRectangleBorder(
                                borderRadius: BorderRadius.circular(10),
                                side: const BorderSide(color: Color(0xFF475569)),
                              ),
                            ),
                            icon: const Icon(Icons.touch_app, size: 18, color: Color(0xFF38BDF8)),
                            label: const Text('Left Click', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 13)),
                            onPressed: _performLeftClick,
                          ),
                          const SizedBox(width: 8),

                          // Right Click Button
                          ElevatedButton.icon(
                            style: ElevatedButton.styleFrom(
                              backgroundColor: const Color(0xFF1E293B),
                              foregroundColor: Colors.white,
                              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
                              shape: RoundedRectangleBorder(
                                borderRadius: BorderRadius.circular(10),
                                side: const BorderSide(color: Color(0xFF475569)),
                              ),
                            ),
                            icon: const Icon(Icons.mouse, size: 18, color: Color(0xFFF59E0B)),
                            label: const Text('Right Click', style: TextStyle(fontWeight: FontWeight.bold, fontSize: 13)),
                            onPressed: _performRightClick,
                          ),
                          const SizedBox(width: 12),

                          // Mode Indicator & Quick Switch
                          InkWell(
                            borderRadius: BorderRadius.circular(8),
                            onTap: () {
                              widget.onInputModeChanged?.call(TouchInputMode.direct);
                            },
                            child: Container(
                              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
                              decoration: BoxDecoration(
                                color: const Color(0xFF0284C7).withValues(alpha: 0.25),
                                borderRadius: BorderRadius.circular(8),
                                border: Border.all(color: const Color(0xFF38BDF8).withValues(alpha: 0.5)),
                              ),
                              child: const Row(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  Icon(Icons.touch_app, size: 15, color: Color(0xFF38BDF8)),
                                  SizedBox(width: 4),
                                  Text(
                                    'Switch to Direct',
                                    style: TextStyle(
                                      color: Color(0xFF38BDF8),
                                      fontSize: 11,
                                      fontWeight: FontWeight.w600,
                                    ),
                                  ),
                                ],
                              ),
                            ),
                          ),
                        ],
                      ),
                    ),
                  ),
                ),
            ],
          ),
        );
      },
    );
  }
}
