#include <driver/i2s.h>

// I2S pins for INMP441
#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33
#define I2S_PORT I2S_NUM_0

#define SAMPLE_RATE 16000

// DC offset filter
int32_t dcOffset = 0;
const float DC_FILTER_ALPHA = 0.95;  // Higher = slower adaptation

void setupI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_PORT, &pin_config);
  i2s_zero_dma_buffer(I2S_PORT);
  
  Serial.println("I2S initialized!");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("INMP441 Mic Test (DC Offset Corrected)");
  Serial.println("=======================================");
  Serial.println("Open Serial Plotter to see waveform");
  Serial.println();
  
  setupI2S();
  
  // Calibrate DC offset at startup (read a few samples in silence)
  Serial.println("Calibrating... keep quiet for 1 second");
  delay(500);
  
  int32_t calibrationSum = 0;
  int calibrationSamples = 0;
  
  for (int i = 0; i < 10; i++) {
    int16_t samples[512];
    size_t bytesRead = 0;
    i2s_read(I2S_PORT, samples, sizeof(samples), &bytesRead, portMAX_DELAY);
    
    int numSamples = bytesRead / 2;
    for (int j = 0; j < numSamples; j++) {
      calibrationSum += samples[j];
      calibrationSamples++;
    }
  }
  
  dcOffset = calibrationSum / calibrationSamples;
  Serial.printf("DC Offset calibrated: %d\n", dcOffset);
  Serial.println("Ready! Try speaking or clapping.\n");
}

void loop() {
  int16_t rawSamples[512];
  int16_t samples[512];
  size_t bytesRead = 0;

  i2s_read(I2S_PORT, rawSamples, sizeof(rawSamples), &bytesRead, portMAX_DELAY);
  
  if (bytesRead > 0) {
    int numSamples = bytesRead / 2;
    
    // Remove DC offset and calculate stats
    int32_t sum = 0;
    int16_t minVal = 32767;
    int16_t maxVal = -32768;
    
    for (int i = 0; i < numSamples; i++) {
      // Remove DC offset
      samples[i] = rawSamples[i] - dcOffset;
      
      // Update running DC offset estimate (adaptive filter)
      dcOffset = (DC_FILTER_ALPHA * dcOffset) + ((1.0 - DC_FILTER_ALPHA) * rawSamples[i]);
      
      sum += abs(samples[i]);
      if (samples[i] < minVal) minVal = samples[i];
      if (samples[i] > maxVal) maxVal = samples[i];
    }
    
    int16_t avgAmplitude = sum / numSamples;
    int16_t peakToPeak = maxVal - minVal;
    
    // For Serial Plotter - now centered around 0
    Serial.print(samples[0]);         // Waveform (should be around 0 in silence)
    Serial.print(",");
    Serial.print(avgAmplitude);       // Volume level
    Serial.print(",");
    Serial.println(peakToPeak / 10);  // Peak-to-peak scaled
    
    // Uncomment for text output:

    Serial.print("Amplitude: ");
    Serial.print(avgAmplitude);
    Serial.print(" | P2P: ");
    Serial.print(peakToPeak);
    Serial.print(" | Range: ");
    Serial.print(minVal);
    Serial.print(" to ");
    Serial.println(maxVal);
  
  }
  
  delay(10);
}