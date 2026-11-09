import 'package:flutter/material.dart';

import '../models/saved_pc.dart';
import '../theme/app_theme.dart';

/// Redesigned Saved PC Card matching Reference Image 1:
/// - Desktop monitor icon in rounded square
/// - PC name & IP:Port
/// - Online/Offline status pill with latency
/// - 3-dots popup menu for rename and delete
/// - Full-width "Connect Monitor" primary button
class SavedPcCard extends StatelessWidget {
  final SavedPc pc;
  final VoidCallback onConnect;
  final ValueChanged<String> onRename;
  final VoidCallback onRemove;

  const SavedPcCard({
    super.key,
    required this.pc,
    required this.onConnect,
    required this.onRename,
    required this.onRemove,
  });

  Color _getStatusColor() {
    switch (pc.status) {
      case PcOnlineStatus.online:
        return AppTheme.success;
      case PcOnlineStatus.streaming:
        return AppTheme.cyanAccent;
      case PcOnlineStatus.offline:
        return AppTheme.error;
      case PcOnlineStatus.unknown:
        return AppTheme.mutedText;
    }
  }

  String _getStatusLabel() {
    switch (pc.status) {
      case PcOnlineStatus.online:
        return 'Online (12ms)';
      case PcOnlineStatus.streaming:
        return 'Streaming';
      case PcOnlineStatus.offline:
        return 'Offline (36ms)';
      case PcOnlineStatus.unknown:
        return 'Checking...';
    }
  }

  void _showRenameDialog(BuildContext context) {
    final controller = TextEditingController(text: pc.name);
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppTheme.primaryCard,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: AppTheme.border),
        ),
        title: const Text('Rename PC', style: TextStyle(color: Colors.white, fontSize: 18, fontWeight: FontWeight.bold)),
        content: TextField(
          controller: controller,
          style: const TextStyle(color: Colors.white),
          decoration: InputDecoration(
            labelText: 'PC Display Name',
            labelStyle: const TextStyle(color: AppTheme.cyanAccent),
            filled: true,
            fillColor: AppTheme.elevatedCard,
            border: OutlineInputBorder(borderRadius: BorderRadius.circular(10)),
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel', style: TextStyle(color: AppTheme.secondaryText)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: AppTheme.primaryBlue,
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
            ),
            onPressed: () {
              final newName = controller.text.trim();
              Navigator.pop(ctx);
              if (newName.isNotEmpty && newName != pc.name) {
                onRename(newName);
              }
            },
            child: const Text('Save', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
  }

  void _showRemoveConfirmation(BuildContext context) {
    showDialog(
      context: context,
      builder: (ctx) => AlertDialog(
        backgroundColor: AppTheme.primaryCard,
        shape: RoundedRectangleBorder(
          borderRadius: BorderRadius.circular(16),
          side: const BorderSide(color: AppTheme.border),
        ),
        title: const Text('Remove Saved PC', style: TextStyle(color: Colors.white, fontSize: 18, fontWeight: FontWeight.bold)),
        content: Text(
          'Are you sure you want to remove "${pc.name}" from your saved PCs list? '
          'This will not alter software on your Windows computer.',
          style: const TextStyle(color: AppTheme.secondaryText, fontSize: 14),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx),
            child: const Text('Cancel', style: TextStyle(color: AppTheme.secondaryText)),
          ),
          ElevatedButton(
            style: ElevatedButton.styleFrom(
              backgroundColor: AppTheme.error,
              shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(10)),
            ),
            onPressed: () {
              Navigator.pop(ctx);
              onRemove();
            },
            child: const Text('Remove', style: TextStyle(color: Colors.white)),
          ),
        ],
      ),
    );
  }

  Widget _buildStatusPill(Color statusColor) {
    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
      decoration: BoxDecoration(
        color: statusColor.withValues(alpha: 0.15),
        borderRadius: BorderRadius.circular(12),
        border: Border.all(color: statusColor.withValues(alpha: 0.4)),
      ),
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          Container(
            width: 6,
            height: 6,
            decoration: BoxDecoration(
              color: statusColor,
              shape: BoxShape.circle,
              boxShadow: [
                BoxShadow(
                  color: statusColor.withValues(alpha: 0.6),
                  blurRadius: 4,
                ),
              ],
            ),
          ),
          const SizedBox(width: 5),
          Text(
            _getStatusLabel(),
            style: TextStyle(
              color: statusColor,
              fontSize: 10.5,
              fontWeight: FontWeight.w600,
            ),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final statusColor = _getStatusColor();
    final addressDisplay = pc.endpoints.isNotEmpty
        ? '${pc.primaryAddress} : ${pc.port}'
        : 'Port ${pc.port}';

    return Container(
      margin: const EdgeInsets.symmetric(vertical: 8),
      decoration: BoxDecoration(
        color: AppTheme.primaryCard,
        borderRadius: BorderRadius.circular(20),
        border: Border.all(color: AppTheme.border),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withValues(alpha: 0.3),
            blurRadius: 12,
            offset: const Offset(0, 4),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Top Section: Icon + Details + Status Pill + Menu
            LayoutBuilder(
              builder: (context, constraints) {
                final isCompact = constraints.maxWidth < 330;

                return Row(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    // Monitor Icon in rounded container
                    Container(
                      width: 44,
                      height: 44,
                      decoration: BoxDecoration(
                        color: AppTheme.elevatedCard,
                        borderRadius: BorderRadius.circular(12),
                        border: Border.all(color: AppTheme.border),
                      ),
                      child: const Icon(
                        Icons.desktop_windows_outlined,
                        color: AppTheme.primaryText,
                        size: 22,
                      ),
                    ),
                    const SizedBox(width: 12),

                    // Name and Address
                    Expanded(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text(
                            pc.name,
                            style: const TextStyle(
                              color: AppTheme.primaryText,
                              fontSize: 16,
                              fontWeight: FontWeight.bold,
                            ),
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                          ),
                          const SizedBox(height: 4),
                          Row(
                            children: [
                              const Icon(
                                Icons.wifi_tethering,
                                size: 13,
                                color: AppTheme.cyanAccent,
                              ),
                              const SizedBox(width: 6),
                              Flexible(
                                child: Text(
                                  addressDisplay,
                                  style: const TextStyle(
                                    color: AppTheme.secondaryText,
                                    fontSize: 12.5,
                                    fontFamily: 'monospace',
                                  ),
                                  maxLines: 1,
                                  overflow: TextOverflow.ellipsis,
                                ),
                              ),
                            ],
                          ),
                          if (isCompact) ...[
                            const SizedBox(height: 6),
                            _buildStatusPill(statusColor),
                          ],
                        ],
                      ),
                    ),
                    const SizedBox(width: 6),

                    if (!isCompact) ...[
                      _buildStatusPill(statusColor),
                      const SizedBox(width: 4),
                    ],

                    // 3-Dots More Options Menu
                    PopupMenuButton<String>(
                      icon: const Icon(Icons.more_vert, color: AppTheme.secondaryText, size: 20),
                      color: AppTheme.elevatedCard,
                      padding: EdgeInsets.zero,
                      constraints: const BoxConstraints(),
                      shape: RoundedRectangleBorder(
                        borderRadius: BorderRadius.circular(12),
                        side: const BorderSide(color: AppTheme.border),
                      ),
                      tooltip: 'PC Options',
                      onSelected: (value) {
                        if (value == 'rename') {
                          _showRenameDialog(context);
                        } else if (value == 'remove') {
                          _showRemoveConfirmation(context);
                        }
                      },
                      itemBuilder: (context) => [
                        const PopupMenuItem(
                          value: 'rename',
                          child: Row(
                            children: [
                              Icon(Icons.edit_outlined, size: 18, color: Colors.white),
                              SizedBox(width: 10),
                              Text('Rename PC', style: TextStyle(color: Colors.white)),
                            ],
                          ),
                        ),
                        const PopupMenuItem(
                          value: 'remove',
                          child: Row(
                            children: [
                              Icon(Icons.delete_outline, size: 18, color: AppTheme.error),
                              SizedBox(width: 10),
                              Text('Remove PC', style: TextStyle(color: AppTheme.error)),
                            ],
                          ),
                        ),
                      ],
                    ),
                  ],
                );
              },
            ),
            const SizedBox(height: 14),

            // Full-Width Primary Action: Connect Monitor
            Container(
              height: 44,
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  colors: [AppTheme.primaryBlue, Color(0xFF1D4ED8)],
                  begin: Alignment.topLeft,
                  end: Alignment.bottomRight,
                ),
                borderRadius: BorderRadius.circular(12),
                boxShadow: [
                  BoxShadow(
                    color: AppTheme.primaryBlue.withValues(alpha: 0.3),
                    blurRadius: 8,
                    offset: const Offset(0, 2),
                  ),
                ],
              ),
              child: Material(
                color: Colors.transparent,
                child: InkWell(
                  borderRadius: BorderRadius.circular(12),
                  onTap: onConnect,
                  child: const Center(
                    child: Row(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Icon(Icons.play_arrow, color: Colors.white, size: 20),
                        SizedBox(width: 8),
                        Text(
                          'Connect Monitor',
                          style: TextStyle(
                            color: Colors.white,
                            fontSize: 15,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
