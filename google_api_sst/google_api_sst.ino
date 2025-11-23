#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <driver/i2s.h>
#include "mbedtls/base64.h"

// WiFi credentials
const char* ssid = "Kverse";
const char* password = "12345678";

// Google Cloud API Key
const char* apiKey = "AIzaSyDKJ7bcEZ8u4VqzFdpYCfbBgweqSeoKCls";

// I2S pins for INMP441
#define I2S_WS 25
#define I2S_SD 32
#define I2S_SCK 33
#define I2S_PORT I2S_NUM_0

// Audio settings
#define SAMPLE_RATE 16000
#define RECORD_SECONDS 3
#define AUDIO_BUFFER_SIZE (SAMPLE_RATE * RECORD_SECONDS * 2)

int16_t* audioBuffer;
size_t audioSize = 0;

// DC offset
int32_t dcOffset = 0;
const float DC_ALPHA = 0.95;

#define BUTTON_PIN 0
#define LED_PIN 2  // Built-in LED

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
}

void calibrateMic() {
  Serial.println("Calibrating...");
  delay(200);
  
  int32_t sum = 0;
  int cnt = 0;
  int16_t buf[256];
  size_t rd;
  
  for (int i = 0; i < 8; i++) {
    i2s_read(I2S_PORT, buf, sizeof(buf), &rd, portMAX_DELAY);
    for (int j = 0; j < rd / 2; j++) {
      sum += buf[j];
      cnt++;
    }
  }
  dcOffset = sum / cnt;
  Serial.printf("DC Offset: %d\n", dcOffset);
}

void recordAudio() {
  // Signal start
  digitalWrite(LED_PIN, HIGH);
  Serial.println("\n********************************");
  Serial.println("*** LED ON - SPEAK NOW! ***");
  Serial.println("********************************");
  
  size_t idx = 0;
  size_t maxSamples = AUDIO_BUFFER_SIZE / 2;
  int16_t buf[256];
  size_t rd;
  int32_t maxAmp = 0;

  unsigned long start = millis();
  while (millis() - start < RECORD_SECONDS * 1000 && idx < maxSamples) {
    i2s_read(I2S_PORT, buf, sizeof(buf), &rd, portMAX_DELAY);
    for (size_t i = 0; i < rd / 2 && idx < maxSamples; i++) {
      int16_t s = buf[i] - dcOffset;
      s = s * 2;  // 2x gain
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      audioBuffer[idx++] = s;
      if (abs(s) > maxAmp) maxAmp = abs(s);
      dcOffset = DC_ALPHA * dcOffset + (1 - DC_ALPHA) * buf[i];
    }
  }
  
  audioSize = idx * 2;
  
  // Signal stop
  digitalWrite(LED_PIN, LOW);
  Serial.println("*** LED OFF - STOPPED ***\n");
  Serial.printf("Recorded %d bytes, max amplitude: %d\n", audioSize, maxAmp);
}

void encodeChunk(uint8_t* in, size_t inLen, char* out, size_t* outLen) {
  mbedtls_base64_encode((unsigned char*)out, *outLen, outLen, in, inLen);
}

String sendToGoogleSTT() {
  WiFiClientSecure client;
  client.setInsecure();
  
  Serial.println("Connecting to Google...");
  if (!client.connect("speech.googleapis.com", 443)) {
    Serial.println("Connection failed!");
    return "Connection failed";
  }

  size_t b64Size = ((audioSize + 2) / 3) * 4;
  
  String jsonStart = "{\"config\":{\"encoding\":\"LINEAR16\",\"sampleRateHertz\":16000,\"languageCode\":\"en-US\"},\"audio\":{\"content\":\"";
  String jsonEnd = "\"}}";
  
  size_t contentLength = jsonStart.length() + b64Size + jsonEnd.length();

  client.println("POST /v1/speech:recognize?key=" + String(apiKey) + " HTTP/1.1");
  client.println("Host: speech.googleapis.com");
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(contentLength));
  client.println("Connection: close");
  client.println();

  client.print(jsonStart);

  Serial.println("Streaming audio...");
  const size_t chunkSize = 300;
  char b64Chunk[410];
  uint8_t* audioBytes = (uint8_t*)audioBuffer;

  for (size_t i = 0; i < audioSize; i += chunkSize) {
    size_t len = min(chunkSize, audioSize - i);
    size_t b64Len = sizeof(b64Chunk);
    encodeChunk(audioBytes + i, len, b64Chunk, &b64Len);
    client.write((uint8_t*)b64Chunk, b64Len);
    yield();
  }

  client.print(jsonEnd);

  Serial.println("Waiting for response...");
  
  unsigned long timeout = millis();
  while (client.connected() && !client.available()) {
    if (millis() - timeout > 30000) {
      Serial.println("Timeout!");
      client.stop();
      return "Timeout";
    }
    delay(10);
  }

  while (client.available()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") break;
  }

  String response = "";
  while (client.available()) {
    response += client.readString();
  }
  client.stop();

  int jsonStart_idx = response.indexOf('{');
  if (jsonStart_idx > 0) {
    response = response.substring(jsonStart_idx);
  }

  Serial.println("--- Response ---");
  Serial.println(response.substring(0, 300));
  Serial.println("----------------");

  int idx = response.indexOf("\"transcript\": \"");
  if (idx < 0) idx = response.indexOf("\"transcript\":\"");
  
  if (idx > 0) {
    int start = response.indexOf("\"", idx + 13) + 1;
    int end = response.indexOf("\"", start);
    if (end > start) {
      String transcript = response.substring(start, end);
      Serial.println("SUCCESS: " + transcript);
      return transcript;
    }
  }
  
  if (response.indexOf("\"error\"") > 0) {
    return "API Error";
  }
  
  if (response.indexOf("results") > 0) {
    return "[Empty result]";
  }
  
  return "[No speech]";
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== ESP32 Speech-to-Text ===\n");
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  audioBuffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE);
  if (!audioBuffer) {
    Serial.println("Buffer allocation failed!");
    while (1);
  }
  Serial.printf("Audio buffer: %d bytes\n", AUDIO_BUFFER_SIZE);

  WiFi.begin(ssid, password);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  setupI2S();
  calibrateMic();
  
  // Blink LED to show ready
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
  
  Serial.println("\n=== READY! ===");
  Serial.println("Press BOOT button - LED will turn ON");
  Serial.println("Speak while LED is ON (3 seconds)");
  Serial.println("LED turns OFF when recording stops\n");
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    if (digitalRead(BUTTON_PIN) == LOW) {
      memset(audioBuffer, 0, AUDIO_BUFFER_SIZE);
      recordAudio();
      
      String result = sendToGoogleSTT();
      
      Serial.println("\n========== RESULT ==========");
      Serial.println(result);
      Serial.println("============================\n");
      
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
      Serial.println("Ready...\n");
    }
  }
  delay(10);
}