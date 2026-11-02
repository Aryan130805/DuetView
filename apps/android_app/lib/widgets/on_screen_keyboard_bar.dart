import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../services/connection_service.dart';
import '../services/frame_accumulator.dart';

/// Floating virtual keyboard accessory bar for DuetView.
/// Provides Windows modifier keys (Ctrl, Shift, Alt, Win), navigation keys (Esc, Tab, Enter, Backspace, Arrows),
/// quick shortcut chords (Ctrl+C, Ctrl+V, Ctrl+A, Ctrl+Z, Alt+Tab, Win+D, Win+P),
/// and a text field with reliable Backspace deletion and Unicode typing.
class OnScreenKeyboardBar extends StatefulWidget {
  final ConnectionService connectionService;
  final VoidCallback onClose;

  const OnScreenKeyboardBar({
    super.key,
    required this.connectionService,
    required this.onClose,
  });

  @override
  State<OnScreenKeyboardBar> createState() => _OnScreenKeyboardBarState();
}

class _OnScreenKeyboardBarState extends State<OnScreenKeyboardBar> {
  static const String _sentinel = '\u200B'; // Zero-width space for soft keyboard backspace detection

  final TextEditingController _textController = TextEditingController();
  final FocusNode _focusNode = FocusNode();

  bool _ctrlActive = false;
  bool _shiftActive = false;
  bool _altActive = false;
  bool _winActive = false;

  int get _currentModifiers {
    int mods = ModifierFlags.none;
    if (_shiftActive) mods |= ModifierFlags.shift;
    if (_ctrlActive) mods |= ModifierFlags.ctrl;
    if (_altActive) mods |= ModifierFlags.alt;
    if (_winActive) mods |= ModifierFlags.meta;
    return mods;
  }

  @override
  void initState() {
    super.initState();
    _resetSentinel();
  }

  void _resetSentinel() {
    _textController.value = const TextEditingValue(
      text: _sentinel,
      selection: TextSelection.collapsed(offset: 1),
    );
  }

  void _sendKey(int vkCode) {
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyDown,
      keyCode: vkCode,
      modifiers: _currentModifiers,
    );
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyUp,
      keyCode: vkCode,
      modifiers: _currentModifiers,
    );

    // Reset single-shot modifiers unless locked
    if (_ctrlActive || _shiftActive || _altActive || _winActive) {
      setState(() {
        _ctrlActive = false;
        _shiftActive = false;
        _altActive = false;
        _winActive = false;
      });
    }
  }

  void _sendChord(int modifierVk, int keyVk) {
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyDown,
      keyCode: modifierVk,
    );
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyDown,
      keyCode: keyVk,
    );
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyUp,
      keyCode: keyVk,
    );
    widget.connectionService.sendKeyboardEvent(
      type: KeyEventType.keyUp,
      keyCode: modifierVk,
    );
  }

  void _sendUnicodeString(String text) {
    for (final char in text.runes) {
      widget.connectionService.sendKeyboardEvent(
        type: KeyEventType.unicodeChar,
        unicode: char,
        modifiers: _currentModifiers,
      );
    }
  }

  @override
  void dispose() {
    _textController.dispose();
    _focusNode.dispose();
    super.dispose();
  }

  Widget _buildModifierKey(String label, bool isActive, VoidCallback onToggle) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2),
      child: Material(
        color: isActive ? const Color(0xFF0284C7) : const Color(0xFF252B3B),
        borderRadius: BorderRadius.circular(6),
        child: InkWell(
          borderRadius: BorderRadius.circular(6),
          onTap: onToggle,
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 8),
            alignment: Alignment.center,
            child: Text(
              label,
              style: TextStyle(
                color: isActive ? Colors.white : Colors.white70,
                fontSize: 12,
                fontWeight: FontWeight.bold,
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildActionKey(String label, int vkCode) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2),
      child: Material(
        color: const Color(0xFF252B3B),
        borderRadius: BorderRadius.circular(6),
        child: InkWell(
          borderRadius: BorderRadius.circular(6),
          onTap: () => _sendKey(vkCode),
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 9, vertical: 8),
            alignment: Alignment.center,
            child: Text(
              label,
              style: const TextStyle(
                color: Colors.white,
                fontSize: 12,
                fontWeight: FontWeight.w600,
              ),
            ),
          ),
        ),
      ),
    );
  }

  Widget _buildIconKey(IconData icon, int vkCode) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2),
      child: Material(
        color: const Color(0xFF252B3B),
        borderRadius: BorderRadius.circular(6),
        child: InkWell(
          borderRadius: BorderRadius.circular(6),
          onTap: () => _sendKey(vkCode),
          child: Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 6),
            alignment: Alignment.center,
            child: Icon(icon, color: Colors.white, size: 16),
          ),
        ),
      ),
    );
  }

  Widget _buildShortcutButton(String label, VoidCallback onPressed) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 2),
      child: OutlinedButton(
        style: OutlinedButton.styleFrom(
          foregroundColor: const Color(0xFF38BDF8),
          side: const BorderSide(color: Color(0xFF334155)),
          backgroundColor: const Color(0xFF1E293B),
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 4),
          minimumSize: const Size(44, 32),
        ),
        onPressed: onPressed,
        child: Text(
          label,
          style: const TextStyle(fontSize: 11, fontWeight: FontWeight.bold),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: const Color(0xF210141F),
        borderRadius: const BorderRadius.vertical(top: Radius.circular(12)),
        border: Border.all(color: Colors.white24),
        boxShadow: const [
          BoxShadow(
            color: Colors.black54,
            blurRadius: 10,
            offset: Offset(0, -3),
          ),
        ],
      ),
      child: SafeArea(
        top: false,
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            // Row 1: Direct text input field and Close button
            Padding(
              padding: const EdgeInsets.fromLTRB(8, 8, 8, 4),
              child: Row(
                children: [
                  Expanded(
                    child: SizedBox(
                      height: 38,
                      child: Focus(
                        onKeyEvent: (node, event) {
                          if (event is KeyDownEvent) {
                            if (event.logicalKey == LogicalKeyboardKey.backspace) {
                              _sendKey(0x08);
                              return KeyEventResult.handled;
                            }
                          }
                          return KeyEventResult.ignored;
                        },
                        child: TextField(
                          controller: _textController,
                          focusNode: _focusNode,
                          style: const TextStyle(color: Colors.white, fontSize: 13),
                          decoration: InputDecoration(
                            hintText: 'Type text here to send to Windows...',
                            hintStyle: const TextStyle(color: Colors.white38, fontSize: 12),
                            contentPadding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                            filled: true,
                            fillColor: const Color(0xFF1B2130),
                            border: OutlineInputBorder(
                              borderRadius: BorderRadius.circular(8),
                              borderSide: BorderSide.none,
                            ),
                          ),
                          onChanged: (text) {
                            if (text.isEmpty) {
                              // Android soft keyboard deleted the sentinel -> user hit Backspace!
                              _sendKey(0x08); // VK_BACK
                              _resetSentinel();
                              return;
                            }
                            if (text.length > 1) {
                              // User entered text!
                              final entered = text.replaceAll(_sentinel, '');
                              if (entered.isNotEmpty) {
                                _sendUnicodeString(entered);
                              }
                              _resetSentinel();
                            }
                          },
                        ),
                      ),
                    ),
                  ),
                  const SizedBox(width: 8),
                  IconButton(
                    icon: const Icon(Icons.close, color: Colors.white70, size: 20),
                    onPressed: widget.onClose,
                    tooltip: 'Close Keyboard',
                  ),
                ],
              ),
            ),

            // Row 2: Modifiers, Navigation keys, and Shortcuts
            SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.fromLTRB(6, 2, 6, 8),
              child: Row(
                children: [
                  // Modifiers
                  _buildModifierKey('Ctrl', _ctrlActive, () => setState(() => _ctrlActive = !_ctrlActive)),
                  _buildModifierKey('Shift', _shiftActive, () => setState(() => _shiftActive = !_shiftActive)),
                  _buildModifierKey('Alt', _altActive, () => setState(() => _altActive = !_altActive)),
                  _buildModifierKey('Win', _winActive, () => setState(() => _winActive = !_winActive)),

                  const SizedBox(width: 6),

                  // Standard navigation keys (VK_* Win32 codes)
                  _buildActionKey('Esc', 0x1B),
                  _buildActionKey('Tab', 0x09),
                  _buildActionKey('Enter', 0x0D),
                  _buildActionKey('Bksp', 0x08),
                  _buildActionKey('Del', 0x2E),

                  const SizedBox(width: 6),

                  // Arrow keys
                  _buildIconKey(Icons.arrow_upward, 0x26),
                  _buildIconKey(Icons.arrow_downward, 0x28),
                  _buildIconKey(Icons.arrow_back, 0x25),
                  _buildIconKey(Icons.arrow_forward, 0x27),

                  const SizedBox(width: 6),

                  // Functional Windows Shortcut Chords
                  _buildShortcutButton('Ctrl+C', () => _sendChord(0x11, 0x43)),
                  _buildShortcutButton('Ctrl+V', () => _sendChord(0x11, 0x56)),
                  _buildShortcutButton('Ctrl+A', () => _sendChord(0x11, 0x41)),
                  _buildShortcutButton('Ctrl+Z', () => _sendChord(0x11, 0x5A)),
                  _buildShortcutButton('Alt+Tab', () => _sendChord(0x12, 0x09)),
                  _buildShortcutButton('Win+D', () => _sendChord(0x5B, 0x44)),
                  _buildShortcutButton('Win+P', () => _sendChord(0x5B, 0x50)),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }
}
