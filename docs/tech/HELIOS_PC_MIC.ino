#include <Arduino.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <WebSocketsServer.h>
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/c/common.h"
#include "helios_model.h"
#include "mfcc_calib.h"
#include "mfcc.h"

// ===== Wi-Fi settings =====
const char* WIFI_SSID = "realme NARZO 70 Turbo 5G";
const char* WIFI_PASS = "Sukoi Mk30";
const uint16_t WS_PORT = 8765;
// ===========================

WebSocketsServer ws(WS_PORT);

constexpr int SAMPLE_RATE = 16000;
constexpr int HOP_SAMPLES = 160;       // 10 ms
constexpr int WINDOW_SAMPLES = 8000;   // 500 ms
constexpr int MFCC_FRAMES = 49;
constexpr int MFCC_COEFFS = 40;

// Start at 96 KB, matching the HELIOS design target.
// Increase only if AllocateTensors() fails.
constexpr size_t ARENA_SIZE = 96 * 1024;

static uint8_t tensor_arena[ARENA_SIZE] __attribute__((aligned(16)));

// Keep these buffers small. MFCC is computed frame-by-frame internally.
// Keep only the rolling audio needed by the MFCC implementation.
// A 500 ms window is accumulated dynamically in PSRAM when available,
// otherwise in a compact heap allocation.
static int16_t* audio_ring = nullptr;
static size_t ring_write = 0;
static size_t ring_count = 0;
static size_t hop_fill = 0;
static int16_t hop_buffer[HOP_SAMPLES];

static tflite::MicroInterpreter* interpreter = nullptr;
static TfLiteTensor* input = nullptr;
static TfLiteTensor* output = nullptr;

static uint32_t audio_received = 0;
static uint32_t inference_count = 0;
static uint32_t detection_count = 0;
static int64_t infer_total_us = 0;
static int64_t infer_min_us = INT64_MAX;
static int64_t infer_max_us = 0;
static uint32_t last_metric_ms = 0;

static void ring_push(const int16_t* p, size_t n) {
  if (!audio_ring) return;

  for (size_t i = 0; i < n; ++i) {
    audio_ring[ring_write] = p[i];
    ring_write = (ring_write + 1) % WINDOW_SAMPLES;
    if (ring_count < WINDOW_SAMPLES) ++ring_count;
  }
}

static bool ring_copy_latest(int16_t* dst, size_t n) {
  if (ring_count < n) return false;

  size_t start = (ring_write + WINDOW_SAMPLES - n) % WINDOW_SAMPLES;
  for (size_t i = 0; i < n; ++i) {
    dst[i] = audio_ring[(start + i) % WINDOW_SAMPLES];
  }
  return true;
}

static bool run_inference() {
  static int16_t* window = nullptr;
  static float features[MFCC_FRAMES * MFCC_COEFFS];

  if (!window) {
    window = (int16_t*)malloc(WINDOW_SAMPLES * sizeof(int16_t));
    if (!window) {
      Serial.println("[ERROR] Cannot allocate MFCC audio window");
      return false;
    }
  }

  if (!ring_copy_latest(window, WINDOW_SAMPLES)) return false;
  if (!input || input->type != kTfLiteInt8) return false;

  // mfcc_compute_49x40() writes 49x40 float features.
  mfcc_compute_49x40(window, WINDOW_SAMPLES, features);

  const float in_scale = input->params.scale;
  const int in_zero = input->params.zero_point;

  int8_t* dst = input->data.int8;

  for (int i = 0; i < MFCC_FRAMES * MFCC_COEFFS; ++i) {
    float z = (features[i] - MFCC_MEAN) / MFCC_STD;
    int q = (int)lroundf(z / in_scale) + in_zero;

    if (q < -128) q = -128;
    if (q > 127) q = 127;

    dst[i] = (int8_t)q;
  }

  const int64_t t0 = esp_timer_get_time();
  const TfLiteStatus st = interpreter->Invoke();
  const int64_t dt = esp_timer_get_time() - t0;

  if (st != kTfLiteOk) {
    Serial.println("[ERROR] TFLite Invoke failed");
    return false;
  }

  ++inference_count;
  infer_total_us += dt;
  if (dt < infer_min_us) infer_min_us = dt;
  if (dt > infer_max_us) infer_max_us = dt;

  int best = 0;
  int bestv = output->data.int8[0];

  for (int i = 1; i < 3; ++i) {
    if (output->data.int8[i] > bestv) {
      best = i;
      bestv = output->data.int8[i];
    }
  }

  float score =
      (bestv - output->params.zero_point) * output->params.scale;

  if (score < 0) score = 0;
  if (score > 1) score = 1;

  const char* label =
      (best == 0) ? "HELIOS" :
      (best == 1) ? "UNKNOWN" : "SILENCE";

  Serial.printf(
      "[KWS] %s  score=%.3f  infer=%.1f ms\n",
      label, score, dt / 1000.0f);

  // Require two HELIOS hits within 1.5 seconds.
  static int hits = 0;
  static uint32_t last_hit = 0;

  if (best == 0 && score >= 0.55f) {
    if (millis() - last_hit > 1500) hits = 0;

    ++hits;
    last_hit = millis();

    if (hits >= 2) {
      ++detection_count;
      hits = 0;

      Serial.println();
      Serial.println("========================================");
      Serial.println("       HELIOS KEYWORD DETECTED");
      Serial.println("========================================");
      Serial.printf("  Score       : %.3f\n", score);
      Serial.printf("  Latency     : %.1f ms\n", dt / 1000.0f);
      Serial.printf("  Free heap   : %u bytes\n", ESP.getFreeHeap());
      Serial.printf("  Detections  : %u\n", detection_count);
      Serial.println("========================================");
      Serial.println();
    }
  }

  return true;
}

static void print_report() {
  const tflite::Model* model = tflite::GetModel(helios_model);

  Serial.println();
  Serial.println("========================================");
  Serial.println(" HELIOS — ESP32 DevKitC-1 PC/PHONE MIC");
  Serial.println("========================================");
  Serial.printf(
      "Model size (flash) : %u bytes (%.1f KB)\n",
      HELIOS_MODEL_LEN, HELIOS_MODEL_LEN / 1024.0f);
  Serial.printf(
      "Tensor arena       : %u bytes (%.1f KB)\n",
      (unsigned)ARENA_SIZE, ARENA_SIZE / 1024.0f);
  Serial.printf(
      "Free heap           : %u bytes (%.1f KB)\n",
      ESP.getFreeHeap(), ESP.getFreeHeap() / 1024.0f);
  Serial.printf(
      "Input               : 1 x %d x %d x 1 int8\n",
      MFCC_FRAMES, MFCC_COEFFS);
  Serial.printf(
      "Input quantization  : scale=%.9f zero=%d\n",
      input ? input->params.scale : 0.0f,
      input ? input->params.zero_point : 0);
  Serial.printf(
      "Output quantization : scale=%.9f zero=%d\n",
      output ? output->params.scale : 0.0f,
      output ? output->params.zero_point : 0);
  Serial.printf("TFLite version      : %d\n", model->version());
  Serial.println("Audio: 16 kHz mono signed 16-bit PCM");
  Serial.println("WebSocket port: 8765");
  Serial.println("Waiting for PC/phone audio...");
  Serial.println();
}

static void ws_event(
    uint8_t num,
    WStype_t type,
    uint8_t* payload,
    size_t length) {

  if (type == WStype_CONNECTED) {
    Serial.printf("[WS] Client %u connected\n", num);
    ws.sendTXT(num, "HELIOS_READY");
    return;
  }

  if (type == WStype_DISCONNECTED) {
    Serial.printf("[WS] Client %u disconnected\n", num);
    return;
  }

  if (type == WStype_TEXT) {
    if (length && payload) {
      Serial.printf(
          "[WS] text: %.*s\n",
          (int)length,
          (char*)payload);
    }
    return;
  }

  if (type != WStype_BIN || !payload || length == 0) return;

  if (length & 1) {
    Serial.println("[WS] Ignoring odd-length PCM packet");
    return;
  }

  const int16_t* samples =
      reinterpret_cast<const int16_t*>(payload);
  size_t n = length / sizeof(int16_t);

  audio_received += n;
  ring_push(samples, n);

  // Run one inference for every 160 new samples once the
  // rolling 500 ms window is full.
  while (n > 0) {
    const size_t remaining = HOP_SAMPLES - hop_fill;
    const size_t take = (n < remaining) ? n : remaining;

    memcpy(
        hop_buffer + hop_fill,
        samples,
        take * sizeof(int16_t));

    hop_fill += take;
    samples += take;
    n -= take;

    if (hop_fill == HOP_SAMPLES) {
      hop_fill = 0;

      if (ring_count >= WINDOW_SAMPLES) {
        run_inference();
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);

  Serial.println("\n[BOOT] HELIOS starting...");

  const tflite::Model* model = tflite::GetModel(helios_model);

  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf(
        "[ERROR] Model schema %d != runtime %d\n",
        model->version(),
        TFLITE_SCHEMA_VERSION);

    while (true) delay(1000);
  }

  // AllOpsResolver is used because this matches the TensorFlowLite_ESP32
  // API available in the project. No MicroMutableOpResolver is required.
  static tflite::AllOpsResolver resolver;

  // Your installed TensorFlowLite_ESP32 build does not expose
  // tflite::MicroErrorReporter, so pass nullptr as the reporter.
  static tflite::MicroInterpreter static_interpreter(
      model,
      resolver,
      tensor_arena,
      ARENA_SIZE,
      nullptr);

  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println();
    Serial.println("[ERROR] AllocateTensors failed.");
    Serial.println("[ERROR] Try ARENA_SIZE = 128 * 1024.");
    while (true) delay(1000);
  }

  input = interpreter->input(0);
  output = interpreter->output(0);

  print_report();

  audio_ring = (int16_t*)malloc(WINDOW_SAMPLES * sizeof(int16_t));
  if (!audio_ring) {
    Serial.println("[ERROR] Cannot allocate audio ring buffer");
    while (true) delay(1000);
  }
  memset(audio_ring, 0, WINDOW_SAMPLES * sizeof(int16_t));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("[WiFi] Connecting");

  const unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 15000) {
    delay(400);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println(
        "[WiFi] FAILED. Check WIFI_SSID/WIFI_PASS and reboot.");
    return;
  }

  Serial.printf(
      "\n[WiFi] Connected. ESP32 IP=%s RSSI=%d dBm\n",
      WiFi.localIP().toString().c_str(),
      WiFi.RSSI());

  // WebSocketsServer(WS_PORT) already stores the port.
  // The installed library's begin() takes no argument.
  ws.begin();
  ws.onEvent(ws_event);

  Serial.printf(
      "[WS] Listening on ws://%s:%u\n",
      WiFi.localIP().toString().c_str(),
      WS_PORT);

  Serial.println(
      "[READY] Start the PC microphone sender now.");
}

void loop() {
  ws.loop();

  if (millis() - last_metric_ms >= 10000) {
    last_metric_ms = millis();

    const float avg =
        inference_count
            ? infer_total_us / (float)inference_count / 1000.0f
            : 0.0f;

    Serial.printf(
        "[METRICS] avg_infer=%.1f ms min=%.1f ms max=%.1f ms "
        "heap=%u B audio=%lu samples detections=%lu\n",
        avg,
        infer_min_us == INT64_MAX ? 0.0f : infer_min_us / 1000.0f,
        infer_max_us / 1000.0f,
        ESP.getFreeHeap(),
        (unsigned long)audio_received,
        (unsigned long)detection_count);
  }
}
