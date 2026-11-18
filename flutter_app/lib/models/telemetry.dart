import 'package:flutter/material.dart';

class Telemetry {
  final String dev, name;
  final double? temp, pressHpa, rms;
  final int? light, act, risk, score, dact, drest, rest60, rssi;
  final bool demo, err, model;
  final DateTime received;

  Telemetry({
    required this.dev, required this.name, this.temp, this.pressHpa, this.rms,
    this.light, this.act, this.risk, this.score, this.dact, this.drest,
    this.rest60, this.rssi, this.demo = false, this.err = false,
    this.model = false, DateTime? received,
  }) : received = received ?? DateTime.now();

  factory Telemetry.fromJson(Map<String, dynamic> j) {
    double? d(String k) => (j[k] as num?)?.toDouble();
    int? i(String k) => (j[k] as num?)?.toInt();
    return Telemetry(
      dev: j['dev'] as String? ?? 'unknown',
      name: j['name'] as String? ?? 'Worker',
      temp: d('T'), pressHpa: d('P'), rms: d('rms'),
      light: i('light'), act: i('act'), risk: i('risk'), score: i('score'),
      dact: i('dact'), drest: i('drest'), rest60: i('rest60'), rssi: i('rssi'),
      demo: j['demo'] == true, err: j['err'] == true, model: j['model'] == true,
    );
  }

  String get riskLabel => err ? 'Sensor Error' : const ['Normal', 'Caution', 'Warning'][(risk ?? 0).clamp(0, 2)];
  String get actLabel => const ['Rest', 'Light', 'Moderate/Vigorous'][(act ?? 0).clamp(0, 2)];
  String get lightLabel => const ['Shade / indoor', 'Bright', 'Strong sun-like'][(light ?? 0).clamp(0, 2)];

  Color get color {
    if (err || risk == null) return Colors.grey;
    return [Colors.green.shade700, Colors.orange.shade700, Colors.red.shade700][risk!.clamp(0, 2)];
  }

  static String minutes(int? s) => s == null ? '--' : '${s ~/ 60} min';
}
