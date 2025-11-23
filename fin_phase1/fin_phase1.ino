/*
  FINAL FIXED Phase-1 sketch
  ESP32 + MAX9814 + ADC + EdgeImpulse
  Commands:
    "yes"   -> LED ON
    "no"    -> LED OFF
    "marvin" -> LED blink
*/

#include <Arduino.h>
#include "embbeded_voice_recogination_system_inferencing.h"
#include "driver/adc.h"

#define ADC_PIN        34
#define LED_PIN        2
#define SAMPLE_RATE    16000
#define CONF_THRESHOLD 0.55f
#define CONF_CONFIRM   1      // 2 consecutive frames

// EI buffer
static float ei_float_buffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];

// Confirmation counters
static int confirm_yes = 0;
static int confirm_no = 0;
static int confirm_marvin = 0;

// ---------- SMALL IMPROVED AUDIO CAPTURE (NO MAJOR CHANGE) ----------
void capture_audio_float(float *out_buf, size_t len) {
  uint32_t period_us = 1000000UL / SAMPLE_RATE;

  for (size_t i = 0; i < len; i++) {
    int raw = analogRead(ADC_PIN);        // 0–4095
    raw = constrain(raw, 0, 4095);

    // DC removal + normalization
    float norm = ((float)raw - 2048.0f) / 2048.0f;

    // Soft AGC to avoid MAX9814 saturation
    norm *= 1.4f;
    norm = constrain(norm, -1.0f, 1.0f);

    out_buf[i] = norm;
    delayMicroseconds(period_us);
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN, ADC_11db);

  Serial.println("\n=== Voice Recognition Ready ===");
  Serial.print("Model expects samples: ");
  Serial.println(EI_CLASSIFIER_RAW_SAMPLE_COUNT);
}

void do_classification_and_action() {

  // 1) Capture audio
  capture_audio_float(ei_float_buffer, EI_CLASSIFIER_RAW_SAMPLE_COUNT);

  // 2) Convert to signal
  signal_t signal;
  numpy::signal_from_buffer(ei_float_buffer, EI_CLASSIFIER_RAW_SAMPLE_COUNT, &signal);

  // 3) Run inference
  ei_impulse_result_t result;
  EI_IMPULSE_ERROR ei_error = run_classifier(&signal, &result, false);
  if (ei_error != EI_IMPULSE_OK) {
    Serial.println("run_classifier failed!");
    return;
  }

  // 4) Print probabilities
  Serial.println("\n--- Prediction ---");
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    Serial.print(result.classification[i].label);
    Serial.print(" : ");
    Serial.println(result.classification[i].value, 4);
  }

  // 5) Identify commands
  float p_yes = 0, p_no = 0, p_marvin = 0;

  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
    const char *lab = result.classification[i].label;
    float v = result.classification[i].value;

    if (strcmp(lab, "yes") == 0) p_yes = v;
    if (strcmp(lab, "no") == 0) p_no = v;
    if (strcmp(lab, "marvin") == 0) p_marvin = v;
  }

  // 6) Confirmation logic
  (p_yes > CONF_THRESHOLD) ? confirm_yes++ : (confirm_yes = 0);
  (p_no  > CONF_THRESHOLD) ? confirm_no++  : (confirm_no = 0);
  (p_marvin > CONF_THRESHOLD) ? confirm_marvin++ : (confirm_marvin = 0);

  // 7) Actions
  if (confirm_yes >= CONF_CONFIRM) {
    Serial.println(">>> YES detected -> LED ON");
    digitalWrite(LED_PIN, HIGH);
    confirm_yes = confirm_no = confirm_marvin = 0;
  }

  else if (confirm_no >= CONF_CONFIRM) {
    Serial.println(">>> NO detected -> LED OFF");
    digitalWrite(LED_PIN, LOW);
    confirm_yes = confirm_no = confirm_marvin = 0;
  }

  else if (confirm_marvin >= CONF_CONFIRM) {
    Serial.println(">>> MARVIN detected -> blink");
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED_PIN, HIGH); delay(120);
      digitalWrite(LED_PIN, LOW);  delay(120);
    }
    confirm_yes = confirm_no = confirm_marvin = 0;
  }

  delay(8); // tiny cooldown
}

void loop() {
  do_classification_and_action();
}
