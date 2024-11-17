#include <esp_now.h>
#include <WiFi.h>
#include <LittleFS.h>

// MAC адрес слейва
uint8_t slaveMac[] = {0xAC, 0x15, 0x18, 0xE9, 0x86, 0x3C};

// Структура для данных устройства
typedef struct {
  uint8_t mac[6];      // MAC-адрес устройства
  uint32_t id;         // Идентификатор устройства
  float voltage;       // Напряжение
  float current;       // Ток
  float temperature;   // Температура
} device_data_t;

// Переменная для хранения полученных данных
device_data_t receivedData;

esp_now_peer_info_t peerInfo;

// Функция обратного вызова при получении данных
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  // Копируем принятые данные в переменную
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  /* 
  // Выводим полученные данные в Serial Monitor
  Serial.println("Data received from slave:");
  Serial.printf("MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X:", receivedData.mac[i]);
  }
  Serial.println();
  Serial.printf("ID: %u\n", receivedData.id);
  Serial.printf("Voltage: %.2f V\n", receivedData.voltage);
  Serial.printf("Current: %.2f A\n", receivedData.current);
  Serial.printf("Temperature: %.2f C\n", receivedData.temperature);
  */

  // Записываем данные в LittleFS
  File file = LittleFS.open("/device_data.bin", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.write((uint8_t *)&receivedData, sizeof(receivedData));
  file.close();
  Serial.println("Data written to file");
  
  // После записи данных в файл, читаем их и выводим в Serial Monitor
  readDataFromFile();
}

// Функция для чтения данных из файла
void readDataFromFile() {
  File file = LittleFS.open("/device_data.bin", FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Читаем данные из файла
  device_data_t readData;
  file.read((uint8_t *)&readData, sizeof(readData));
  file.close();

  // Отображаем данные в десятичном формате
  Serial.println("Data read from file:");
  Serial.printf("MAC: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X:", readData.mac[i]);
  }
  Serial.println();
  Serial.printf("ID: %u\n", readData.id);
  Serial.printf("Voltage: %.2f V\n", readData.voltage);
  Serial.printf("Current: %.2f A\n", readData.current);
  Serial.printf("Temperature: %.2f C\n", readData.temperature);
}

void setup() {
  Serial.begin(115200);
  
  // Инициализация Wi-Fi в режиме STA
  WiFi.mode(WIFI_STA);
  
  // Инициализация ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  // Добавляем слейва
  memcpy(peerInfo.peer_addr, slaveMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Регистрируем функцию обратного вызова для получения данных
  esp_now_register_recv_cb(OnDataRecv);

  // Инициализация LittleFS
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }
  
  Serial.println("Master setup complete");
}

void loop() {
  static bool lastRequestSuccess = true;  // Флаг успешности последнего запроса

  if (lastRequestSuccess) {
    // Мастер отправляет запрос с минимальными данными (например, запрос на получение данных)
    uint8_t request[] = {0x01};  // 0x01 — это простой запрос на получение данных

    Serial.println("Sending request to slave...");
    esp_err_t result = esp_now_send(slaveMac, request, sizeof(request));
    
    if (result == ESP_OK) {
      Serial.println("Request sent successfully");
      lastRequestSuccess = true;
    } else {
      Serial.println("Error sending request");
      lastRequestSuccess = false;
    }
  }

  // Задержка 1 секунда перед следующим запросом
  delay(5000);
}