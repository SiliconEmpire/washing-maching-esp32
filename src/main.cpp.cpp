#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <time.h>
#include <SPIFFS.h>

int CCpin = 4;
int CCWpin = 16;
String SETTINGS_FILE = "/settings.txt";
String dataPacket;

int circles = 20;
bool status = 0;
int remainingCircles = 0;
int pausePeriod = 0;
int ccwPeriod = 0;
int ccPeriod = 0;

BLEServer *pServer = NULL;
BLEService *pService = NULL;
BLECharacteristic *pNotifyCharacteristic = NULL;
BLECharacteristic *pWriteCharacteristic = NULL;

String outJsonString;

void turnCC()
{
  digitalWrite(CCpin, LOW);
  delay(ccPeriod * 1000);
  digitalWrite(CCpin, HIGH);
}

void turnCCW()
{
  digitalWrite(CCWpin, LOW);
  delay(ccwPeriod * 1000);
  digitalWrite(CCWpin, HIGH);
}

void runMachine()
{
  turnCC();
  delay(pausePeriod * 1000);
  turnCCW();
}

void readSpiffs()
{
  Serial.print("in read spiff++++++++:");
  // Read the settings from the file
  File file = SPIFFS.open(SETTINGS_FILE, "r");
  if (file)
  {
    dataPacket = file.readString();
    // Serial.print("data from spiff:");
    // Serial.println(dataPacket);
    file.close();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, dataPacket);
    if (!error)
    {

      circles = doc["circles"];
      status = doc["status"];
      remainingCircles = doc["remainingCircles"];
      pausePeriod = doc["pausePeriod"];
      ccwPeriod = doc["ccwPeriod"];
      ccPeriod = doc["ccPeriod"];

      Serial.print("--->pausePeriod:");
      Serial.println(pausePeriod);
      Serial.print("--->circles:");
      Serial.println(circles);
      Serial.print("--->remainingCircles:");
      Serial.println(remainingCircles);

      delay(500);
    }
    else
    {
      Serial.println("Failed to parse JSON data");
      delay(5000);
    }
  }
  else
  {
    Serial.println("Using default settings");
  }
}

void writeSpiffs(String data_to_store)
{
  File file = SPIFFS.open(SETTINGS_FILE, "w");
  if (file)
  {
    file.println(data_to_store);
    file.close();
  }
  else
  {
    Serial.println("Failed to save settings to file");
  }
}

class CharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    int circle_s;
    bool statu_s;
    int pausePerio_d;
    int ccwPerio_d;
    int ccPerio_d;

    String inJsonString;

    std::string value = pCharacteristic->getValue();
    Serial.print(" raw Received value: ");
    Serial.println(value.c_str());
    DynamicJsonDocument doc(1024);
    // {"circles":32,"pausePeriod":5,"ccwPeriod":5,"ccPeriod":1,"status":false}

    DeserializationError error = deserializeJson(doc, value);
    if (!error)
    {
      circle_s = doc["circles"];
      statu_s = doc["status"];
      pausePerio_d = doc["pausePeriod"];
      ccwPerio_d = doc["ccwPeriod"];
      ccPerio_d = doc["ccPeriod"];
    }

    DynamicJsonDocument inJsonDoc(1024);

    inJsonDoc["circles"] = circle_s;
    inJsonDoc["status"] = statu_s;
    inJsonDoc["remainingCircles"] = remainingCircles;
    inJsonDoc["pausePeriod"] = pausePerio_d;
    inJsonDoc["ccwPeriod"] = ccwPerio_d;
    inJsonDoc["ccPeriod"] = ccPerio_d;
    serializeJson(inJsonDoc, inJsonString);

    writeSpiffs(inJsonString);
    delay(500);
    readSpiffs();
  }
};

class ServerCallbacks : public BLEServerCallbacks
{
  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Device disconnected");
    pServer->getAdvertising()->start();
  }
};

void setup()
{

  pinMode(CCpin, OUTPUT);
  pinMode(CCWpin, OUTPUT);
  digitalWrite(CCpin, HIGH);
  digitalWrite(CCWpin, HIGH);
  Serial.begin(115200);

  SPIFFS.begin();

  BLEDevice::init("Smart Washing Machine");
  pServer = BLEDevice::createServer();
  pService = pServer->createService("0000FF13-0000-1000-8000-00805F9B34FB");

  pNotifyCharacteristic = pService->createCharacteristic(
      BLEUUID("0000FF14-0000-1000-8000-00805F9B34FB"),
      BLECharacteristic::PROPERTY_NOTIFY);
  pWriteCharacteristic = pService->createCharacteristic(
      BLEUUID("0000FF15-0000-1000-8000-00805F9B34FB"),
      BLECharacteristic::PROPERTY_WRITE);

  pServer->setCallbacks(new ServerCallbacks());

  pWriteCharacteristic->setCallbacks(new CharacteristicCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  // writeSpiffs(" {\"circles\":30,\"status\":true,\"remainingCircles\":0,\"pausePeriod\":0,\"ccwPeriod\":0,\"ccPeriod\":0} ");
  readSpiffs();
}

void pushBleNotification()
{
  DynamicJsonDocument outJsonDoc(1024);

  outJsonDoc["status"] = status;
  outJsonDoc["remainingCircles"] = remainingCircles;

  serializeJson(outJsonDoc, outJsonString);
  Serial.print("--->outJsonString:");
  Serial.println(outJsonString);
  pNotifyCharacteristic->setValue(outJsonString.c_str());
  pNotifyCharacteristic->notify();
}

void loop()
{
  // readSpiffs();

  if (remainingCircles < circles && status == true)
  {
    runMachine();

    remainingCircles++;

    pushBleNotification();

    writeSpiffs(outJsonString);
    outJsonString = "";
    delay(500);
  }
  else
  {

    // Serial.println("<-----------------------");

    // remainingCircles = 0;
    // circles = 0;
    status = false;
    if (remainingCircles >= circles)
    {
      remainingCircles = 0;
    }

    pushBleNotification();
    writeSpiffs(outJsonString);
    outJsonString = "";
    delay(500);
  }

  delay(1000);
}