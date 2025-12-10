// include/wifi_config.h
#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ========================
// 自动检测芯片型号
// ========================
#ifdef ESP8266
  #include <ESP8266WiFi.h>
  #define CHIP_TYPE "ESP8266"
#elif defined(ESP32)
  #include <WiFi.h>
  #define CHIP_TYPE "ESP32"
#else
  #error "Unsupported platform! Please use ESP8266 or ESP32"
#endif

#include <WiFiManager.h>
#include <ArduinoJson.h>

// ========================
// 针对不同芯片的文件系统适配
// ========================
#ifdef ESP8266
  #include <LittleFS.h>
  #define FileSystem LittleFS
#elif defined(ESP32)
  #include <SPIFFS.h>
  #define FileSystem SPIFFS
#endif

#include <vector>
#include <map>

// ========================
// 调试宏定义
// ========================
#define DEBUG_ENABLED true

#if DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(fmt, ...)
#endif

// ========================
// 参数定义结构体
// ========================
struct ConfigParam {
  String key;
  String label;
  String defaultValue;
  int maxLength;
  String value;
  WiFiManagerParameter* wfmParam;
};

// ========================
// 全局容器
// ========================
std::vector<ConfigParam> configParams;
std::map<String, String> configValues;

#define CONFIG_FILE "/config.json"
#define AUTO_START_AP true

// ========================
// 芯片信息结构体
// ========================
struct ChipInfo {
  String chipType;
  String chipModel;
  uint32_t chipId;
  uint32_t flashSize;
  uint32_t heapSize;
};

ChipInfo chipInfo;

// ========================
// 函数声明
// ========================
void initChipInfo();
void printChipInfo();
void registerParam(const char* key, const char* label, const char* defaultValue, int maxLength = 64);
void unregisterParam(const char* key);
void clearAllParams();
String getConfigValue(const char* key);
void setConfigValue(const char* key, const char* value);
bool initFileSystem();
bool initWiFiManager(const char* deviceName = "ESP-Device");
bool readConfig();
bool saveConfig();
void printAllParams();
bool isWiFiConnected();
int getWiFiSignalStrength();
void resetConfig();
void printSystemInfo();

// ========================
// 初始化芯片信息
// ========================
void initChipInfo() {
  chipInfo.chipType = CHIP_TYPE;
  
  #ifdef ESP8266
    chipInfo.chipModel = "ESP8266";
    chipInfo.chipId = ESP.getChipId();
    chipInfo.flashSize = ESP.getFlashChipSize();
    chipInfo.heapSize = ESP.getFreeHeap();
  #elif defined(ESP32)
    chipInfo.chipModel = "ESP32";
    chipInfo.chipId = (uint32_t)ESP.getEfuseMac();
    chipInfo.flashSize = ESP.getFlashChipSize();
    chipInfo.heapSize = ESP.getFreeHeap();
  #endif
  
  DEBUG_PRINTLN("\n=== Chip Info ===");
  DEBUG_PRINTLN("Chip Type: " + chipInfo.chipType);
  DEBUG_PRINTLN("Chip Model: " + chipInfo.chipModel);
  DEBUG_PRINTF("Chip ID: %X\n", chipInfo.chipId);
  DEBUG_PRINTF("Flash Size: %d bytes\n", chipInfo.flashSize);
  DEBUG_PRINTF("Heap Size: %d bytes\n", chipInfo.heapSize);
  DEBUG_PRINTLN("==================\n");
}

// ========================
// 打印系统信息
// ========================
void printSystemInfo() {
  Serial.println("\n");
  Serial.println("╔════════════════════════════════╗");
  Serial.println("║     System Information         ║");
  Serial.println("╠════════════════════════════════╣");
  Serial.printf("║ Chip Type: %-19s ║\n", chipInfo.chipType.c_str());
  Serial.printf("║ Chip Model: %-18s ║\n", chipInfo.chipModel.c_str());
  Serial.printf("║ Chip ID: 0x%-21X ║\n", chipInfo.chipId);
  Serial.printf("║ Flash Size: %-18d ║\n", chipInfo.flashSize);
  Serial.printf("║ Free Heap: %-19d ║\n", chipInfo.heapSize);
  
  if (isWiFiConnected()) {
    Serial.printf("║ WiFi Signal: %-17d ║\n", getWiFiSignalStrength());
    Serial.printf("║ IP Address: %-19s ║\n", WiFi.localIP().toString().c_str());
  }
  
  Serial.println("╚════════════════════════════════╝");
  Serial.println();
}

// ========================
// 注册参数
// ========================
void registerParam(const char* key, const char* label, const char* defaultValue, int maxLength) {
  // 检查是否已存在
  for (auto& param : configParams) {
    if (param.key == key) {
      DEBUG_PRINTLN("Param already registered: " + String(key));
      return;
    }
  }
  
  ConfigParam newParam;
  newParam.key = String(key);
  newParam.label = String(label);
  newParam.defaultValue = String(defaultValue);
  newParam.maxLength = maxLength;
  newParam.value = String(defaultValue);
  newParam.wfmParam = new WiFiManagerParameter(
    key,
    label,
    defaultValue,
    maxLength
  );
  
  configParams.push_back(newParam);
  configValues[String(key)] = String(defaultValue);
  
  DEBUG_PRINTLN("✓ Registered: " + String(key));
}

// ========================
// 注销参数
// ========================
void unregisterParam(const char* key) {
  for (auto it = configParams.begin(); it != configParams.end(); ++it) {
    if (it->key == key) {
      if (it->wfmParam != nullptr) {
        delete it->wfmParam;
      }
      configParams.erase(it);
      configValues.erase(String(key));
      DEBUG_PRINTLN("✗ Unregistered: " + String(key));
      return;
    }
  }
}

// ========================
// 清空所有参数
// ========================
void clearAllParams() {
  for (auto& param : configParams) {
    if (param.wfmParam != nullptr) {
      delete param.wfmParam;
    }
  }
  configParams.clear();
  configValues.clear();
  DEBUG_PRINTLN("All params cleared");
}

// ========================
// 获取参数值
// ========================
String getConfigValue(const char* key) {
  if (configValues.find(String(key)) != configValues.end()) {
    return configValues[String(key)];
  }
  DEBUG_PRINTLN("⚠ Config key not found: " + String(key));
  return "";
}

// ========================
// 设置参数值
// ========================
void setConfigValue(const char* key, const char* value) {
  for (auto& param : configParams) {
    if (param.key == key) {
      param.value = String(value);
      configValues[String(key)] = String(value);
      DEBUG_PRINTLN("✓ Set " + String(key) + " = " + String(value));
      saveConfig();
      return;
    }
  }
  DEBUG_PRINTLN("⚠ Config key not found: " + String(key));
}

// ========================
// 初始化文件系统
// ========================
bool initFileSystem() {
  #ifdef ESP8266
    if (!LittleFS.begin()) {
      DEBUG_PRINTLN("✗ Failed to mount LittleFS");
      return false;
    }
    DEBUG_PRINTLN("✓ LittleFS mounted");
  #elif defined(ESP32)
    if (!SPIFFS.begin(true)) {
      DEBUG_PRINTLN("✗ Failed to mount SPIFFS");
      return false;
    }
    DEBUG_PRINTLN("✓ SPIFFS mounted");
  #endif
  
  return true;
}

// ========================
// 读取配置文件
// ========================
bool readConfig() {
  if (!FileSystem.exists(CONFIG_FILE)) {
    DEBUG_PRINTLN("⚠ Config file not found");
    return false;
  }

  File file = FileSystem.open(CONFIG_FILE, "r");
  if (!file) {
    DEBUG_PRINTLN("✗ Failed to open config file");
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    DEBUG_PRINTLN("✗ Failed to parse config file");
    return false;
  }

  // 从 JSON 读取所有参数
  for (auto& param : configParams) {
    if (doc.containsKey(param.key.c_str())) {
      String value = doc[param.key.c_str()].as<String>();
      param.value = value;
      configValues[param.key] = value;
    }
  }

  DEBUG_PRINTLN("✓ Config loaded successfully");
  return true;
}

// ========================
// 保存配置文件
// ========================
bool saveConfig() {
  File file = FileSystem.open(CONFIG_FILE, "w");
  if (!file) {
    DEBUG_PRINTLN("✗ Failed to open config file for writing");
    return false;
  }

  DynamicJsonDocument doc(2048);
  
  // 保存所有参数
  for (auto& param : configParams) {
    doc[param.key] = param.value;
  }

  serializeJson(doc, file);
  file.close();

  DEBUG_PRINTLN("✓ Config saved successfully");
  return true;
}

// ========================
// 从 WiFiManager 加载配置
// ========================
void loadConfigFromWiFiManager() {
  bool changed = false;
  
  for (auto& param : configParams) {
    if (param.wfmParam != nullptr) {
      String value = String(param.wfmParam->getValue());
      if (value.length() > 0 && value != param.value) {
        param.value = value;
        configValues[param.key] = value;
        changed = true;
      }
    }
  }
  
  if (changed) {
    saveConfig();
    DEBUG_PRINTLN("✓ Config updated from WiFiManager");
  }
}

// ========================
// 初始化 WiFiManager
// ========================
bool initWiFiManager(const char* deviceName) {
  initChipInfo();
  initFileSystem();
  
  // 读取已保存的配置
  if (!readConfig()) {
    DEBUG_PRINTLN("Using default config values");
  }
  
  // 更新 WiFiManager 参数的显示值
  for (auto& param : configParams) {
    if (param.wfmParam != nullptr) {
      param.wfmParam->setValue(param.value.c_str(), param.maxLength);
    }
  }
  
  WiFiManager wifiManager;
  
  // 自定义 WiFiManager 样式（可选）
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(20);
  
  // 添加所有参数到 WiFiManager
  for (auto& param : configParams) {
    if (param.wfmParam != nullptr) {
      wifiManager.addParameter(param.wfmParam);
    }
  }
  
  // 设置回调函数
  wifiManager.setSaveConfigCallback([]() {
    DEBUG_PRINTLN("Config portal saved");
  });
  
  bool connected = false;
  
  if (AUTO_START_AP) {
    DEBUG_PRINTLN("Starting config portal...");
    connected = wifiManager.startConfigPortal(deviceName);
  } else {
    DEBUG_PRINTLN("Attempting auto-connect...");
    connected = wifiManager.autoConnect(deviceName);
  }
  
  if (connected) {
    loadConfigFromWiFiManager();
    
    Serial.println("\n");
    Serial.println("╔════════════════════════════════╗");
    Serial.println("║    WiFi Connected Successfully ║");
    Serial.println("╠════════════════════════════════╣");
    Serial.printf("║ SSID: %-25s ║\n", WiFi.SSID().c_str());
    Serial.printf("║ IP: %-28s ║\n", WiFi.localIP().toString().c_str());
    Serial.printf("║ Signal: %-24d dBm ║\n", WiFi.RSSI());
    Serial.println("╚════════════════════════════════╝");
    Serial.println();
    
    return true;
  } else {
    DEBUG_PRINTLN("✗ WiFi connection failed");
    return false;
  }
}

// ========================
// 打印所有参数
// ========================
void printAllParams() {
  Serial.println("\n");
  Serial.println("╔════════════════════════════════════════════╗");
  Serial.println("║        Current Configuration              ║");
  Serial.println("╠════════════════════════════════════════════╣");
  
  for (auto& param : configParams) {
    String line = "║ " + param.label;
    // 填充空格
    while (line.length() < 28) line += " ";
    line += ": " + param.value;
    while (line.length() < 43) line += " ";
    line += "║";
    Serial.println(line);
  }
  
  Serial.println("╚════════════════════════════════════════════╝");
  Serial.println();
}

// ========================
// WiFi 状态检查
// ========================
bool isWiFiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

int getWiFiSignalStrength() {
  return WiFi.RSSI();
}

String getWiFiSSID() {
  return WiFi.SSID();
}

String getLocalIP() {
  return WiFi.localIP().toString();
}

// ========================
// 重置配置
// ========================
void resetConfig() {
  if (FileSystem.exists(CONFIG_FILE)) {
    FileSystem.remove(CONFIG_FILE);
    DEBUG_PRINTLN("✓ Config file deleted");
  }
  
  // 重置所有参数为默认值
  for (auto& param : configParams) {
    param.value = param.defaultValue;
    configValues[param.key] = param.defaultValue;
  }
  
  DEBUG_PRINTLN("✓ Config reset to defaults");
}

// ========================
// 列出文件系统中的文件
// ========================
void listFiles() {
  DEBUG_PRINTLN("\n=== Files in FileSystem ===");
  
  #ifdef ESP8266
    Dir dir = LittleFS.openDir("/");
    while (dir.next()) {
      DEBUG_PRINT(dir.fileName());
      DEBUG_PRINT(" - ");
      DEBUG_PRINTLN(dir.fileSize());
    }
  #elif defined(ESP32)
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
      DEBUG_PRINT(file.name());
      DEBUG_PRINT(" - ");
      DEBUG_PRINTLN(file.size());
      file = root.openNextFile();
    }
  #endif
  
  DEBUG_PRINTLN("===========================\n");
}

#endif