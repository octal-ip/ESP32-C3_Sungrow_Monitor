#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ModbusMaster.h> //Note that the maximum buffer size (ku8MaxBufferSize) must be increased to 128 in ModbusMaster.h
#include <RunningAverage.h>
#include <InfluxDbClient.h>
#include <TelnetStream.h>

#include <secrets.h> /*Edit this file to include the following details.
#define SECRET_SSID "your_ssid"
#define SECRET_PASS "your_password"
#define SECRET_INFLUXDB_URL "http://10.x.x.x:8086"
#define SECRET_INFLUXDB_TOKEN "your_token"
#define SECRET_INFLUXDB_ORG "default"
#define SECRET_INFLUXDB_BUCKET "your_bucket"
#define SECRET_MQTT_SERVER "10.x.x.x"
#define SECRET_MQTT_USER "your_MQTT_username"
#define SECRET_MQTT_PASS "your_MQTT_password"
*/

#define MQTT
#define debugEnabled 0

#ifdef MQTT
  #include <PubSubClient.h>
  WiFiClient espClient;
  PubSubClient MQTTclient(espClient);
  const char *MQTTclientId = "Sungrow-SG8K-D";
  const char *MQTTtopicPrefix = "home/solar/sungrow";
  char MQTTtopic[70];
  char statString[20];
#endif

InfluxDBClient InfluxDBclient(SECRET_INFLUXDB_URL, SECRET_INFLUXDB_ORG, SECRET_INFLUXDB_BUCKET, SECRET_INFLUXDB_TOKEN);

const char* hostname = "ESP32-C3-Sungrow-Monitor";

byte avSamples = 60;
int failures = 0; //The number of failed operations. Will automatically restart the ESP if too many failures occurr in a row.

struct stats{
   const char *name;
   byte address;
   int type; //Whether the result is 16 or 32 bit number.
   float value;
   RunningAverage average;
   float multiplier;
   Point measurement;
   bool sendAverage;
};

#define REALTIMESTATS 30
stats arrstats[REALTIMESTATS] = {
	//Register name, MODBUS address, integer type (0 = uint16_t, 1 = uint32_t, 3 = int32_t), value, running average, multiplier, InfluxDB measurement, whether to send average or most current value)
	{"Nominal_output_power", 0, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Nominal output power"), 0},
	{"Output_type", 1, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Output type"), 0},
	{"Daily_power", 2, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Daily power"), 0},
	{"Total_power_yield", 3, 1, 0.0, RunningAverage(avSamples), 1.0, Point("Total power yield"), 0}, //U32
	{"Total_running_time", 5, 1, 0.0, RunningAverage(avSamples), 1.0, Point("Total running time"), 0}, //U32
	{"Internal_temperature", 7, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Internal temperature"), 1},
	{"DC_voltage_of_MPPT1", 10, 0, 0.0, RunningAverage(avSamples), 0.1, Point("DC voltage of MPPT1"), 1},
	{"DC_current_of_MPPT1", 11, 0, 0.0, RunningAverage(avSamples), 0.1, Point("DC current of MPPT1"), 1},
	{"DC_voltage_of_MPPT2", 12, 0, 0.0, RunningAverage(avSamples), 0.1, Point("DC voltage of MPPT2"), 1},
	{"DC_current_of_MPPT2", 13, 0, 0.0, RunningAverage(avSamples), 0.1, Point("DC current of MPPT2"), 1},
	{"Total_DC_power", 16, 1, 0.0, RunningAverage(avSamples), 1.0, Point("Total DC power"), 1},
	{"Phase_A_voltage", 18, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Phase A voltage"), 1},
	{"Phase_A_current", 21, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Phase A current"), 1},
	{"Total_active_power", 30, 1, 0.0, RunningAverage(avSamples), 1.0, Point("Total active power"), 1}, //U32
	{"Total_reactive_power", 32, 2, 0.0, RunningAverage(avSamples), 1.0, Point("Total reactive power"), 1}, //S32
	{"Power_factor", 34, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Power factor"), 1}, //S16
	{"Grid_frequency", 35, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Grid frequency"), 1},
	{"Work_state", 37, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Work state"), 0},
	{"Fault_year", 38, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault year"), 0},
	{"Fault_month", 39, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault month"), 0},
	{"Fault_day", 40, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault day"), 0},
	{"Fault_hour", 41, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault hour"), 0},
	{"Fault_minute", 42, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault minute"), 0},
	{"Fault_second", 43, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault second"), 0},
	{"Fault_code", 44, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Fault code"), 0},
	{"Nominal_reactive_output_power", 48, 0, 0.0, RunningAverage(avSamples), 0.1, Point("Nominal reactive output power"), 1},
	{"Impedance_to_ground", 70, 0, 0.0, RunningAverage(avSamples), 1.0, Point("Impedance to ground"), 1},
	{"Work_state_2", 80, 1, 0.0, RunningAverage(avSamples), 1.0, Point("Work state 2"), 0},
	{"Feeder_power", 82, 2, 0.0, RunningAverage(avSamples), 1.0, Point("Feeder power"), 1}, //S32
	{"Load_power", 90, 2, 0.0, RunningAverage(avSamples), 1.0, Point("Load power"), 1} //S32
};

unsigned long lastUpdate = 0;

// Create the ModbusMaster object
ModbusMaster Sungrow;
uint8_t MODBUSresult;

void postTransmission(){
  delay(9); //Wait for for the data to be sent through the serial interface.
}

void setup()
{
  Serial.begin(9600);

  // ****Start ESP32 OTA and Wifi Configuration****
  if (debugEnabled == 1) {
    Serial.println();
    Serial.print("Connecting to "); Serial.println(SECRET_SSID);
  }
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.begin(SECRET_SSID, SECRET_PASS); //Edit include/secrets.h to update this data.

  unsigned long connectTime = millis();
  if (debugEnabled == 1) {
    Serial.print("Waiting for WiFi to connect");
  }
  while (!WiFi.isConnected() && (unsigned long)(millis() - connectTime) < 5000) { //Wait for the wifi to connect for up to 5 seconds.
    delay(100);
    if (debugEnabled == 1) {
      Serial.print(".");
    }
  }
  if (!WiFi.isConnected()) {
    if (debugEnabled == 1) {
      Serial.println();
      Serial.println("WiFi didn't connect, restarting...");
    }
    ESP.restart(); //Restart if the WiFi still hasn't connected.
  }

  if (debugEnabled == 1) {
    Serial.println();
    Serial.print("IP address: "); Serial.println(WiFi.localIP());
  }

  // Port defaults to 3232
  ArduinoOTA.setPort(3232);

  ArduinoOTA.setHostname(hostname);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  // ****End ESP32 OTA and Wifi Configuration****

  // Sungrow Device ID 1
  Sungrow.begin(1, Serial);

  #ifdef MQTT
    MQTTclient.setServer(SECRET_MQTT_SERVER, 1883);
  #endif

  // Callbacks for MODBUS transmission wait.
  Sungrow.postTransmission(postTransmission);

  // Telnet log is accessible at port 23
  TelnetStream.begin();

  // Check the InfluxDB connection
  if (InfluxDBclient.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(InfluxDBclient.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(InfluxDBclient.getLastErrorMessage());
    Serial.print("Restarting...");
    ESP.restart();
  }
}


void readMODBUS() {
  Serial.flush(); //Make sure the hardware serial buffer is empty before communicating over MODBUS.
  Sungrow.clearResponseBuffer();
  MODBUSresult = Sungrow.readInputRegisters(0x1388, 105);
  
  if (MODBUSresult == Sungrow.ku8MBSuccess) {
    if (failures >= 1) {
        failures--; //Decrement the failure counter if we've received a response.
    }
    
    /*
    TelnetStream.print("Full MODBUS response: ");
    for (int r = 0; r < 105; r++) {
      TelnetStream.print(Sungrow.getResponseBuffer(r));
    }
    TelnetStream.println();
    */

    for (int i = 0; i < REALTIMESTATS; i++) { //Iterate through each of the MODBUS queries and obtain their values.
      ArduinoOTA.handle();
    
      if (arrstats[i].type == 0) { //U16
        //TelnetStream.print("Raw MODBUS for address: "); TelnetStream.print(arrstats[i].address); TelnetStream.print(": "); TelnetStream.println(Growatt.getResponseBuffer(0));
        arrstats[i].value = (Sungrow.getResponseBuffer(arrstats[i].address) * arrstats[i].multiplier);
      }
      else if (arrstats[i].type == 1) { //U32
        arrstats[i].value = ((Sungrow.getResponseBuffer(arrstats[i].address+1) << 8) | Sungrow.getResponseBuffer(arrstats[i].address)) * arrstats[i].multiplier;
      }
      else if (arrstats[i].type == 2) { //S32
        arrstats[i].value = (Sungrow.getResponseBuffer(arrstats[i].address) + (Sungrow.getResponseBuffer(arrstats[i].address+1) << 16)) * arrstats[i].multiplier;
      }

      //Do a sanity check on the feeder and load power results, as they return very high and incorrect values if AC power is interrupted.
      if ((arrstats[i].address == 82  || arrstats[i].address == 90) && arrstats[i].value > 200000000) {
        arrstats[i].value = 0;
      }
      
      TelnetStream.print(arrstats[i].name); TelnetStream.print(": "); TelnetStream.println(arrstats[i].value);
      arrstats[i].average.addValue(arrstats[i].value); //Add the value to the running average.
      //TelnetStream.print("Values collected: "); TelnetStream.println(arrstats[i].average.getCount());

      if (arrstats[i].average.getCount() >= avSamples) { //If we have enough samples added to the running average, send the data to InfluxDB and clear the average.
        arrstats[i].measurement.clearFields();
        arrstats[i].measurement.clearTags();
        arrstats[i].measurement.addTag("sensor", "Sungrow-SG8K-D");

        if (arrstats[i].sendAverage == 1) arrstats[i].measurement.addField("value", arrstats[i].average.getAverage());
        else arrstats[i].measurement.addField("value", arrstats[i].value); //Send the current value for measurements where it doesn't make sense to provide an average.

        TelnetStream.print("Sending data to InfluxDB: ");
        TelnetStream.println(InfluxDBclient.pointToLineProtocol(arrstats[i].measurement));
        
        if (!InfluxDBclient.writePoint(arrstats[i].measurement)) { //Write the data point to InfluxDB
          failures++;
          TelnetStream.print("InfluxDB write failed: ");
          TelnetStream.println(InfluxDBclient.getLastErrorMessage());
        }
        else if (failures >= 1) failures --;
        
        arrstats[i].average.clear();
      }
  
      #ifdef MQTT
        sprintf(MQTTtopic, "%s/%s", MQTTtopicPrefix, arrstats[i].name);
        if (MQTTclient.connect(MQTTclientId, SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
          dtostrf(arrstats[i].value, 1, 2, statString);
          TelnetStream.printf("Posting %s to MQTT topic %s \r\n", statString, MQTTtopic);
          MQTTclient.publish(MQTTtopic, statString, (bool)1);
          
          if (failures >= 1) failures--;
        }
        else {
          TelnetStream.print("MQTT connection failed: "); TelnetStream.println(MQTTclient.state());
          failures++;
        }
      #endif
      yield();
    }
  }
  else {
    TelnetStream.print("MODBUS read failed. Returned value: "); TelnetStream.println(MODBUSresult);
    failures++;
    TelnetStream.print("Failure counter: "); TelnetStream.println(failures);
  }
}


void loop()
{
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    if (debugEnabled == 1) {
      Serial.println("WiFi disconnected. Attempting to reconnect... ");
    }
    failures++;
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(1000);
  }

  if ((unsigned long)(millis() - lastUpdate) >= 2000) { //Every x seconds.
  
    TelnetStream.printf("\r\n\r\nWiFi signal strength is: %d\r\n", WiFi.RSSI());

    TelnetStream.println("Reading the MODBUS...");
    readMODBUS();
    lastUpdate = millis();

    #ifdef MQTT
      MQTTclient.loop();
      if (!MQTTclient.connected()) {
        TelnetStream.println("MQTT disconnected. Attempting to reconnect..."); 
        if (MQTTclient.connect(MQTTclientId, SECRET_MQTT_USER, SECRET_MQTT_PASS)) {
          if (failures >= 1) failures--;
          TelnetStream.println("MQTT Connected.");
        }
        else {
          TelnetStream.print("MQTT connection failed: "); TelnetStream.println(MQTTclient.state());
          failures++;
        }
      }
    #endif
  }

  if (failures >= 20) {
    if (debugEnabled == 1) {
      Serial.print("Too many failures, rebooting...");
    }
    TelnetStream.print("Failure counter has reached: "); TelnetStream.print(failures); TelnetStream.println(". Rebooting...");
    delay(1);
    ESP.restart(); //Reboot the ESP if too many problems have been counted.
  }
}