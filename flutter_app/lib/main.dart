import 'package:fl_chart/fl_chart.dart';
import 'package:flutter/material.dart';
import 'models/telemetry.dart';
import 'services/mqtt_service.dart';

void main() => runApp(const FieldGuardApp());

class FieldGuardApp extends StatelessWidget {
  const FieldGuardApp({super.key});
  @override
  Widget build(BuildContext context) => MaterialApp(
        title: 'FieldGuard',
        theme: ThemeData(colorSchemeSeed: Colors.green, useMaterial3: true),
        home: const HomePage(),
      );
}

class HomePage extends StatefulWidget {
  const HomePage({super.key});
  @override
  State<HomePage> createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> {
  final svc = MqttService();

  @override
  void initState() {
    super.initState();
    BrokerSettings.load().then((s) { if (s.host.isNotEmpty) svc.connect(s); });
  }

  @override
  void dispose() { svc.dispose(); super.dispose(); }

  Future<void> _settings() async {
    final s = await BrokerSettings.load();
    final host = TextEditingController(text: s.host);
    final port = TextEditingController(text: '${s.port}');
    final user = TextEditingController(text: s.user);
    final pass = TextEditingController(text: s.pass);
    var tls = s.tls;
    if (!mounted) return;
    final ok = await showDialog<bool>(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, set) => AlertDialog(
          title: const Text('MQTT broker'),
          content: SingleChildScrollView(
            child: Column(mainAxisSize: MainAxisSize.min, children: [
              TextField(controller: host, decoration: const InputDecoration(labelText: 'Host')),
              TextField(controller: port, keyboardType: TextInputType.number, decoration: const InputDecoration(labelText: 'Port')),
              TextField(controller: user, decoration: const InputDecoration(labelText: 'Username')),
              TextField(controller: pass, obscureText: true, decoration: const InputDecoration(labelText: 'Password')),
              SwitchListTile(title: const Text('TLS'), value: tls, onChanged: (v) => set(() => tls = v)),
            ]),
          ),
          actions: [
            TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Cancel')),
            FilledButton(onPressed: () => Navigator.pop(ctx, true), child: const Text('Connect')),
          ],
        ),
      ),
    );
    if (ok == true) {
      final n = BrokerSettings(host: host.text.trim(), port: int.tryParse(port.text) ?? 8883,
          user: user.text.trim(), pass: pass.text, tls: tls);
      await n.save();
      svc.connect(n);
    }
  }

  @override
  Widget build(BuildContext context) => ListenableBuilder(
        listenable: svc,
        builder: (context, _) {
          final devs = svc.latest.values.toList()
            ..sort((a, b) => (b.risk ?? -1).compareTo(a.risk ?? -1)); // worst first
          return Scaffold(
            appBar: AppBar(
              title: const Text('FieldGuard'),
              actions: [
                Chip(label: Text(svc.status), avatar: Icon(Icons.circle, size: 12, color: svc.connected ? Colors.green : Colors.grey)),
                IconButton(icon: const Icon(Icons.settings), onPressed: _settings),
              ],
            ),
            body: Column(children: [
              if (svc.lastError != null)
                MaterialBanner(content: Text(svc.lastError!), actions: [TextButton(onPressed: _settings, child: const Text('Settings'))]),
              if (svc.alerts.isNotEmpty)
                Container(
                  width: double.infinity, color: Colors.red.shade50, padding: const EdgeInsets.all(8),
                  child: Text('⚠ ${svc.alerts.first.name}: ${svc.alerts.first.level}'
                      '${svc.alerts.first.temp != null ? " at ${svc.alerts.first.temp!.toStringAsFixed(1)} °C" : ""}',
                      style: TextStyle(color: Colors.red.shade900, fontWeight: FontWeight.bold)),
                ),
              Expanded(
                child: devs.isEmpty
                    ? const Center(child: Text('Waiting for devices…\nTopic: fieldguard/+/telemetry', textAlign: TextAlign.center))
                    : ListView(children: [for (final t in devs) _card(t)]),
              ),
              const Padding(
                padding: EdgeInsets.all(6),
                child: Text('Risk indicators only - not a diagnosis, not WBGT.', style: TextStyle(fontSize: 11, color: Colors.grey)),
              ),
            ]),
          );
        },
      );

  Widget _card(Telemetry t) {
    final on = svc.online[t.dev] ?? true;
    return Card(
      margin: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
      child: ListTile(
        onTap: () => Navigator.push(context, MaterialPageRoute(builder: (_) => DetailPage(svc: svc, dev: t.dev))),
        leading: CircleAvatar(backgroundColor: on ? t.color : Colors.grey, child: const Icon(Icons.person, color: Colors.white)),
        title: Text('${t.name}  ·  ${on ? t.riskLabel : "Offline"}${t.demo ? " (demo)" : ""}'),
        subtitle: Text(t.err
            ? 'Sensor error'
            : '${t.temp?.toStringAsFixed(1)} °C · ${t.actLabel} · active ${Telemetry.minutes(t.dact)}'),
        trailing: const Icon(Icons.chevron_right),
      ),
    );
  }
}

class DetailPage extends StatelessWidget {
  final MqttService svc; final String dev;
  const DetailPage({super.key, required this.svc, required this.dev});

  @override
  Widget build(BuildContext context) => ListenableBuilder(
        listenable: svc,
        builder: (context, _) {
          final t = svc.latest[dev];
          if (t == null) return const Scaffold(body: Center(child: Text('No data')));
          final h = svc.history[dev] ?? [];
          return Scaffold(
            appBar: AppBar(title: Text(t.name)),
            body: ListView(padding: const EdgeInsets.all(12), children: [
              Container(
                padding: const EdgeInsets.all(20),
                decoration: BoxDecoration(color: t.color, borderRadius: BorderRadius.circular(14)),
                child: Text(t.riskLabel.toUpperCase(), textAlign: TextAlign.center,
                    style: const TextStyle(color: Colors.white, fontSize: 32, fontWeight: FontWeight.bold)),
              ),
              const SizedBox(height: 8),
              GridView.count(
                crossAxisCount: 2, shrinkWrap: true, childAspectRatio: 2.4, mainAxisSpacing: 6, crossAxisSpacing: 6,
                physics: const NeverScrollableScrollPhysics(),
                children: [
                  _m('Temperature', t.temp == null ? '--' : '${t.temp!.toStringAsFixed(1)} °C'),
                  _m('Pressure', t.pressHpa == null ? '--' : '${t.pressHpa!.toStringAsFixed(0)} hPa'),
                  _m('Activity', t.actLabel), _m('Light', t.lightLabel),
                  _m('Activity time', Telemetry.minutes(t.dact)), _m('Rest time', Telemetry.minutes(t.drest)),
                  _m('Rest (last 60 min)', Telemetry.minutes(t.rest60)), _m('Score', '${t.score ?? "--"} / 8'),
                ],
              ),
              const SizedBox(height: 10),
              const Text('Temperature (°C)'),
              SizedBox(height: 140, child: _chart(h.map((e) => e.temp).toList(), Colors.orange)),
              const SizedBox(height: 10),
              const Text('Movement RMS (g)'),
              SizedBox(height: 140, child: _chart(h.map((e) => e.rms).toList(), Colors.blue)),
              SwitchListTile(
                title: const Text('Demo mode (simulated heat + fast timers)'),
                subtitle: const Text('Sends a command to the device. Clearly marked as simulation.'),
                value: t.demo, onChanged: (v) => svc.setDemo(dev, v),
              ),
              Text('Device $dev · RSSI ${t.rssi ?? "--"} dBm · ${t.model ? "TinyML" : "threshold"} classifier',
                  style: const TextStyle(fontSize: 11, color: Colors.grey)),
            ]),
          );
        },
      );

  Widget _m(String k, String v) => Card(
        child: Padding(
          padding: const EdgeInsets.all(8),
          child: Column(crossAxisAlignment: CrossAxisAlignment.start, mainAxisAlignment: MainAxisAlignment.center,
              children: [Text(k, style: const TextStyle(fontSize: 11, color: Colors.grey)), Text(v, style: const TextStyle(fontWeight: FontWeight.bold))]),
        ),
      );

  Widget _chart(List<double?> ys, Color c) {
    final spots = <FlSpot>[for (var i = 0; i < ys.length; i++) if (ys[i] != null) FlSpot(i.toDouble(), ys[i]!)];
    if (spots.length < 2) return const Center(child: Text('collecting…'));
    return LineChart(LineChartData(
      lineBarsData: [LineChartBarData(spots: spots, color: c, dotData: const FlDotData(show: false), barWidth: 2)],
      titlesData: const FlTitlesData(
        topTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
        rightTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
        bottomTitles: AxisTitles(sideTitles: SideTitles(showTitles: false)),
      ),
      gridData: const FlGridData(show: true),
    ));
  }
}
