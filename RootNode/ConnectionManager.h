#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include "painlessMesh.h"
#include <Arduino.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <WiFiClientSecure.h>
#include <TimeLib.h>

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

void receivedCallback( uint32_t from, String &msg);
void newConnectionCallback(uint32_t nodeId);
void changedConnectionCallback();

class ConnectionManager {
  public:
    ConnectionManager();
    void connectWIFI();
    void disconnectWIFI();
    void disconnectMESH();
    void initMESH();

    void sendMeshMessage(uint32_t nodeId, String dataPacket);
    void sendBroadcast(String dataPacket);

    void update();

    bool isWIFIconnected();

  private:
    const char* SSID = "Galaxy S20+";
    const char* PASSWORD = "PASSWORD";

    bool meshIsInit = false;

    Scheduler userScheduler;
    painlessMesh  mesh;
};

#endif
