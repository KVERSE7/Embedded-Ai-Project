#include <Arduino.h>
#include "embbeded_voice_recogination_system_inferencing.h"   // your EI library

// -----------------------------
// USER SETTINGS
// -----------------------------
#define adcPin 34          // <-- CHANGE THIS TO YOUR ADC PIN
#define EI_CLASSIFIER_FREQUENCY 16000
#define BLOCK_SIZE 1024     // Samples per ADC batch
#define SAMPLE_TIME_US (1000000 / EI_CLASSIFIER_FREQUENCY)

// Buffer for audio
static int16_t audio_buffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];

// -----------------------------
// CAPTURE AUDIO (100ms window)
// -----------------------------
void capture_audio() {
    for (int i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {

        int raw = analogRead(adcPin);    // 0–4095
        raw = raw - 2048;               // Remove DC offset
        raw = raw / 2;                  // Scale down to int16 range

        audio_buffer[i] = (int16_t)raw;
        delayMicroseconds(SAMPLE_TIME_US);
    }
}

// -----------------------------
// CLASSIFICATION
// -----------------------------
void classify_audio() {

    // Convert int16_t buffer to float buffer
    static float float_buffer[EI_CLASSIFIER_RAW_SAMPLE_COUNT];

    for (int i = 0; i < EI_CLASSIFIER_RAW_SAMPLE_COUNT; i++) {
        float_buffer[i] = (float)audio_buffer[i];
    }

    signal_t signal;

    // FIX: signal_from_buffer returns int, so store as int
    int ret = numpy::signal_from_buffer(
                  float_buffer,
                  EI_CLASSIFIER_RAW_SAMPLE_COUNT,
                  &signal);

    if (ret != 0) {
        Serial.println("Failed to create signal!");
        return;
    }

    ei_impulse_result_t result;

    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

    if (res != EI_IMPULSE_OK) {
        Serial.println("Classifier error!");
        return;
    }

    Serial.println("\n=== PREDICTION ===");
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        Serial.print(result.classification[i].label);
        Serial.print(" : ");
        Serial.println(result.classification[i].value, 5);
    }
}


// -----------------------------
// SETUP
// -----------------------------
void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);   // best for audio signals
    Serial.println("\nVoice Recognition System Ready...");
}

// -----------------------------
// LOOP
// -----------------------------
void loop() {
    capture_audio();
    classify_audio();
}
