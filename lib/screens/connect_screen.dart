import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:google_fonts/google_fonts.dart';
import '../controllers/ble_controller.dart';

class ConnectScreen extends StatefulWidget {
  final BleController ble;
  const ConnectScreen({super.key, required this.ble});

  @override
  State<ConnectScreen> createState() => _ConnectScreenState();
}

class _ConnectScreenState extends State<ConnectScreen>
    with SingleTickerProviderStateMixin {
  late final AnimationController _radarCtrl;

  @override
  void initState() {
    super.initState();
    _radarCtrl = AnimationController(
      vsync: this,
      duration: const Duration(seconds: 2),
    )..repeat();
  }

  @override
  void dispose() {
    _radarCtrl.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.ble,
      builder: (context, _) {
        final state = widget.ble.connectionState;
        return CustomScrollView(
          physics: const BouncingScrollPhysics(),
          slivers: [
            SliverToBoxAdapter(
              child: _buildRadarHeader(state),
            ),
            if (state == BotConnectionState.connected)
              SliverToBoxAdapter(child: _buildConnectedCard()),
            if (state == BotConnectionState.disconnected ||
                state == BotConnectionState.scanning)
              SliverToBoxAdapter(child: _buildScanButton(state)),
            if (widget.ble.foundDevices.isNotEmpty &&
                state != BotConnectionState.connected)
              SliverToBoxAdapter(
                child: Padding(
                  padding: const EdgeInsets.fromLTRB(16, 16, 16, 4),
                  child: Text(
                    'NEARBY DEVICES',
                    style: GoogleFonts.outfit(
                      fontSize: 11,
                      color: Colors.white30,
                      fontWeight: FontWeight.w700,
                      letterSpacing: 2,
                    ),
                  ),
                ),
              ),
            SliverList(
              delegate: SliverChildBuilderDelegate(
                (context, i) => _DeviceTile(
                  device: widget.ble.foundDevices[i],
                  onConnect: () =>
                      widget.ble.connect(widget.ble.foundDevices[i]),
                ),
                childCount: state == BotConnectionState.connected
                    ? 0
                    : widget.ble.foundDevices.length,
              ),
            ),
            if (widget.ble.errorMessage.isNotEmpty)
              SliverToBoxAdapter(child: _buildError()),
            const SliverToBoxAdapter(child: SizedBox(height: 80)),
          ],
        );
      },
    );
  }

  Widget _buildRadarHeader(BotConnectionState state) {
    final isScanning = state == BotConnectionState.scanning;
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 56, 16, 0),
      padding: const EdgeInsets.all(32),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
          colors: [Color(0xFF0D0D1F), Color(0xFF0A0A0F)],
        ),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: Colors.white.withOpacity(0.06)),
      ),
      child: Column(
        children: [
          Text(
            'Bluetooth Scanner',
            style: GoogleFonts.outfit(
              fontSize: 13,
              color: Colors.white30,
              fontWeight: FontWeight.w600,
              letterSpacing: 2,
            ),
          ),
          const SizedBox(height: 32),
          // Radar animation
          SizedBox(
            width: 140,
            height: 140,
            child: Stack(
              alignment: Alignment.center,
              children: [
                // Rings
                for (final r in [70.0, 50.0, 30.0])
                  Container(
                    width: r * 2,
                    height: r * 2,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      border: Border.all(
                        color: Colors.white.withOpacity(0.06),
                      ),
                    ),
                  ),
                // Sweep
                if (isScanning)
                  RotationTransition(
                    turns: _radarCtrl,
                    child: CustomPaint(
                      size: const Size(140, 140),
                      painter: _RadarSweepPainter(),
                    ),
                  ),
                // Center dot
                Container(
                  width: 16,
                  height: 16,
                  decoration: BoxDecoration(
                    color: isScanning
                        ? const Color(0xFF7C3AED)
                        : Colors.white12,
                    shape: BoxShape.circle,
                    boxShadow: isScanning
                        ? [
                            BoxShadow(
                              color:
                                  const Color(0xFF7C3AED).withOpacity(0.6),
                              blurRadius: 12,
                              spreadRadius: 3,
                            )
                          ]
                        : [],
                  ),
                ),
                // Blinking dots for found devices
                if (isScanning)
                  for (var i = 0;
                      i < widget.ble.foundDevices.length && i < 5;
                      i++)
                    Positioned(
                      left: 40.0 + (i * 12),
                      top: 30.0 + (i % 3) * 20,
                      child: Container(
                        width: 8,
                        height: 8,
                        decoration: const BoxDecoration(
                          color: Color(0xFF06B6D4),
                          shape: BoxShape.circle,
                        ),
                      ),
                    ),
              ],
            ),
          ),
          const SizedBox(height: 24),
          Text(
            isScanning
                ? 'Scanning for EMO Bot...'
                : state == BotConnectionState.connected
                    ? '✓ Connected to EMO Bot'
                    : state == BotConnectionState.connecting
                        ? 'Connecting...'
                        : 'Tap Scan to find your bot',
            style: GoogleFonts.outfit(
              fontSize: 16,
              fontWeight: FontWeight.w600,
              color: Colors.white70,
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildScanButton(BotConnectionState state) {
    final isScanning = state == BotConnectionState.scanning;
    return Padding(
      padding: const EdgeInsets.all(16),
      child: GestureDetector(
        onTap: isScanning ? widget.ble.stopScan : widget.ble.startScan,
        child: AnimatedContainer(
          duration: const Duration(milliseconds: 300),
          padding: const EdgeInsets.symmetric(vertical: 18),
          decoration: BoxDecoration(
            gradient: LinearGradient(
              colors: isScanning
                  ? [const Color(0xFF4B1D1D), const Color(0xFF3B1010)]
                  : [
                      const Color(0xFF7C3AED),
                      const Color(0xFF5B21B6),
                    ],
            ),
            borderRadius: BorderRadius.circular(20),
            boxShadow: [
              BoxShadow(
                color: (isScanning
                    ? Colors.red
                    : const Color(0xFF7C3AED)).withOpacity(0.3),
                blurRadius: 20,
                spreadRadius: 2,
              ),
            ],
          ),
          child: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(
                isScanning
                    ? Icons.stop_rounded
                    : Icons.bluetooth_searching_rounded,
                color: Colors.white,
              ),
              const SizedBox(width: 10),
              Text(
                isScanning ? 'Stop Scanning' : 'Scan for EMO Bot',
                style: GoogleFonts.outfit(
                  fontSize: 16,
                  fontWeight: FontWeight.w700,
                  color: Colors.white,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildConnectedCard() {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 16, 16, 0),
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        gradient: const LinearGradient(
          colors: [Color(0xFF064E3B), Color(0xFF065F46)],
        ),
        borderRadius: BorderRadius.circular(20),
        border: Border.all(
          color: const Color(0xFF10B981).withOpacity(0.3),
        ),
      ),
      child: Column(
        children: [
          Row(
            children: [
              const Icon(Icons.check_circle_rounded,
                  color: Color(0xFF10B981), size: 28),
              const SizedBox(width: 12),
              Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      'EMO Bot Connected',
                      style: GoogleFonts.outfit(
                        fontWeight: FontWeight.w700,
                        fontSize: 16,
                        color: Colors.white,
                      ),
                    ),
                    Text(
                      'BLE link active — sending commands in real-time',
                      style:
                          GoogleFonts.outfit(fontSize: 12, color: Colors.white54),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 16),
          GestureDetector(
            onTap: widget.ble.disconnect,
            child: Container(
              padding: const EdgeInsets.symmetric(vertical: 12),
              decoration: BoxDecoration(
                color: Colors.red.withOpacity(0.15),
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: Colors.red.withOpacity(0.25)),
              ),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  const Icon(Icons.bluetooth_disabled_rounded,
                      color: Colors.redAccent, size: 18),
                  const SizedBox(width: 8),
                  Text(
                    'Disconnect',
                    style: GoogleFonts.outfit(
                      fontSize: 14,
                      fontWeight: FontWeight.w600,
                      color: Colors.redAccent,
                    ),
                  ),
                ],
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildError() {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 16, 16, 0),
      padding: const EdgeInsets.all(16),
      decoration: BoxDecoration(
        color: Colors.red.withOpacity(0.1),
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: Colors.red.withOpacity(0.2)),
      ),
      child: Row(
        children: [
          const Icon(Icons.error_outline_rounded,
              color: Colors.redAccent, size: 20),
          const SizedBox(width: 10),
          Expanded(
            child: Text(
              widget.ble.errorMessage,
              style: GoogleFonts.outfit(
                fontSize: 13,
                color: Colors.redAccent,
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _DeviceTile extends StatelessWidget {
  final BluetoothDevice device;
  final VoidCallback onConnect;

  const _DeviceTile({required this.device, required this.onConnect});

  @override
  Widget build(BuildContext context) {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 0, 16, 10),
      decoration: BoxDecoration(
        color: const Color(0xFF12121A),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: Colors.white.withOpacity(0.06)),
      ),
      child: ListTile(
        contentPadding:
            const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
        leading: Container(
          width: 44,
          height: 44,
          decoration: BoxDecoration(
            color: const Color(0xFF7C3AED).withOpacity(0.15),
            borderRadius: BorderRadius.circular(12),
          ),
          child: const Icon(Icons.bluetooth_rounded,
              color: Color(0xFF7C3AED), size: 22),
        ),
        title: Text(
          device.platformName.isEmpty ? 'Unknown Device' : device.platformName,
          style: GoogleFonts.outfit(
            color: Colors.white,
            fontWeight: FontWeight.w600,
            fontSize: 15,
          ),
        ),
        subtitle: Text(
          device.remoteId.str,
          style: GoogleFonts.outfit(fontSize: 11, color: Colors.white38),
        ),
        trailing: GestureDetector(
          onTap: onConnect,
          child: Container(
            padding:
                const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
            decoration: BoxDecoration(
              gradient: const LinearGradient(
                colors: [Color(0xFF7C3AED), Color(0xFF5B21B6)],
              ),
              borderRadius: BorderRadius.circular(12),
            ),
            child: Text(
              'Connect',
              style: GoogleFonts.outfit(
                fontSize: 13,
                fontWeight: FontWeight.w700,
                color: Colors.white,
              ),
            ),
          ),
        ),
      ),
    );
  }
}

class _RadarSweepPainter extends CustomPainter {
  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final radius = size.width / 2;
    final paint = Paint()
      ..shader = SweepGradient(
        colors: [
          const Color(0xFF7C3AED).withOpacity(0),
          const Color(0xFF7C3AED).withOpacity(0.4),
          const Color(0xFF7C3AED).withOpacity(0),
        ],
        stops: const [0.0, 0.8, 1.0],
      ).createShader(Rect.fromCircle(center: center, radius: radius));

    canvas.drawCircle(center, radius, paint);
  }

  @override
  bool shouldRepaint(_RadarSweepPainter old) => false;
}
