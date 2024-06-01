# ESP32-C3_Sungrow_Monitor
This project provides PlatformIO code for building a custom Wi-Fi enabled monitoring device for Sungrow solar inverters. It collects all available metrics through the inverter's RS485/MODBUS interface and sends them to InfluxDB v2 or MQTT.  
This solution completely replaces the proprietary Wi-Fi dongle which provides monitoring only through the iSolarCloud service. With this device, the inverter can be properly monitored in an isolated environment, where an internet connection isn't available, or where the security conscious owner doesn't want persistent connections from their network to external/international services through the Sungrow dongle (which also broadcasts an open Wi-Fi network).  
This has been tested on a Sungrow SG8K-D, but should work on all PV Grid-Connected String Inverters listed in the [Sungrow communication protocol documentation](https://github.com/octal-ip/ESP32-C3_Sungrow_Monitor/blob/main/Sungrow%20-%20Communication%20Protocol%20of%20PV%20Grid-Connected%20String%20Inverters_V1.1.37_EN.pdf).
  
![Grafana Dashboard](https://github.com/octal-ip/ESP32-C3_Sungrow_Monitor/blob/main/pics/Sungrow_Grafana.png "Grafana Dashboard")
The Sungrow_Grafana_Dashboard.json file can be imported into Grafana to display metrics from InfluxDB v2 as in the screenshot below.
  
  
#### Required Hardware
- An ESP32-C3 SuperMini board. They're small, cheap and well suited to this project.  
![ESP32-C3 SuperMini](https://github.com/octal-ip/ESP32-C3_Sungrow_Monitor/blob/main/pics/ESP32-C3_SuperMini.png?raw=true "ESP32-C3 SuperMini")
- A TTL to RS485 adaptor, many of which are available on eBay and Aliexpress. I recommend the type depicted below as they have built-in level shifting, flow control and transient protection.  
![TTL to RS485 Module](https://github.com/octal-ip/ESP32-C3_Sungrow_Monitor/blob/main/pics/TTL_RS485_Module.png "TTL to RS485 Module")
- An old Ethernet cable you're willing to sacrifice. This will plug into the RS485 RJ45 jack on the inverter and will also provide power.
  
  
#### Wiring and Connections
| ESP32-C3 SuperMini | TTL to RS485 adaptor |
| ------------ | ------------ |
| TX | TXD |
| RX | RXD |
| GND | GND |
| 3v3 | VCC |
  
| RJ45 connector | TTL to RS485 adaptor | ESP32-C3 SuperMini |
| ------------ | ------------ | ------------ |
| Pin 2 (T568B Orange) | GND | GND | 
| Pin 1 (T568B Orange/White) | - | 5V |
| Pin 6 (T568B Green) | A+ | - |
| Pin 3 (T568B Green/White) | B- | - |
  
  
### Credits:
[4-20ma for ModbusMaster](https://github.com/4-20ma/ModbusMaster)  
  
[RobTillaart for RunningAverage](https://github.com/RobTillaart/RunningAverage)  
  
[JAndrassy for TelnetStream](https://github.com/jandrassy/TelnetStream)  
  
[Nick O'Leary for PubSubClient](https://github.com/knolleary/pubsubclient)  
  
[Tobias Schürg for InfluxDB Client](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino/)  
  
  
### Release notes:
#### Jun 1, 2024
	- Initial release
