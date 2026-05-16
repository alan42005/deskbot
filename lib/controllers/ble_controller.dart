import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// UUID for the custom BLE service on the ESP32
const String kServiceUUID = '12345678-1234-1234-1234-123456789abc';
const String kAnimationCharUUID = '12345678-1234-1234-1234-123456789001';
const String kTextCharUUID = '12345678-1234-1234-1234-123456789002';
const String kStatusCharUUID = '12345678-1234-1234-1234-123456789003';

enum BotConnectionState { disconnected, scanning, connecting, connected }

class BleController extends ChangeNotifier {
  BluetoothDevice? _device;
  BluetoothCharacteristic? _animChar;
  BluetoothCharacteristic? _textChar;
  BluetoothCharacteristic? _statusChar;

  BotConnectionState _connectionState = BotConnectionState.disconnected;
  List<BluetoothDevice> _foundDevices = [];
  int _currentAnimation = 0;
  String _statusText = '';
  String _errorMessage = '';
  StreamSubscription? _scanSubscription;
  StreamSubscription? _deviceStateSubscription;

  BotConnectionState get connectionState => _connectionState;
  List<BluetoothDevice> get foundDevices => _foundDevices;
  int get currentAnimation => _currentAnimation;
  String get statusText => _statusText;
  String get errorMessage => _errorMessage;
  bool get isConnected => _connectionState == BotConnectionState.connected;

  // --- Scanning ---
  Future<void> startScan() async {
    _foundDevices.clear();
    _errorMessage = '';
    _connectionState = BotConnectionState.scanning;
    notifyListeners();

    _scanSubscription = FlutterBluePlus.scanResults.listen((results) {
      for (final r in results) {
        if (!_foundDevices.any((d) => d.remoteId == r.device.remoteId)) {
          if (r.device.platformName.isNotEmpty) {
            _foundDevices.add(r.device);
            notifyListeners();
          }
        }
      }
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 8));
    await Future.delayed(const Duration(seconds: 8));
    await _scanSubscription?.cancel();

    if (_connectionState == BotConnectionState.scanning) {
      _connectionState = BotConnectionState.disconnected;
      notifyListeners();
    }
  }

  void stopScan() {
    FlutterBluePlus.stopScan();
    _scanSubscription?.cancel();
    _connectionState = BotConnectionState.disconnected;
    notifyListeners();
  }

  // --- Connecting ---
  Future<void> connect(BluetoothDevice device) async {
    _errorMessage = '';
    _connectionState = BotConnectionState.connecting;
    notifyListeners();

    try {
      await device.connect(timeout: const Duration(seconds: 10));
      _device = device;

      _deviceStateSubscription = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _handleDisconnect();
        }
      });

      final services = await device.discoverServices();
      for (final service in services) {
        if (service.serviceUuid.toString().toLowerCase() ==
            kServiceUUID.toLowerCase()) {
          for (final char in service.characteristics) {
            final uuid = char.characteristicUuid.toString().toLowerCase();
            if (uuid == kAnimationCharUUID.toLowerCase()) _animChar = char;
            if (uuid == kTextCharUUID.toLowerCase()) _textChar = char;
            if (uuid == kStatusCharUUID.toLowerCase()) _statusChar = char;
          }
        }
      }

      _connectionState = BotConnectionState.connected;
      notifyListeners();
    } catch (e) {
      _errorMessage = 'Connection failed: $e';
      _connectionState = BotConnectionState.disconnected;
      notifyListeners();
    }
  }

  void _handleDisconnect() {
    _device = null;
    _animChar = null;
    _textChar = null;
    _statusChar = null;
    _connectionState = BotConnectionState.disconnected;
    notifyListeners();
  }

  Future<void> disconnect() async {
    await _device?.disconnect();
    _handleDisconnect();
  }

  // --- Commands ---
  Future<bool> sendAnimation(int index) async {
    if (_animChar == null) return _mockSend(index);
    try {
      await _animChar!.write([index], withoutResponse: true);
      _currentAnimation = index;
      notifyListeners();
      return true;
    } catch (e) {
      _errorMessage = 'Send failed: $e';
      notifyListeners();
      return false;
    }
  }

  Future<bool> sendText(String text) async {
    if (_textChar == null) return false;
    try {
      await _textChar!.write(text.codeUnits, withoutResponse: true);
      return true;
    } catch (e) {
      _errorMessage = 'Text send failed: $e';
      notifyListeners();
      return false;
    }
  }

  // --- Software Features ---
  Future<bool> startPomodoro(int minutes) async {
    return sendText("CMD:POMO:$minutes");
  }

  Future<bool> setAlarm(int hour, int minute) async {
    final hh = hour.toString().padLeft(2, '0');
    final mm = minute.toString().padLeft(2, '0');
    return sendText("CMD:ALRM:$hh:$mm");
  }

  Future<bool> sendNotificationAlert() async {
    return sendText("CMD:NOTI:1");
  }

  /// Demo mode: works without a real device connected
  bool _mockSend(int index) {
    _currentAnimation = index;
    notifyListeners();
    return true;
  }

  @override
  void dispose() {
    _scanSubscription?.cancel();
    _deviceStateSubscription?.cancel();
    super.dispose();
  }
}
