import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../controllers/ble_controller.dart';
import 'mood_pad_screen.dart';
import 'connect_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen>
    with TickerProviderStateMixin {
  late final BleController _ble;
  late final AnimationController _pulseCtrl;
  late final Animation<double> _pulse;
  int _selectedNav = 0;

  @override
  void initState() {
    super.initState();
    _ble = BleController();
    _pulseCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat(reverse: true);
    _pulse = Tween<double>(begin: 0.95, end: 1.05).animate(
      CurvedAnimation(parent: _pulseCtrl, curve: Curves.easeInOut),
    );
  }

  @override
  void dispose() {
    _pulseCtrl.dispose();
    _ble.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: _ble,
      builder: (context, _) {
        return Scaffold(
          backgroundColor: const Color(0xFF0A0A0F),
          body: _buildBody(),
          bottomNavigationBar: _buildBottomNav(),
        );
      },
    );
  }

  Widget _buildBody() {
    return switch (_selectedNav) {
      0 => _DashboardTab(ble: _ble, pulse: _pulse),
      1 => MoodPadScreen(ble: _ble),
      2 => ConnectScreen(ble: _ble),
      _ => _DashboardTab(ble: _ble, pulse: _pulse),
    };
  }

  Widget _buildBottomNav() {
    final items = [
      (Icons.dashboard_rounded, 'Dashboard'),
      (Icons.mood_rounded, 'Moods'),
      (Icons.bluetooth_rounded, 'Connect'),
    ];

    return Container(
      decoration: BoxDecoration(
        color: const Color(0xFF12121A),
        border: Border(top: BorderSide(color: Colors.white.withOpacity(0.06))),
      ),
      child: SafeArea(
        child: Padding(
          padding: const EdgeInsets.symmetric(vertical: 8),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: List.generate(items.length, (i) {
              final selected = _selectedNav == i;
              return GestureDetector(
                onTap: () => setState(() => _selectedNav = i),
                child: AnimatedContainer(
                  duration: const Duration(milliseconds: 250),
                  padding: const EdgeInsets.symmetric(
                    horizontal: 20, vertical: 10),
                  decoration: BoxDecoration(
                    color: selected
                        ? const Color(0xFF7C3AED).withOpacity(0.18)
                        : Colors.transparent,
                    borderRadius: BorderRadius.circular(16),
                  ),
                  child: Column(
                    mainAxisSize: MainAxisSize.min,
                    children: [
                      Icon(
                        items[i].$1,
                        color: selected
                            ? const Color(0xFF7C3AED)
                            : Colors.white38,
                        size: 24,
                      ),
                      const SizedBox(height: 4),
                      Text(
                        items[i].$2,
                        style: GoogleFonts.outfit(
                          fontSize: 11,
                          color: selected
                              ? const Color(0xFF7C3AED)
                              : Colors.white38,
                          fontWeight: selected
                              ? FontWeight.w600
                              : FontWeight.w400,
                        ),
                      ),
                    ],
                  ),
                ),
              );
            }),
          ),
        ),
      ),
    );
  }
}

// ─── Dashboard Tab ──────────────────────────────────────────────────────────
class _DashboardTab extends StatelessWidget {
  final BleController ble;
  final Animation<double> pulse;

  const _DashboardTab({required this.ble, required this.pulse});

  static const _animations = [
    ('😘', 'Kissing', Color(0xFFFF6B9D)),
    ('👀', 'Looking', Color(0xFF06B6D4)),
    ('😢', 'Crying', Color(0xFF3B82F6)),
    ('😴', 'Sleeping', Color(0xFF8B5CF6)),
    ('😂', 'LaughCry', Color(0xFFF59E0B)),
    ('🥹', 'Cute Blink', Color(0xFFEC4899)),
    ('🥺', 'Pleading', Color(0xFF10B981)),
    ('🧃', 'Drinking', Color(0xFF14B8A6)),
  ];

  @override
  Widget build(BuildContext context) {
    return CustomScrollView(
      physics: const BouncingScrollPhysics(),
      slivers: [
        SliverToBoxAdapter(child: _buildHeader()),
        SliverToBoxAdapter(child: _buildStatusCard()),
        SliverToBoxAdapter(
          child: Padding(
            padding: const EdgeInsets.fromLTRB(20, 8, 20, 4),
            child: Text(
              'QUICK MOODS',
              style: GoogleFonts.outfit(
                fontSize: 11,
                fontWeight: FontWeight.w700,
                color: Colors.white30,
                letterSpacing: 2,
              ),
            ),
          ),
        ),
        SliverPadding(
          padding: const EdgeInsets.fromLTRB(16, 8, 16, 100),
          sliver: SliverGrid(
            delegate: SliverChildBuilderDelegate(
              (context, i) => _AnimCard(
                emoji: _animations[i].$1,
                label: _animations[i].$2,
                color: _animations[i].$3,
                isActive: ble.currentAnimation == i,
                onTap: () => ble.sendAnimation(i),
              ),
              childCount: _animations.length,
            ),
            gridDelegate: const SliverGridDelegateWithFixedCrossAxisCount(
              crossAxisCount: 2,
              mainAxisSpacing: 12,
              crossAxisSpacing: 12,
              childAspectRatio: 1.5,
            ),
          ),
        ),
      ],
    );
  }

  Widget _buildHeader() {
    return Container(
      padding: const EdgeInsets.fromLTRB(24, 60, 24, 20),
      decoration: const BoxDecoration(
        gradient: LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0xFF1A0A2E), Color(0xFF0A0A0F)],
        ),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              ScaleTransition(
                scale: pulse,
                child: Container(
                  width: 48,
                  height: 48,
                  decoration: BoxDecoration(
                    gradient: const LinearGradient(
                      colors: [Color(0xFF7C3AED), Color(0xFF06B6D4)],
                    ),
                    borderRadius: BorderRadius.circular(14),
                    boxShadow: [
                      BoxShadow(
                        color: const Color(0xFF7C3AED).withOpacity(0.5),
                        blurRadius: 16,
                        spreadRadius: 2,
                      ),
                    ],
                  ),
                  child: const Center(
                    child: Text('🤖', style: TextStyle(fontSize: 26)),
                  ),
                ),
              ),
              const SizedBox(width: 14),
              Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    'EMO Bot',
                    style: GoogleFonts.outfit(
                      fontSize: 26,
                      fontWeight: FontWeight.w800,
                      color: Colors.white,
                    ),
                  ),
                  Text(
                    'Desktop Companion',
                    style: GoogleFonts.outfit(
                      fontSize: 13,
                      color: Colors.white38,
                    ),
                  ),
                ],
              ),
            ],
          ),
        ],
      ),
    );
  }

  Widget _buildStatusCard() {
    final connected = ble.isConnected;
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 4, 16, 16),
      child: Container(
        padding: const EdgeInsets.all(18),
        decoration: BoxDecoration(
          gradient: LinearGradient(
            colors: connected
                ? [const Color(0xFF064E3B), const Color(0xFF065F46)]
                : [const Color(0xFF1C1C2E), const Color(0xFF16162A)],
          ),
          borderRadius: BorderRadius.circular(20),
          border: Border.all(
            color: connected
                ? const Color(0xFF10B981).withOpacity(0.3)
                : Colors.white.withOpacity(0.06),
          ),
        ),
        child: Row(
          children: [
            Container(
              width: 44,
              height: 44,
              decoration: BoxDecoration(
                color: (connected
                    ? const Color(0xFF10B981)
                    : const Color(0xFF7C3AED)).withOpacity(0.15),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(
                connected
                    ? Icons.sensors_rounded
                    : Icons.sensors_off_rounded,
                color: connected
                    ? const Color(0xFF10B981)
                    : const Color(0xFF7C3AED),
                size: 22,
              ),
            ),
            const SizedBox(width: 14),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    connected ? 'Bot Connected' : 'Not Connected',
                    style: GoogleFonts.outfit(
                      fontWeight: FontWeight.w700,
                      fontSize: 15,
                      color: Colors.white,
                    ),
                  ),
                  Text(
                    connected
                        ? 'Sending commands via BLE'
                        : 'Demo mode active — go to Connect',
                    style: GoogleFonts.outfit(
                      fontSize: 12,
                      color: Colors.white54,
                    ),
                  ),
                ],
              ),
            ),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 5),
              decoration: BoxDecoration(
                color: (connected
                    ? const Color(0xFF10B981)
                    : const Color(0xFF374151)).withOpacity(0.2),
                borderRadius: BorderRadius.circular(20),
              ),
              child: Text(
                connected ? 'LIVE' : 'DEMO',
                style: GoogleFonts.outfit(
                  fontSize: 11,
                  fontWeight: FontWeight.w700,
                  color: connected
                      ? const Color(0xFF10B981)
                      : Colors.white38,
                  letterSpacing: 1,
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}

// ─── Animation Card ──────────────────────────────────────────────────────────
class _AnimCard extends StatefulWidget {
  final String emoji;
  final String label;
  final Color color;
  final bool isActive;
  final VoidCallback onTap;

  const _AnimCard({
    required this.emoji,
    required this.label,
    required this.color,
    required this.isActive,
    required this.onTap,
  });

  @override
  State<_AnimCard> createState() => _AnimCardState();
}

class _AnimCardState extends State<_AnimCard>
    with SingleTickerProviderStateMixin {
  late final AnimationController _ctrl;
  late final Animation<double> _scale;

  @override
  void initState() {
    super.initState();
    _ctrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 150),
      lowerBound: 0.92,
      upperBound: 1.0,
      value: 1.0,
    );
    _scale = _ctrl;
  }

  @override
  void dispose() {
    _ctrl.dispose();
    super.dispose();
  }

  void _onTap() {
    _ctrl.reverse().then((_) => _ctrl.forward());
    widget.onTap();
  }

  @override
  Widget build(BuildContext context) {
    return ScaleTransition(
      scale: _scale,
      child: GestureDetector(
        onTap: _onTap,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 300),
          decoration: BoxDecoration(
            gradient: LinearGradient(
              begin: Alignment.topLeft,
              end: Alignment.bottomRight,
              colors: widget.isActive
                  ? [
                      widget.color.withOpacity(0.3),
                      widget.color.withOpacity(0.12),
                    ]
                  : [
                      const Color(0xFF1A1A28),
                      const Color(0xFF12121E),
                    ],
            ),
            borderRadius: BorderRadius.circular(20),
            border: Border.all(
              color: widget.isActive
                  ? widget.color.withOpacity(0.6)
                  : Colors.white.withOpacity(0.06),
              width: widget.isActive ? 1.5 : 1,
            ),
            boxShadow: widget.isActive
                ? [
                    BoxShadow(
                      color: widget.color.withOpacity(0.25),
                      blurRadius: 20,
                      spreadRadius: 1,
                    )
                  ]
                : [],
          ),
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Row(
                  mainAxisAlignment: MainAxisAlignment.spaceBetween,
                  children: [
                    Text(
                      widget.emoji,
                      style: const TextStyle(fontSize: 32),
                    ),
                    if (widget.isActive)
                      Container(
                        width: 8,
                        height: 8,
                        decoration: BoxDecoration(
                          color: widget.color,
                          shape: BoxShape.circle,
                          boxShadow: [
                            BoxShadow(
                              color: widget.color.withOpacity(0.6),
                              blurRadius: 6,
                              spreadRadius: 1,
                            )
                          ],
                        ),
                      ),
                  ],
                ),
                Text(
                  widget.label,
                  style: GoogleFonts.outfit(
                    fontSize: 14,
                    fontWeight: FontWeight.w600,
                    color: widget.isActive ? Colors.white : Colors.white70,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
