#pragma once
// ============================================================================
// FieldGuard - central configuration.
// Every number marked PROVISIONAL must be replaced by the calibrated value
// (see README "Calibration"), exactly as stated in the paper.
// ============================================================================
#define FW_VERSION "1.0.0"
#define WORKER_NAME "Worker 1"

// ---- MYOSA ESP32 motherboard pins / addresses ------------------------------
#define I2C_SDA 21
#define I2C_SCL 22
#define I2C_CLOCK 400000          // drop to 100000 if the wrist-cable test drops out
#define BUZZER_PIN 25             // !! VERIFY against your MYOSA motherboard docs
#define OLED_ADDR 0x3C
#define MPU_ADDR 0x68

// ---- Sampling / windowing ---------------------------------------------------
#define IMU_HZ 50
#define IMU_PERIOD_US (1000000UL / IMU_HZ)
#define WINDOW_N 100              // 2 s window
#define WINDOW_STRIDE 50          // 50 % overlap -> 1 window per second
#define MA_N 50                   // 1 s moving average for gravity removal
#define ENV_SMOOTH_N 5

// ---- Activity classification (PROVISIONAL) ----------------------------------
#define RMS_REST_G 0.05f
#define RMS_LIGHT_G 0.15f
#define ACTIVITY_CONFIRM_WINDOWS 3
// #define USE_EDGE_IMPULSE        // uncomment after adding the exported library
#define EI_MIN_CONFIDENCE 0.60f

// ---- Timers (design choices, paper Sec. 5) ----------------------------------
#define REST_RECOGNIZED_S 120     // 2 min continuous rest
#define REST_RESET_S 600          // 10 min rest resets activity timer
#define DACT_MED_S 1800           // 30 min
#define DACT_HIGH_S 3600          // 60 min
#define REST_MIN_S 600            // >= 10 min rest per last 60 min
#define LEVEL_CONFIRM_CYCLES 3

// ---- Risk thresholds (PROVISIONAL, dry-air temperature only) ----------------
#define TEMP_CAUTION_C 30.0f
#define TEMP_HIGH_C 35.0f
#define TEMP_CAL_OFFSET_C 0.0f    // fitted from reference-thermometer test
#define TEMP_CAL_SLOPE 1.0f

// ---- APDS9960 clear channel (gain 1x, 10 ms => saturates near 4095) ---------
#define LIGHT_BRIGHT_CLEAR 800    // PROVISIONAL
#define LIGHT_STRONG_CLEAR 2500   // PROVISIONAL (saturation also counts as strong)

// ---- Plausibility limits for validation ------------------------------------
#define TEMP_MIN_C -20.0f
#define TEMP_MAX_C 70.0f
#define PRESS_MIN_PA 30000.0f
#define PRESS_MAX_PA 110000.0f

// ---- Demo mode ---------------------------------------------------------------
#define DEMO_TIME_SCALE 60        // 1 real second = 60 simulated seconds
#define DEMO_TEMP_OFFSET_C 6.0f

// ---- Alerts / IoT ------------------------------------------------------------
#define HIGH_REPEAT_MS 60000UL    // low-rate repeat while High persists
#define PUBLISH_PERIOD_MS 2000UL
#define HISTORY_PERIOD_MS 60000UL
#define HISTORY_MAX_BYTES 65536
