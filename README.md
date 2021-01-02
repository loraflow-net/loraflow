# LoraFlow is JSON based messaging system for LoRa modules
* Single channel
* Simple addressing
* Low power consumption (only 0.15mA with Arduino mini)
![GreenHouse example](https://github.com/loraflow-net/loraflow/blob/master/img/greenhouse.png)

## Installation
0) **Type “sudo raspi-config” and enable serial communication under  “Interfacing Options” > Serial**
1) git clone https://github.com/loraflow-net/loraflow.git
2) sudo pip3 install -r loraflow/requirements.txt
3) chmod +x loraflow/install
4) ./loraflow/install
5) "sudo nano -w /usr/local/bin/loraflow.py" and add onlinedb.net API KEY (Free Service)

You can install packages separately if needed 
- sudo apt install python3-pip 
- python3 -m pip install pyserial
- python3 -m pip install RPi.GPIO
- python3 -m pip install websocket-client

## Why Lora is perfect technology for IoT messaging?
## Benefits: 
* Long distance (up to 8km)
* Energy efficient
* Cheap hardware
* Hardware is small in size
* No existing infrastructure is required


## Potential solutions/applications:
### Smart home
- [x] security systems (motion sensors, automatic lights)
- [x] device wireless control
- [x] telemetry data (temp/humidity data sensors)
### Meter reading
- [x] Environment monitor, telemetry data (temp/humidity data sensors)
- [x] Smart agriculture (Autonomous greenhouse)



