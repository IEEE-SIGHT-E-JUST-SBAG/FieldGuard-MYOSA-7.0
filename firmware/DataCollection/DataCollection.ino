// FieldGuard data collection for Edge Impulse.
// Streams "ax,ay,az" (in g) at 50 Hz over USB serial. On your PC run:
//   edge-impulse-data-forwarder --frequency 50
// Name the axes ax,ay,az and label each recording: rest / light / vigorous.
// IMPORTANT: the firmware classifies windows of ax,ay,az in g -> collect in g too.
#include <Wire.h>
#define I2C_SDA 21
#define I2C_SCL 22
#define MPU_ADDR 0x68
const uint32_t PERIOD_US = 20000;   // 50 Hz
uint32_t last = 0;

static void wr(uint8_t r, uint8_t v) { Wire.beginTransmission(MPU_ADDR); Wire.write(r); Wire.write(v); Wire.endTransmission(); }

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL); Wire.setClock(400000);
  wr(0x6B, 0x01); delay(10);
  wr(0x1A, 0x04);   // DLPF ~21 Hz (same as deployment firmware)
  wr(0x1C, 0x08);   // +-4 g
  last = micros();
}

void loop() {
  if ((uint32_t)(micros() - last) < PERIOD_US) return;
  last += PERIOD_US;
  Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (uint8_t)true) != 6) return;
  int16_t v[3];
  for (int i = 0; i < 3; i++) { int16_t hi = Wire.read(); v[i] = (hi << 8) | Wire.read(); }
  Serial.print(v[0] / 8192.0f, 4); Serial.print(',');
  Serial.print(v[1] / 8192.0f, 4); Serial.print(',');
  Serial.println(v[2] / 8192.0f, 4);
}
