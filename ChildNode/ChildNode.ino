#include "painlessMesh.h"       // Библиотека для работы с mesh-сетью
#include <TimeLib.h>            // Библиотека для работы с датой и временем

// Параметры для инициализации mesh-сети
#define   MESH_PREFIX     "whateverYouLike"   // Название сети
#define   MESH_PASSWORD   "somethingSneaky"   // Пароль от сети
#define   MESH_PORT       5555                // Порт mesh-сети

Scheduler userScheduler;         // Планировщик задач (нужен для painlessMesh)
painlessMesh  mesh;              // Объект для работы с сетью

// Пример формата сообщения:
// {"node_id": "12345", "node_num": "5", "sendDateTime" : "2023-10-01 12:47:32", "setupDateTime": "2023-10-01 12:34:56"}

// Уникальный номер узла в сети
const unsigned int NODE_NUM = 5;

// Флаг, указывающий, синхронизированы ли дата и время
bool isDateTimeSynchronized = false;

// Строка с моментом включения устройства
String setUpDateTime;

// Массив для хранения распарсенной даты и времени: [день, месяц, год, час, минута, секунда]
int timeArray[6] = {0, 0, 0, 0, 0, 0};

// Перечисление кодов входящих сообщений
enum class InputCodes {
  SetDateTime = 1,   // Установка даты и времени
  Message = 2,       // Просто сообщение
};

// Перечисление кодов исходящих сообщений
enum class OutputCodes {
  NeedDateTime = 3,   // Нужно получить дату и время
  SendMessage = 4,    // Отправка сообщения
};

// Прототип функции отправки сообщения (нужно для корректной работы с PlatformIO)
void sendMessage();

// Задача для периодической отправки сообщений каждые 20 секунд
Task taskSendMessage( TASK_SECOND * 20 , TASK_FOREVER, &sendMessage );

// Получение текущей даты и времени в виде строки
String getCurrentDate() {
  String datetime = 
      String(day())     +  ":" +
      String(month())   +  ":" +
      String(year())    +  ":" +
      String(hour())    +  ":" +
      String(minute())  +  ":" +
      String(second());

  return datetime;
}

// Вычисление времени запуска устройства (на основе текущего времени и uptime)
void calculationSetUpDateTime() {
  time_t current = now();                 // Текущее время
  time_t newTime = current - (millis() / 1000);  // Отнимаем время с момента запуска

  // Формируем строку с датой и временем запуска
  setUpDateTime = 
      String(day(newTime))     +  ":" +
      String(month(newTime))   +  ":" +
      String(year(newTime))    +  ":" +
      String(hour(newTime))    +  ":" +
      String(minute(newTime))  +  ":" +
      String(second(newTime));
}

// Отправка сообщения в сеть
void sendMessage() {
  String msg = "";
  if (isDateTimeSynchronized) {
    // Отправляем полезные данные
    msg += String((int)OutputCodes::SendMessage);
    msg += 
      "\"node_num\": " + String(NODE_NUM) +
      ", \"sendDateTime\" : \"" + getCurrentDate() + "\"" +
      ", \"setupDateTime\": \"" + setUpDateTime + "\"";
  } else {
    // Запрашиваем дату и время
    msg += String((int)OutputCodes::NeedDateTime);
  }
  mesh.sendBroadcast( msg ); // Отправляем всем в сети
  taskSendMessage.setInterval(TASK_SECOND * 20); // Повтор каждые 20 сек
}

// Обработка входящих сообщений
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());

  if (msg.length() == 0) {
    Serial.println("WARNING: Сообщение пришло пустым!");
    return;
  }

  int code = msg.substring(0, 1).toInt();        // Определяем код команды
  String data = msg.substring(1, msg.length());  // Оставшиеся данные

  switch (code) {
    case ((int)InputCodes::SetDateTime): {
      // Установка даты и времени
      if (msg.length() > 1 && !isDateTimeSynchronized) {
        String dateTime = msg.substring(1, msg.length());
        Serial.printf("DEBUG MESSAGE: Пришла текущая дата: %s\n", dateTime.c_str());

        // Преобразуем строку в массив char для разбиения на токены
        char dateTimeCharArray[dateTime.length() + 1];
        dateTime.toCharArray(dateTimeCharArray, sizeof(dateTimeCharArray));
        char* token = strtok(dateTimeCharArray, ":");

        int i = 0;
        while (token != NULL) {
          timeArray[i] = atoi(token);
          token = strtok(nullptr, ":");
          i++;
        }

        // Выводим разобранные значения
        Serial.printf("%d, %d, %d, %d, %d, %d\n", timeArray[0], timeArray[1], timeArray[2], timeArray[3], timeArray[4], timeArray[5]);

        if (i == 6) {
          // Устанавливаем системное время
          setTime(timeArray[3], timeArray[4], timeArray[5], timeArray[0], timeArray[1], timeArray[2]);
        } else {
          Serial.println("ERROR: Не удалось распарсить дату полностью!");
        }

        calculationSetUpDateTime();        // Вычисляем время запуска
        isDateTimeSynchronized = true;     // Отмечаем, что синхронизация прошла
      } else {
        Serial.println("ERROR: Сообщение с датой слишком короткое!");
      }
      break;
    }
    case ((int)InputCodes::Message): {
      // Пришло обычное сообщение
      Serial.printf("DEBUG MESSAGE: Сообщение: '%s' от узла id=%u\n", msg, from);
      break;
    }
    case ((int)OutputCodes::NeedDateTime): {
      // код адресован шлюзу
      break;
    }
    case ((int)OutputCodes::SendMessage): {
      // код адресован шлюзу
      break;
    }
    default: {
      Serial.println("ERROR: Такого кода не существует!");
      break;
    }
  }
}

// Колбэк при новом соединении
void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

// Колбэк при изменении списка соединений
void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

// Инициализация узла
void setup() {
  Serial.begin(115200); // Запуск последовательного порта
  mesh.setDebugMsgTypes( ERROR | STARTUP ); // Включение отладочных сообщений

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT ); // Инициализация mesh
  mesh.setContainsRoot(true);  // Устанавливаем, что узел может быть корнем сети

  // Назначаем обработчики
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);

  // Добавляем задачу отправки сообщений в планировщик
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable(); // Включаем задачу
}

// Главный цикл
void loop() {
  mesh.update();  // Обновляем состояние mesh-сети
}
