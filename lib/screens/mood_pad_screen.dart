import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../controllers/ble_controller.dart';

class MoodPadScreen extends StatefulWidget {
  final BleController ble;
  const MoodPadScreen({super.key, required this.ble});

  @override
  State<MoodPadScreen> createState() => _MoodPadScreenState();
}

class _MoodPadScreenState extends State<MoodPadScreen>
    with TickerProviderStateMixin {
  final TextEditingController _msgCtrl = TextEditingController();
  late final AnimationController _faceCtrl;
  bool _sending = false;

  static const _moods = [
    _Mood(0, '😘', 'Kiss', Color(0xFFFF6B9D), 'Sends a sweet kiss!'),
    _Mood(1, '👀', 'Look', Color(0xFF06B6D4), 'Eyes wander around'),
    _Mood(2, '😢', 'Cry', Color(0xFF3B82F6), 'Sad tears falling'),
    _Mood(3, '😴', 'Sleep', Color(0xFF8B5CF6), 'Snoring ZZZs'),
    _Mood(4, '😂', 'Laugh', Color(0xFFF59E0B), 'Laughing until crying'),
    _Mood(5, '🥹', 'Blink', Color(0xFFEC4899), 'Cute lash blink'),
    _Mood(6, '🥺', 'Plead', Color(0xFF10B981), 'Pleading puppy eyes'),
    _Mood(7, '🧃', 'Drink', Color(0xFF14B8A6), 'Sipping a juice box'),
  ];

  @override
  void initState() {
    super.initState();
    _faceCtrl = AnimationController(
      vsync: this,
      duration: const Duration(milliseconds: 600),
    );
  }

  @override
  void dispose() {
    _faceCtrl.dispose();
    _msgCtrl.dispose();
    super.dispose();
  }

  Future<void> _selectMood(int index) async {
    setState(() => _sending = true);
    _faceCtrl.forward(from: 0);
    await widget.ble.sendAnimation(index);
    await Future.delayed(const Duration(milliseconds: 400));
    setState(() => _sending = false);
  }

  Future<void> _sendMessage() async {
    final text = _msgCtrl.text.trim();
    if (text.isEmpty) return;
    final ok = await widget.ble.sendText(text);
    if (mounted) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(
          content: Text(ok ? 'Message sent to EMO!' : 'Not connected — demo mode'),
          backgroundColor: ok
              ? const Color(0xFF10B981)
              : const Color(0xFF7C3AED),
          behavior: SnackBarBehavior.floating,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
          ),
        ),
      );
    }
    _msgCtrl.clear();
  }

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.ble,
      builder: (context, _) {
        final cur = widget.ble.currentAnimation;
        final mood = _moods[cur];
        return CustomScrollView(
          physics: const BouncingScrollPhysics(),
          slivers: [
            SliverToBoxAdapter(child: _buildFacePreview(mood)),
            SliverToBoxAdapter(child: _buildMoodWheel(cur)),
            SliverToBoxAdapter(child: _buildMessageBox()),
            const SliverToBoxAdapter(child: SizedBox(height: 80)),
          ],
        );
      },
    );
  }

  Widget _buildFacePreview(_Mood mood) {
    return Container(
      margin: const EdgeInsets.fromLTRB(16, 56, 16, 0),
      padding: const EdgeInsets.all(32),
      decoration: BoxDecoration(
        gradient: RadialGradient(
          colors: [
            mood.color.withOpacity(0.12),
            const Color(0xFF0A0A0F),
          ],
          radius: 0.8,
        ),
        borderRadius: BorderRadius.circular(28),
        border: Border.all(color: mood.color.withOpacity(0.2)),
      ),
      child: Column(
        children: [
          Padding(
            padding: const EdgeInsets.only(top: 8),
            child: Text(
              'Mood Pad',
              style: GoogleFonts.outfit(
                fontSize: 13,
                color: Colors.white30,
                fontWeight: FontWeight.w600,
                letterSpacing: 2,
              ),
            ),
          ),
          const SizedBox(height: 16),
          AnimatedSwitcher(
            duration: const Duration(milliseconds: 400),
            transitionBuilder: (child, anim) => ScaleTransition(
              scale: CurvedAnimation(parent: anim, curve: Curves.elasticOut),
              child: child,
            ),
            child: Text(
              mood.emoji,
              key: ValueKey(mood.index),
              style: const TextStyle(fontSize: 80),
            ),
          ),
          const SizedBox(height: 12),
          Text(
            mood.name,
            style: GoogleFonts.outfit(
              fontSize: 24,
              fontWeight: FontWeight.w800,
              color: Colors.white,
            ),
          ),
          const SizedBox(height: 6),
          Text(
            mood.description,
            style: GoogleFonts.outfit(
              fontSize: 13,
              color: mood.color,
            ),
          ),
          const SizedBox(height: 20),
          AnimatedContainer(
            duration: const Duration(milliseconds: 300),
            height: 4,
            width: _sending ? 120.0 : 0.0,
            decoration: BoxDecoration(
              color: mood.color,
              borderRadius: BorderRadius.circular(2),
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildMoodWheel(int current) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 20, 16, 0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'SELECT MOOD',
            style: GoogleFonts.outfit(
              fontSize: 11,
              color: Colors.white30,
              fontWeight: FontWeight.w700,
              letterSpacing: 2,
            ),
          ),
          const SizedBox(height: 12),
          SizedBox(
            height: 100,
            child: ListView.separated(
              scrollDirection: Axis.horizontal,
              physics: const BouncingScrollPhysics(),
              itemCount: _moods.length,
              separatorBuilder: (_, __) => const SizedBox(width: 10),
              itemBuilder: (context, i) {
                final m = _moods[i];
                final sel = current == m.index;
                return GestureDetector(
                  onTap: () => _selectMood(m.index),
                  child: AnimatedContainer(
                    duration: const Duration(milliseconds: 250),
                    width: 80,
                    decoration: BoxDecoration(
                      gradient: LinearGradient(
                        begin: Alignment.topCenter,
                        end: Alignment.bottomCenter,
                        colors: sel
                            ? [
                                m.color.withOpacity(0.4),
                                m.color.withOpacity(0.15),
                              ]
                            : [
                                const Color(0xFF1A1A28),
                                const Color(0xFF12121E),
                              ],
                      ),
                      borderRadius: BorderRadius.circular(20),
                      border: Border.all(
                        color: sel
                            ? m.color.withOpacity(0.7)
                            : Colors.white.withOpacity(0.06),
                        width: sel ? 1.5 : 1,
                      ),
                    ),
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(m.emoji,
                            style: TextStyle(
                              fontSize: sel ? 34 : 28,
                            )),
                        const SizedBox(height: 4),
                        Text(
                          m.name,
                          style: GoogleFonts.outfit(
                            fontSize: 11,
                            color:
                                sel ? Colors.white : Colors.white38,
                            fontWeight: sel
                                ? FontWeight.w700
                                : FontWeight.w400,
                          ),
                        ),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
        ],
      ),
    );
  }

  Widget _buildMessageBox() {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 24, 16, 0),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            'SEND MESSAGE TO EMO',
            style: GoogleFonts.outfit(
              fontSize: 11,
              color: Colors.white30,
              fontWeight: FontWeight.w700,
              letterSpacing: 2,
            ),
          ),
          const SizedBox(height: 12),
          Container(
            decoration: BoxDecoration(
              color: const Color(0xFF12121A),
              borderRadius: BorderRadius.circular(20),
              border: Border.all(color: Colors.white.withOpacity(0.08)),
            ),
            child: TextField(
              controller: _msgCtrl,
              maxLength: 30,
              style: GoogleFonts.outfit(color: Colors.white),
              decoration: InputDecoration(
                hintText: 'Type a message to display...',
                hintStyle: GoogleFonts.outfit(color: Colors.white24),
                counterStyle: GoogleFonts.outfit(color: Colors.white24),
                border: InputBorder.none,
                contentPadding: const EdgeInsets.symmetric(
                  horizontal: 20, vertical: 16),
                suffixIcon: Padding(
                  padding: const EdgeInsets.only(right: 8),
                  child: TextButton.icon(
                    onPressed: _sendMessage,
                    icon: const Icon(Icons.send_rounded, size: 16),
                    label: Text(
                      'Send',
                      style: GoogleFonts.outfit(fontWeight: FontWeight.w600),
                    ),
                    style: TextButton.styleFrom(
                      foregroundColor: const Color(0xFF7C3AED),
                    ),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _Mood {
  final int index;
  final String emoji;
  final String name;
  final Color color;
  final String description;

  const _Mood(
      this.index, this.emoji, this.name, this.color, this.description);
}
