import 'dart:convert';
import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../models/telemetry.dart';

class BrokerSettings {
  String host, user, pass;
  int port;
  bool tls;
  BrokerSettings({this.host = '', this.port = 8883, this.user = '', this.pass = '', this.tls = true});

  static Future<BrokerSettings> load() async {
    final p = await SharedPreferences.getInstance();
    return BrokerSettings(
      host: p.getString('host') ?? '', port: p.getInt('port') ?? 8883,
      user: p.getString('user') ?? '', pass: p.getString('pass') ?? '',
      tls: p.getBool('tls') ?? true,
    );
  }

  Future<void> save() async {
    final p = await SharedPreferences.getInstance();
    await p.setString('host', host); await p.setInt('port', port);
    await p.setString('user', user); await p.setString('pass', pass);
    await p.setBool('tls', tls);
  }
}

class AlertEvent {
  final String name; final String level; final double? temp; final DateTime at;
  AlertEvent(this.name, this.level, this.temp) : at = DateTime.now();
}

class MqttService extends ChangeNotifier {
  MqttServerClient? _client;
  final Map<String, Telemetry> latest = {};
  final Map<String, List<Telemetry>> history = {};
  final Map<String, bool> online = {};
  final List<AlertEvent> alerts = [];
  String status = 'Disconnected';
  String? lastError;
  static const _maxHistory = 600; // ~20 min at 2 s

  bool get connected => _client?.connectionStatus?.state == MqttConnectionState.connected;

  Future<void> connect(BrokerSettings s) async {
    disconnect();
    if (s.host.isEmpty) { lastError = 'Set the broker host in Settings.'; notifyListeners(); return; }
    final id = 'fg-app-${DateTime.now().millisecondsSinceEpoch}';
    final c = MqttServerClient.withPort(s.host, id, s.port)
      ..secure = s.tls
      ..keepAlivePeriod = 30
      ..autoReconnect = true
      ..logging(on: false)
      ..onConnected = () { status = 'Connected'; notifyListeners(); }
      ..onDisconnected = () { status = 'Disconnected'; notifyListeners(); }
      ..onAutoReconnect = () { status = 'Reconnecting…'; notifyListeners(); }
      ..onAutoReconnected = () { status = 'Connected'; notifyListeners(); };
    c.connectionMessage = MqttConnectMessage().withClientIdentifier(id).startClean();
    _client = c;
    status = 'Connecting…'; lastError = null; notifyListeners();
    try {
      await c.connect(s.user.isEmpty ? null : s.user, s.user.isEmpty ? null : s.pass);
    } catch (e) {
      lastError = e.toString(); c.disconnect(); status = 'Error'; notifyListeners(); return;
    }
    if (!connected) { lastError = 'Connection refused'; status = 'Error'; notifyListeners(); return; }
    c.subscribe('fieldguard/+/telemetry', MqttQos.atMostOnce);
    c.subscribe('fieldguard/+/status', MqttQos.atLeastOnce);
    c.updates!.listen(_onMessages);
  }

  void disconnect() { _client?.disconnect(); _client = null; status = 'Disconnected'; }

  void setDemo(String dev, bool on) {
    if (!connected) return;
    final b = MqttClientPayloadBuilder()..addString(jsonEncode({'demo': on}));
    _client!.publishMessage('fieldguard/$dev/cmd', MqttQos.atLeastOnce, b.payload!);
  }

  void _onMessages(List<MqttReceivedMessage<MqttMessage>> events) {
    for (final ev in events) {
      try {
        final msg = ev.payload as MqttPublishMessage;
        final text = MqttPublishPayload.bytesToStringAsString(msg.payload.message);
        final parts = ev.topic.split('/');
        if (parts.length < 3) continue;
        final dev = parts[1];
        final json = jsonDecode(text) as Map<String, dynamic>;
        if (parts[2] == 'status') {
          online[dev] = json['online'] == true;
        } else if (parts[2] == 'telemetry') {
          final t = Telemetry.fromJson(json);
          final prev = latest[dev]?.risk ?? 0;
          latest[dev] = t;
          final h = history.putIfAbsent(dev, () => []);
          h.add(t);
          if (h.length > _maxHistory) h.removeAt(0);
          online[dev] = true;
          if ((t.risk ?? 0) == 2 && prev < 2) {
            alerts.insert(0, AlertEvent(t.name, t.riskLabel, t.temp));
            HapticFeedback.heavyImpact();
          }
        }
      } catch (_) {/* ignore malformed packet */}
    }
    notifyListeners();
  }

  @override
  void dispose() { disconnect(); super.dispose(); }
}
