#include "ConnectionManager.h"

/*
 * ConnectionManager - класс, менеджер подключений. Он отвечает за wifi соединение и подключение к mesh сети
 * Все подключения/отключения происходят через методы этого класса
 * реализация callback функций находятся в этом файле, их определения в ConnectionManager.h
*/

// количество сообщений, после достижения которого они будут отправлены на Яндекс Диск
const int SENT_NUMBER = 20;

// Константы для авторизации и загрузки файла на Яндекс Диск
const String TIME_AUTH_TOKEN       = "TIME_AUTH_TOKEN";
const String O_AUTH_TOKEN       = "O_AUTH_TOKEN";
const String YANDEX_DISK_SERVER = "https://cloud-api.yandex.net/v1/disk/resources/upload";
const String FILE_PATH          = "meshNetworkLOG/logfile.txt";   // путь к файлу на Яндекс Диске
const String OVERWRITE          = "true";                         // перезаписывать ли файл при загрузке

//const String DATE_TIME_PATH     = "https://www.timeapi.io/api/Time/current/zone?timeZone=Europe/Moscow"; // API для получения текущей даты и времени
const String DATE_TIME_PATH     = "https://api.api-ninjas.com/v1/worldtime?city=Moscow"; // API для получения текущей даты и времени


// переменные для накопления данных
String collectedData = "";   // строка для хранения накопленных сообщений
unsigned int dataCnt = 0;    // счётчик сообщений

// задержка между запросами времени
const unsigned int READING_DELAY = 10000; // миллисекунды
unsigned int prevMillist = 0;

// экземпляр менеджера подключений
ConnectionManager connManager;

// Коды входящих сообщений
enum class InputCodes
{
  NeedDateTime = 3, // запрос на текущую дату/время
  Message = 4       // обычное сообщение от узла
};

// Коды исходящих сообщений
enum class OutputCodes
{
  SendDateTime = 1, // отправка текущей даты/времени
  SendMessage = 2   // отправка произвольного сообщения (не используется в этом коде напрямую)
};

// получение текущей даты и времени через API
void setCurrentDateTime()
{
  connManager.connectWIFI(); // подключение к Wi-Fi
  delay(1000);

  //WiFiClientSecure client;
  //client.setInsecure();  // используем соединение без проверки сертификатов
  delay(1000);

  int httpResponseCode = 0;

  // выполняем HTTP GET запрос к API, пока не получим успешный ответ
  while (httpResponseCode <= 0)
  {
    HTTPClient http;
    //http.begin(client, DATE_TIME_PATH);
    http.begin(DATE_TIME_PATH);
    http.addHeader("X-Api-Key", TIME_AUTH_TOKEN);

    httpResponseCode = http.GET();
        
    if (httpResponseCode > 0)
    {
      Serial.printf("DEBUG MESSAGE: Успешное получение текущей даты и текущего времени, httpResponseCode=%d\n", httpResponseCode);
      String payload = http.getString();
      JSONVar myObject = JSON.parse(payload.c_str());

      // разбираем JSON и устанавливаем системное время
      int day =  myObject["day"];
      int month =  myObject["month"];
      int year =  myObject["year"];
      int hour =  myObject["hour"];
      int minute =  myObject["minute"];
      int seconds =  myObject["second"];
      //int milliSeconds =  myObject["milliSeconds"];
      Serial.printf("Дата: %s\n", myObject.c_str());
      setTime(hour, minute, seconds, day, month, year);
    }
    else
    {
      Serial.printf("ERROR: Ошибка получения текущей даты и текущего времени, httpResponseCode=%d\n", httpResponseCode);
      delay(1000);
    }
  }

  connManager.disconnectWIFI(); // отключение от Wi-Fi
}

// формирует строку с текущей датой и временем
String getCurrentDateTime()
{
  String datetime = 
      String(day())     +  ":" +
      String(month())   +  ":" +
      String(year())    +  ":" +
      String(hour())    +  ":" +
      String(minute())  +  ":" +
      String(second());
  
  return datetime;
}

// отправка текущей даты и времени указанному узлу
void sendCurrentDateTime(uint32_t nodeId)
{
  OutputCodes code = OutputCodes::SendDateTime;
  String dataPacket = String((int)code) + getCurrentDateTime();

  Serial.printf("DEBUG MESSAGE: Отправка даты и времени, datetime=%s\n", dataPacket.c_str());
  
  // можно отправить конкретному узлу, а пока используется широковещательная отправка
  // connManager.sendMeshMessage(nodeId, dataPacket);
  connManager.sendBroadcast(dataPacket);
}

// отправка накопленного лога на Яндекс Диск
void sendFileToYandexDisk()
{
  connManager.disconnectMESH(); // отключение от mesh-сети
  delay(1000);
  connManager.connectWIFI();    // подключение к Wi-Fi

  if(connManager.isWIFIconnected())
  {
    HTTPClient http;
    String serverPath = YANDEX_DISK_SERVER + "?path=" + FILE_PATH + "&overwrite=" + OVERWRITE;
    http.begin(serverPath.c_str());
    http.addHeader("Authorization","OAuth " + O_AUTH_TOKEN);

    // Отправляем GET-запрос, чтобы получить ссылку на загрузку файла
    int httpResponseCode = http.GET();
      
    if (httpResponseCode > 0)
    {
      Serial.print("HTTP GET Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);

      // разбираем JSON, получаем ссылку на загрузку (href)
      JSONVar myObject = JSON.parse(payload.c_str());
      String href = myObject["href"];

      http.end(); // закрываем предыдущее соединение
      http.begin(href.c_str()); // начинаем новое соединение для PUT-загрузки
      http.addHeader("Content-Type", "text/plain");

      // отправка содержимого collectedData методом PUT
      int httpResponseCode = http.PUT((uint8_t*)collectedData.c_str(), collectedData.length());

      Serial.print("HTTP PUT Response code: ");
      Serial.println(httpResponseCode);
      payload = http.getString();
      Serial.println(payload);
    }
    else
    {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
  else
  {
    Serial.println("WiFi Disconnected");
  }

  connManager.disconnectWIFI(); // отключение от Wi-Fi
  delay(1000);
  connManager.initMESH();       // повторное подключение к mesh-сети
}

// колбэк — вызывается при получении сообщения из сети mesh
void receivedCallback(uint32_t from, String &msg)
{
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());

  String getDateTime = getCurrentDateTime(); // получаем текущее время для лога

  if (msg.length() == 0)
  {
    Serial.println("WARNING: Сообщение пришло пустым!");
    return;
  }

  // первый символ сообщения — это код запроса
  int inputCode = msg.substring(0, 1).toInt();

  switch (inputCode)
  {
    case ((int)InputCodes::NeedDateTime):
    {
      // если прошло достаточно времени, отправляем текущую дату и время
      unsigned int currentMillis = millis();
      if (currentMillis - prevMillist > READING_DELAY)
      {
        sendCurrentDateTime(from);
        prevMillist = currentMillis;
      }
      break;
    }
    case ((int)InputCodes::Message):
    {
      if (msg.length() > 1)
      {
        // извлекаем данные из сообщения и сохраняем в лог
        String data = msg.substring(1, msg.length());
        collectedData += "{node_id : " + String(from) + ", " + "getDateTime: " + getDateTime + ", " + data + "}\n";
        dataCnt++;

        // если накоплено нужное количество сообщений, отправляем на диск
        if (dataCnt == SENT_NUMBER)
        {
          sendFileToYandexDisk();
          dataCnt = 0;
        }
        break;
      }
      else
      {
        Serial.println("WARNING: Сообщение не содержит ничего кроме кода!");
      }
    }
    default:
    {
      Serial.println("ERROR: Такого кода не существует!");
    }
  }
}

// колбэк при новом подключении узла
void newConnectionCallback(uint32_t nodeId) {
  Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

// колбэк при изменении подключения
void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

// начальная инициализация
void setup()
{
  Serial.begin(115200);  
  setCurrentDateTime();     // получаем и устанавливаем текущее время
  connManager.initMESH();   // инициализация mesh-сети
}

// основной цикл
void loop()
{
  connManager.update(); // обновление состояния подключения
}