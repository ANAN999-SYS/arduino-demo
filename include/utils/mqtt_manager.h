// include/mqtt_manager.h
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "wifi_config.h"

// ========================
// MQTT 回调函数类型定义
// ========================
typedef void (*CommandCallback)(const char* command, JsonDocument& payload);
typedef void (*MessageCallback)(const char* topic, const char* message);

// ========================
// MQTT 主题结构体
// ========================
struct MQTTTopic {
  String name;                    // 主题名称
  CommandCallback onCommand;      // 命令回调
  MessageCallback onMessage;      // 消息回调
};

// ========================
// MQTT 设备状态结构体
// ========================
struct DeviceStatus {
  String deviceId;
  String chipType;
  bool isConnected;
  uint32_t lastUpdateTime;
  uint32_t uptime;
  int signalStrength;
  String ipAddress;
};

// ========================
// MQTT 管理器类
// ========================
class MQTTManager {
private:
  PubSubClient* mqttClient;
  std::vector<MQTTTopic> topics;
  DeviceStatus deviceStatus;
  
  String baseTopicPrefix;         // 主题前缀，如 "home"
  String deviceId;
  uint32_t lastStatusPublish;     // 上次状态发布时间
  uint32_t statusPublishInterval; // 状态发布间隔（毫秒）
  bool autoStatusReport;          // 自动状态上报
  bool debugEnabled;              // 调试模式

public:
  // ========================
  // 构造函数
  // ========================
  MQTTManager(PubSubClient* client, const char* deviceId, const char* prefix = "home") {
    mqttClient = client;
    this->deviceId = String(deviceId);
    baseTopicPrefix = String(prefix);
    lastStatusPublish = 0;
    statusPublishInterval = 30000;  // 默认 30 秒
    autoStatusReport = true;
    debugEnabled = true;
    
    deviceStatus.deviceId = this->deviceId;
    deviceStatus.chipType = CHIP_TYPE;
    deviceStatus.isConnected = false;
    deviceStatus.uptime = 0;
    
    // 设置 MQTT 回调
    if (mqttClient) {
      mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
        this->onMqttMessage(topic, payload, length);
      });
    }
  }

  // ========================
  // 注册主题和回调
  // ========================
  void registerTopic(const char* topicName, CommandCallback cmdCallback = nullptr, MessageCallback msgCallback = nullptr) {
    MQTTTopic newTopic;
    newTopic.name = String(topicName);
    newTopic.onCommand = cmdCallback;
    newTopic.onMessage = msgCallback;
    
    topics.push_back(newTopic);
    
    if (debugEnabled) {
      Serial.println("✓ Registered topic: " + String(topicName));
    }
  }

  // ========================
  // 注销主题
  // ========================
  void unregisterTopic(const char* topicName) {
    for (auto it = topics.begin(); it != topics.end(); ++it) {
      if (it->name == topicName) {
        topics.erase(it);
        if (debugEnabled) {
          Serial.println("✗ Unregistered topic: " + String(topicName));
        }
        return;
      }
    }
  }

  // ========================
  // 连接 MQTT 服务器
  // ========================
  bool connect() {
    if (!mqttClient) {
      if (debugEnabled) Serial.println("✗ MQTT Client not initialized");
      return false;
    }

    String server = getConfigValue("mqtt_server");
    int port = getConfigValue("mqtt_port").toInt();
    String username = getConfigValue("mqtt_user");
    String password = getConfigValue("mqtt_pass");

    if (debugEnabled) {
      Serial.println("\nConnecting to MQTT server...");
      Serial.printf("Server: %s:%d\n", server.c_str(), port);
    }

    // 连接 MQTT 服务器
    bool connected = false;
    if (username.length() > 0 && password.length() > 0) {
      connected = mqttClient->connect(deviceId.c_str(), username.c_str(), password.c_str());
    } else {
      connected = mqttClient->connect(deviceId.c_str());
    }

    if (connected) {
      if (debugEnabled) {
        Serial.println("✓ MQTT Connected");
      }
      
      deviceStatus.isConnected = true;
      deviceStatus.lastUpdateTime = millis();
      
      // 订阅所有注册的主题
      subscribeToAllTopics();
      
      // 发布上线消息
      publishOnlineStatus();
      
      return true;
    } else {
      if (debugEnabled) {
        Serial.print("✗ MQTT Connect failed: ");
        Serial.println(mqttClient->state());
      }
      deviceStatus.isConnected = false;
      return false;
    }
  }

  // ========================
  // 断开连接
  // ========================
  void disconnect() {
    if (mqttClient && mqttClient->connected()) {
      publishOfflineStatus();
      mqttClient->disconnect();
      deviceStatus.isConnected = false;
      
      if (debugEnabled) {
        Serial.println("✓ MQTT Disconnected");
      }
    }
  }

  // ========================
  // 检查连接状态
  // ========================
  bool isConnected() {
    return mqttClient && mqttClient->connected();
  }

  // ========================
  // 保持连接（在 loop 中调用）
  // ========================
  void loop() {
    if (!mqttClient) return;

    if (!mqttClient->connected()) {
      connect();
    } else {
      mqttClient->loop();
      
      // 自动发布状态
      if (autoStatusReport && (millis() - lastStatusPublish >= statusPublishInterval)) {
        publishStatus();
        lastStatusPublish = millis();
      }
    }
  }

  // ========================
  // 设置状态发布间隔
  // ========================
  void setStatusPublishInterval(uint32_t intervalMs) {
    statusPublishInterval = intervalMs;
  }

  // ========================
  // 启用/禁用自动状态上报
  // ========================
  void setAutoStatusReport(bool enabled) {
    autoStatusReport = enabled;
  }

  // ========================
  // 更新设备状态
  // ========================
  void updateStatus(const DeviceStatus& status) {
    deviceStatus = status;
    deviceStatus.uptime = millis() / 1000;
    deviceStatus.signalStrength = getWiFiSignalStrength();
    deviceStatus.ipAddress = getLocalIP();
    deviceStatus.lastUpdateTime = millis();
  }

  // ========================
  // 发布自定义消息
  // ========================
  bool publish(const char* topic, const char* message) {
    if (!isConnected()) {
      if (debugEnabled) Serial.println("✗ MQTT not connected");
      return false;
    }

    String fullTopic = buildTopic(topic);
    bool result = mqttClient->publish(fullTopic.c_str(), message);
    
    if (debugEnabled) {
      Serial.printf("✓ Published to %s: %s\n", fullTopic.c_str(), message);
    }
    
    return result;
  }

  // ========================
  // 发布 JSON 消息
  // ========================
  bool publishJson(const char* topic, JsonDocument& doc) {
    String message;
    serializeJson(doc, message);
    return publish(topic, message.c_str());
  }

  // ========================
  // 发布设备状态
  // ========================
  bool publishStatus() {
    if (!isConnected()) {
      return false;
    }

    DynamicJsonDocument doc(512);
    
    doc["device_id"] = deviceStatus.deviceId;
    doc["chip_type"] = deviceStatus.chipType;
    doc["is_connected"] = deviceStatus.isConnected;
    doc["uptime"] = deviceStatus.uptime;
    doc["signal_strength"] = deviceStatus.signalStrength;
    doc["ip_address"] = deviceStatus.ipAddress;
    doc["timestamp"] = millis();

    String fullTopic = baseTopicPrefix + "/" + deviceStatus.deviceId + "/status";
    return publishJson(fullTopic.c_str(), doc);
  }

  // ========================
  // 发布上线消息
  // ========================
  void publishOnlineStatus() {
    DynamicJsonDocument doc(256);
    doc["device_id"] = deviceStatus.deviceId;
    doc["status"] = "online";
    doc["timestamp"] = millis();
    doc["ip_address"] = getLocalIP();
    
    String fullTopic = baseTopicPrefix + "/" + deviceStatus.deviceId + "/online";
    publishJson(fullTopic.c_str(), doc);
  }

  // ========================
  // 发布离线消息
  // ========================
  void publishOfflineStatus() {
    DynamicJsonDocument doc(256);
    doc["device_id"] = deviceStatus.deviceId;
    doc["status"] = "offline";
    doc["timestamp"] = millis();
    
    String fullTopic = baseTopicPrefix + "/" + deviceStatus.deviceId + "/offline";
    // 离线消息可以设置 MQTT 遗嘱，这里简单发布
    mqttClient->publish(fullTopic.c_str(), "{\"status\":\"offline\"}");
  }

  // ========================
  // 发送命令执行结果
  // ========================
  bool publishCommandResponse(const char* command, bool success, const char* message = "") {
    if (!isConnected()) return false;

    DynamicJsonDocument doc(256);
    doc["command"] = command;
    doc["success"] = success;
    doc["message"] = message;
    doc["timestamp"] = millis();
    
    String fullTopic = baseTopicPrefix + "/" + deviceStatus.deviceId + "/response";
    return publishJson(fullTopic.c_str(), doc);
  }

  // ========================
  // 打印所有订阅的主题
  // ========================
  void printSubscribedTopics() {
    Serial.println("\n╔════════════════════════════════╗");
    Serial.println("║  Subscribed MQTT Topics        ║");
    Serial.println("╠════════════════════════════════╣");
    
    for (auto& topic : topics) {
      String line = "║ " + topic.name;
      while (line.length() < 33) line += " ";
      line += "║";
      Serial.println(line);
    }
    
    Serial.println("╚════════════════════════════════╝\n");
  }

  // ========================
  // 打印设备状态
  // ========================
  void printStatus() {
    Serial.println("\n╔════════════════════════════════╗");
    Serial.println("║     Device MQTT Status         ║");
    Serial.println("╠════════════════════════════════╣");
    Serial.printf("║ Device ID: %-20s ║\n", deviceStatus.deviceId.c_str());
    Serial.printf("║ Status: %-25s ║\n", isConnected() ? "Connected" : "Disconnected");
    Serial.printf("║ Uptime: %-23lds ║\n", deviceStatus.uptime);
    Serial.printf("║ Signal: %-25d ║\n", deviceStatus.signalStrength);
    Serial.printf("║ IP: %-27s ║\n", deviceStatus.ipAddress.c_str());
    
    if (deviceStatus.temperature != 0.0) {
      Serial.printf("║ Temperature: %-17.1f ║\n", deviceStatus.temperature);
    }
    if (deviceStatus.humidity != 0.0) {
      Serial.printf("║ Humidity: %-20.1f ║\n", deviceStatus.humidity);
    }
    if (deviceStatus.lightLevel != 0) {
      Serial.printf("║ Light Level: %-18d ║\n", deviceStatus.lightLevel);
    }
    
    Serial.println("╚════════════════════════════════╝\n");
  }

  // ========================
  // 启用/禁用调试
  // ========================
  void setDebug(bool enabled) {
    debugEnabled = enabled;
  }

private:
  // ========================
  // MQTT 消息处理（内部）
  // ========================
  void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    char message[256];
    snprintf(message, sizeof(message), "%.*s", (int)length, (char*)payload);
    
    if (debugEnabled) {
      Serial.printf("Message from topic [%s]: %s\n", topic, message);
    }

    // 解析 JSON
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
      if (debugEnabled) {
        Serial.println("✗ Failed to parse JSON");
      }
      return;
    }

    // 匹配主题并调用回调
    for (auto& t : topics) {
      String fullTopic = buildTopic(t.name.c_str());
      if (strcmp(topic, fullTopic.c_str()) == 0) {
        // 如果有 command 字段，调用 command 回调
        if (doc.containsKey("command") && t.onCommand != nullptr) {
          String command = doc["command"].as<String>();
          t.onCommand(command.c_str(), doc);
        }
        // 否则调用 message 回调
        else if (t.onMessage != nullptr) {
          t.onMessage(topic, message);
        }
        return;
      }
    }
  }

  // ========================
  // 订阅所有注册的主题
  // ========================
  void subscribeToAllTopics() {
    for (auto& topic : topics) {
      String fullTopic = buildTopic(topic.name.c_str());
      if (mqttClient->subscribe(fullTopic.c_str())) {
        if (debugEnabled) {
          Serial.println("✓ Subscribed to: " + fullTopic);
        }
      } else {
        if (debugEnabled) {
          Serial.println("✗ Subscribe failed: " + fullTopic);
        }
      }
    }
  }

  // ========================
  // 构建完整的主题名
  // ========================
  String buildTopic(const char* subTopic) {
    return baseTopicPrefix + "/" + deviceStatus.deviceId + "/" + String(subTopic);
  }
};

#endif