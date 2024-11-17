#include <esp_now.h>
#include <WiFi.h>

// MAC адрес мастера
uint8_t masterMac[] = {0xCC, 0x7B, 0x5C, 0x34, 0xCA, 0x78};

// Структура для данных устройства
typedef struct {
  uint8_t mac[6];      // MAC-адрес устройства
  uint32_t id;         // Идентификатор устройства
  float voltage;       // Напряжение
  float current;       // Ток
  float temperature;   // Температура
} device_data_t;

// Переменная для хранения данных
device_data_t dataToSend;

esp_now_peer_info_t peerInfo;

// Функция обратного вызова при отправке данных
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("Data sent successfully");
  } else {
    Serial.println("Error sending data");
  }
}

// Функция для обработки полученных запросов
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == 1 && incomingData[0] == 0x01) {  // Если это запрос на получение данных
    // Заполняем данные для отправки
    dataToSend.id = 12345;
    dataToSend.voltage = 3.3;
    dataToSend.current = 0.5;
    dataToSend.temperature = 25.0;
    memcpy(dataToSend.mac, masterMac, 6);
    
    // Отправляем данные мастеру
    esp_err_t result = esp_now_send(masterMac, (uint8_t *)&dataToSend, sizeof(dataToSend));
    if (result == ESP_OK) {
      Serial.println("Data sent to master");
    } else {
      Serial.println("Error sending data");
    }
  }
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

  // Регистрируем функцию обратного вызова для отправки данных
  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  // Добавляем мастера как пира
  memcpy(peerInfo.peer_addr, masterMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  Serial.println("Slave setup complete");
}

void loop() {
  delay(1000);  // Просто задержка для предотвращения сплошных циклов
}
