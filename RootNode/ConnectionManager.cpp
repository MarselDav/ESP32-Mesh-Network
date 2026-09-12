#include "ConnectionManager.h"

// Конструктор класса ConnectionManager
ConnectionManager::ConnectionManager(){}

// Метод подключения к WiFi
void ConnectionManager::connectWIFI()
{
  if (meshIsInit) // Проверяем, активна ли mesh-сеть
  {
    Serial.println("ERROR: В данный момент запущена mesh сеть, невозможно подключиться к WIFI");
    return; // Если mesh активна — выходим из метода
  }

  WiFi.begin(SSID, PASSWORD); // Начинаем подключение к WiFi по заданному SSID и паролю
  Serial.println("DEBUG MESSAGE: Connecting to WiFi:");

  // Ожидаем подключения
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("DEBUG MESSAGE: Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP()); // Выводим IP-адрес, выданный устройству
}

// Метод отключения от WiFi
void ConnectionManager::disconnectWIFI()
{
  if (WiFi.status() == WL_CONNECTED) // Проверяем, подключены ли к WiFi
  {
    WiFi.disconnect(true); // Полностью сбрасываем соединение WiFi
    WiFi.mode(WIFI_OFF);   // Выключаем WiFi-модуль
    Serial.println("DEBUG MESSAGE: WIFI отключён");
  }
  else
  {
    Serial.println("WARNING: WIFI уже отключён");
  }
}

// Метод отключения от mesh-сети
void ConnectionManager::disconnectMESH()
{
  meshIsInit = false; // Отмечаем, что mesh больше не инициализирован

  Serial.println("disconnecting MESH....");

  mesh.stop();  // Останавливаем работу mesh-сети
  delay(1000);

  disconnectWIFI(); // Также выключаем WiFi-модуль
  delay(1000);

  Serial.println("MESH is disconnected....");
}

// Метод инициализации mesh-сети
void ConnectionManager::initMESH()
{
  if (isWIFIconnected()) // Проверяем, подключены ли к WiFi
  {
    Serial.println("ERROR: В данный момент работает WIFI, невозможно подключиться к mesh сети");
    return;
  }

  mesh.setDebugMsgTypes( ERROR | STARTUP );  // Включаем отладочные сообщения
  mesh.setRoot(true);              // Указываем, что это корневой узел сети
  mesh.setContainsRoot(true);     // Указываем, что сеть содержит корень

  // Инициализируем mesh-сеть с заданными параметрами
  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );

  // Привязываем callback-функции для обработки событий сети
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  meshIsInit = true; // Отмечаем, что mesh инициализирован
}

// Метод для отправки сообщения конкретному узлу в mesh-сети
void ConnectionManager::sendMeshMessage(uint32_t nodeId, String dataPacket)
{
  mesh.sendSingle(nodeId, dataPacket); // Отправляем пакет одному узлу
}

// Метод для широковещательной отправки сообщения всем узлам сети
void ConnectionManager::sendBroadcast(String dataPacket)
{
  mesh.sendBroadcast(dataPacket); // Рассылаем сообщение всем
}

// Метод, который необходимо вызывать в loop для обновления состояния сети
void ConnectionManager::update()
{
  if (meshIsInit) // Проверка, активна ли mesh-сеть
    mesh.update(); // Обновление состояния
}

// Метод, возвращающий true, если WiFi подключён
bool ConnectionManager::isWIFIconnected()
{
  return WiFi.status() == WL_CONNECTED; // Проверка статуса WiFi
}
