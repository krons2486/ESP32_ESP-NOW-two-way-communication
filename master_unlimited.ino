#include <esp_now.h>
#include <WiFi.h>
#include <LittleFS.h>

#define GROUP_SIZE 20

typedef struct {
  uint8_t mac[6];
  uint32_t id;
  float voltage;
  float current;
  float temperature;
} device_data_t;

uint8_t deviceMacs[][6] = {
  { 0xAC, 0x15, 0x18, 0xE9, 0x86, 0x3C },  // Пример MAC
                                           // ...
};

int currentGroup = 0;
int totalDevices = 0;  // Фактическое число устройств

device_data_t receivedData;
bool dataReceived = false;

uint8_t groupMacs[GROUP_SIZE][6];
int groupSize = 0;

esp_now_peer_info_t peerInfo;

// Получение фактического количества MAC-адресов в массиве
int getFilledMacCount() {
  int count = 0;
  for (size_t i = 0; i < sizeof(deviceMacs) / sizeof(deviceMacs[0]); i++) {
    bool isNonZero = false;
    for (size_t j = 0; j < 6; j++) {
      if (deviceMacs[i][j] != 0x00) {
        isNonZero = true;
        break;
      }
    }
    if (isNonZero) count++;
  }
  return count;
}

void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  memcpy(&receivedData, incomingData, sizeof(receivedData));
  updateFileWithData(receivedData);
  dataReceived = true;  // Устанавливаем флаг
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

void updateFileWithData(const device_data_t &data) {
  File file = LittleFS.open("/device_data.bin", FILE_READ);
  String updatedData = "";
  bool found = false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    if (line.indexOf(macToString(data.mac)) != -1) {
      line = formatDeviceData(data);
      found = true;
    }
    updatedData += line + "\n";
  }
  file.close();

  if (!found) {
    updatedData += formatDeviceData(data) + "\n";
  }

  file = LittleFS.open("/device_data.bin", FILE_WRITE);
  file.print(updatedData);
  file.close();
}

String formatDeviceData(const device_data_t &data) {
  char buffer[128];
  snprintf(buffer, sizeof(buffer), "MAC: %s ID: %u Voltage: %.2fV Current: %.2fA Temperature: %.2fC",
           macToString(data.mac).c_str(), data.id, data.voltage, data.current, data.temperature);
  return String(buffer);
}

String macToString(const uint8_t *mac) {
  char buffer[18];
  snprintf(buffer, sizeof(buffer), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buffer);
}

void loadGroup(int groupIndex) {
  groupSize = 0;
  int startIdx = groupIndex * GROUP_SIZE;
  int endIdx = min(startIdx + GROUP_SIZE, totalDevices);

  for (int i = startIdx; i < endIdx; i++) {
    memcpy(groupMacs[groupSize], deviceMacs[i], 6);
    groupSize++;
  }
}

void initializePeers() {
  for (int i = 0; i < groupSize; i++) {
    memcpy(peerInfo.peer_addr, groupMacs[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Failed to add peer: %s\n", macToString(groupMacs[i]).c_str());
    }
  }
}

void clearPeers() {
  for (int i = 0; i < groupSize; i++) {
    esp_now_del_peer(groupMacs[i]);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW initialization failed");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed");
    return;
  }

  if (!LittleFS.exists("/device_data.bin")) {
    File file = LittleFS.open("/device_data.bin", FILE_WRITE);
    file.close();
  }

  totalDevices = getFilledMacCount();  // Подсчитать количество фактических устройств
  Serial.printf("Total devices found: %d\n", totalDevices);

  Serial.println("Master setup complete");
}

void loop() {
  if (totalDevices == 0) {
    Serial.println("No devices in the list. Waiting...");
    delay(1000);
    return;
  }

  loadGroup(currentGroup);
  initializePeers();

  dataReceived = false;

  for (int i = 0; i < groupSize; i++) {
    uint8_t request[] = { 0x01 };
    esp_err_t result = esp_now_send(groupMacs[i], request, sizeof(request));
    if (result == ESP_OK) {
      Serial.printf("Request sent to %s\n", macToString(groupMacs[i]).c_str());
    } else {
      Serial.printf("Error sending request to %s\n", macToString(groupMacs[i]).c_str());
    }
  }

  // Ждём данные (до 500 мс), но продолжаем сразу, если данные пришли
  unsigned long startWait = millis();
  while (!dataReceived && (millis() - startWait < 500)) {
    delay(10);  // Небольшая задержка для снижения нагрузки
  }

  clearPeers();  // Очистка пиров только после обработки данных

  currentGroup = (currentGroup + 1) % ((totalDevices + GROUP_SIZE - 1) / GROUP_SIZE);

  delay(1000);  // Задержка перед обработкой следующей группы
}