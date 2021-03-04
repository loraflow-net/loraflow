/* 

Arduino mini with 3 moisture sensors, 
AM2301 for humidity and Temperature, 
DS18B20 for ground temperature and PIR HC-SR501 in order to detect motion (triger alarm)
There is used Lora E32-868T20D with such setup: https://www.onlinedb.net/examples/lora-gateway-with-raspberry-pi (ESP32 might be used intead of Rasspberry Pi)


Arduino mini - https://www.aliexpress.com/item/32672852945.html
Lora - https://www.aliexpress.com/item/32802241921.html
Moisture sensor - https://www.aliexpress.com/item/32832538686.html
PIR sensor - https://www.aliexpress.com/item/1874938103.html
Temperature + Humidity - https://www.aliexpress.com/item/32769460765.html
Temperature on ground - https://www.aliexpress.com/item/4000068914916.html
1.3" OLED screen - https://www.aliexpress.com/item/32844104782.html


Please use ArduinoJson 6.17.3 library

Note: I am using short version of font u8x8_font_inr46_4x8_f 
There are used numbers only otherwise code is too large and will not be able to upload it to Mini

Note 2: Use alarm cable 8 core for Moisture sensor. Calibration percentage based on wire length

 */


// **** INCLUDES *****
#include "LowPower.h"
#include <SoftwareSerial.h>
#include "EBYTE.h"
#include <ArduinoJson.h>
#include "DHT_U.h"



//#define SERIAL_DEBUG_ENABLED
#define PIR_RISING
//#define HAS_DS18B20
//#define READ_SOLAR_DISABLE
//#define HAS_DISPLAY
//#define IS_MOTION_SENSOR

#ifdef HAS_DISPLAY
  #include <U8x8lib.h>
  #ifdef U8X8_HAVE_HW_I2C
    #include <Wire.h>
  #endif
#endif

#ifdef HAS_DS18B20
  #include <DS18B20.h>
  #include <OneWire.h>
#endif


//device id
char deviceId[] = "g_2";
char codeVersion[] = "1.0.3";

// Use pin 2 & 3 as wake up pin
const int wakeUpPin = 2;
const int enableLightPin = 8;
const int enableSensorsPin = 10;
const int solarAnalogPin = A0;
const int moisture_1_AnalogPin = A1;
const int moisture_2_AnalogPin = A2;
const int moisture_3_AnalogPin = A3;
#ifdef HAS_DS18B20
  const int DS18B20Pin = A4;
#endif
const int beepPin = 13;
const int sleepThresholdInIterations = 450; /* each iteration happens every 8s - every 1h (60*60/8)=450 */
const int beepThresholdInIterations = 900; /* each iteration happens every 8s - every 2 hour (2*60*60/8)=450 */
const int badMoistureThreshold = 450; /*remind about bad Moisture, every hour*/
const int badMoistureProcentageThreshold = 30;
long loraTimeoutOver = 0;

int sleepCounter = 0;
int beepCounter = 0;
int badMoistureCounter = 0;

char json[243] = {'\0'}; //243 - lora maximum bytes

#ifndef IS_MOTION_SENSOR
  
  #ifdef HAS_DISPLAY
  U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(/* clock=*/ 12, /* data=*/ 11, /* reset=*/ U8X8_PIN_NONE); 
  #endif
  
  DHT dht(9, DHT21);
#endif

#ifdef HAS_DS18B20
  OneWire oneWire(DS18B20Pin);
  DS18B20 DS18B20_sensor(&oneWire);
#endif


volatile bool lora_interrupt_in_progress = false;
volatile bool motion_interrupt_in_progress = false;

SoftwareSerial ESerial(4, 5);
EBYTE Transceiver(&ESerial, 6, 7, 3);


void wakeUpOnMovement(){
    // Just a handler for the pin interrupt.
    if( lora_interrupt_in_progress == false){
      motion_interrupt_in_progress = true; 
    } 
}


void wakeUpOnLoraMsg(){
     lora_interrupt_in_progress = true;  
     motion_interrupt_in_progress = false;  
}

void setup(){

     Serial.begin(9600);


     #ifndef IS_MOTION_SENSOR
       tone(beepPin,1000,100);
       delay(1000);
       dht.begin();
       #ifdef HAS_DISPLAY
        u8x8.begin();
       #endif
     #endif
  
    // Configure wake up pin as input.
    // This will consumes few uA of current.
    pinMode(wakeUpPin, INPUT);
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);

    #ifndef IS_MOTION_SENSOR
     pinMode(beepPin, OUTPUT);
     pinMode(enableSensorsPin, OUTPUT); //switch on all sensors pin
    #endif


    #ifdef IS_MOTION_SENSOR
      pinMode(enableLightPin, OUTPUT);
    #endif


    ESerial.begin(9600);
    Transceiver.init();

    #ifndef IS_MOTION_SENSOR
      #ifdef HAS_DISPLAY
      showValueOnDisplay({"    "});
      #endif
    #endif


    #ifndef IS_MOTION_SENSOR
      #ifdef HAS_DISPLAY
        u8x8.setPowerSave(true);
      #endif
    #endif



    /* uncomment if you need to setup Lora default channel */

    //Transceiver.SetAddressL(1);
    //Transceiver.SetAddressH(0);
    //Transceiver.SetChannel(6);
    //Transceiver.SaveParameters(PERMANENT);
    //Transceiver.PrintParameters();
    


    Transceiver.SetMode(MODE_POWERDOWN);
    #ifdef PIR_RISING
    attachInterrupt(0, wakeUpOnMovement, RISING);    
    #else
    attachInterrupt(0, wakeUpOnMovement, FALLING);    
    #endif
    attachInterrupt(1, wakeUpOnLoraMsg, FALLING); //AUX drops 2-3ms before sending msg in serial 


}


bool isItForMe( char *str ){


    #ifdef SERIAL_DEBUG_ENABLED
      Serial.print(F("My device id is: "));
      Serial.println(deviceId);
  
  
      Serial.print(F("Compare with: "));
      Serial.println(trim(str));
    #endif


    if(  (strcmp(trim(str), deviceId) == 0) ||  (strcmp(trim(str), {"all"}) == 0)  ){
       return true;
    }
    
    return false;
}


#ifndef IS_MOTION_SENSOR
  void playBadSong(){
      
  int melody[] = {
    262, 196,196, 220, 196,0, 247, 262};

  // note durations: 4 = quarter note, 8 = eighth note, etc.:
  int noteDurations[] = {
    4, 8, 8, 4,4,4,4,4 };
  
      for (int thisNote = 0; thisNote < 8; thisNote++) {
        int noteDuration = 1000/noteDurations[thisNote];
        tone(13, melody[thisNote],noteDuration);
        int pauseBetweenNotes = noteDuration * 1.30;
        delay(pauseBetweenNotes);
        noTone(12);
      }
  }
#endif




float getSolarSensorVoltage(){

  #ifdef READ_SOLAR_DISABLE
    return 0.0;
  #endif 

  int sensorValue = analogRead(solarAnalogPin);  // read the input pin

  #ifdef SERIAL_DEBUG_ENABLED
    Serial.print(F("Sensor value: "));
    Serial.println(sensorValue);
  #endif 


  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  float currentBatteryVoltage = readVcc() / 1000.f;

  #ifdef SERIAL_DEBUG_ENABLED
    Serial.print(F("Voltage read: "));
    Serial.println(currentBatteryVoltage);
  #endif
  
  float voltage = sensorValue * (currentBatteryVoltage / 1023.0);

  #ifdef SERIAL_DEBUG_ENABLED
    Serial.print(F("Sensor voltage: "));
    Serial.println(voltage);
  #endif

  return voltage;
}

float moistureToProcentage( float moisture){

          /* 380 = 100% */
          /* 480 = 50% */
          /* 580 = 0 %*/
      
      
          /* 410 = 100% */
          /* 610 = 0 %*/

        
           int startProcentage = 400;
           int endProcentage = 600;
      
           return (float)(endProcentage - moisture)/(float)(endProcentage-startProcentage) * 100.f;
}



void sendReport(){


      
      digitalWrite(enableSensorsPin, HIGH);



      char DS18B20Val[10] = "0.0";
      #ifdef HAS_DS18B20
        DS18B20_sensor.begin();
        DS18B20_sensor.requestTemperatures();
        //first will be incorrect reading
        delay(100);
        DS18B20_sensor.requestTemperatures();
        #ifdef SERIAL_DEBUG_ENABLED
          Serial.print(F("DS18B20_sensor temperature is: "));
          Serial.println(DS18B20_sensor.getTempC());
        #endif
        
        dtostrf(DS18B20_sensor.getTempC(), 4, 2, DS18B20Val);
      #endif
      
      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Waiting.."));
      #endif


      #ifndef IS_MOTION_SENSOR
        dht.begin();
        delay(random(1000, 10000)); //a lot of devices can send report parallel //wairant for other nodes
      #endif
      

      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Ok, go..."));
      #endif
      
    
      

      char resultVol[10] = "0.0";
      dtostrf((readVcc()/1000.f), 4, 2, resultVol); //4 is mininum width, 2 is precision; float value is copied onto buff


      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("reading solar..."));
      #endif

      char resultSolar[10] = "0.0";
      dtostrf(getSolarSensorVoltage(), 4, 2, resultSolar); //4 is mininum width, 2 is precision; float value is copied onto buff


      
   
      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("reading temperature..."));
      #endif

     

      #ifndef IS_MOTION_SENSOR
            float hum = dht.readHumidity();
            float temp = dht.readTemperature();
      
      
             #ifdef SERIAL_DEBUG_ENABLED
              Serial.println(F("Temp: "));
              Serial.print(temp);
              Serial.println(F("Hum: "));
              Serial.print(hum);
            #endif
      
            tone(beepPin,500,20);
      
            
           // check if returns are valid, if they are NaN (not a number) then something went wrong!
           if (isnan(temp) || isnan(hum)) {
            hum = 0.0;
            temp = 0.0;
           }
    
          char resultTemp[10];
          dtostrf(temp, 4, 1, resultTemp); //4 is mininum width, 2 is precision; float value is copied onto buff
      
          char resultHum[10];
          dtostrf(hum, 4, 1, resultHum);//4 is mininum width, 2 is precision; float value is copied onto buff
        
          
         #ifdef SERIAL_DEBUG_ENABLED
            Serial.println(F("reading moistures..."));
          #endif
    
          char result_m1[5];
          dtostrf(moistureToProcentage(analogRead(moisture_1_AnalogPin)), 4, 0, result_m1);


          char result_m2[5];
          dtostrf(moistureToProcentage(analogRead(moisture_2_AnalogPin)), 4, 0, result_m2);


          char result_m3[5];
          dtostrf(moistureToProcentage(analogRead(moisture_3_AnalogPin)), 4, 0, result_m3);
    

          sprintf(json,"{\"rd\":\"%s\",\"vol\":%s,\"solar\":%s,\"ver\":\"%s\",\"t\":%s,\"hum\":%s,\"m_1\":%s,\"m_2\":%s,\"m_3\":%s,\"t2\":%s}\n",
          deviceId, resultVol,resultSolar,codeVersion,resultTemp,resultHum,result_m1,result_m2,result_m3,DS18B20Val);

      #endif 


      #ifdef IS_MOTION_SENSOR
        sprintf(json,"{\"rd\":\"%s\",\"vol\":%s,\"solar\":%s,\"ver\":\"%s\"}\n",deviceId, resultVol,resultSolar,codeVersion);
      #endif 

      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(json);
      #endif
      
      Transceiver.SetMode(MODE_NORMAL);
      delay(200);
      Transceiver.SendStruct(&json, strlen(json));
       ESerial.flush();
       delay(500);
       Transceiver.SetMode(MODE_POWERDOWN);

       #ifndef IS_MOTION_SENSOR
        delay(1000);
        digitalWrite(enableSensorsPin, LOW);
       #endif
}



#ifdef IS_MOTION_SENSOR

bool isItDarkOutside( float value){
  if( value >= 0.5f ){
    return false;
  }
  return true;
}

void turnOnLed( int duration){

    if( (duration == NULL) || (duration == 0) ){
      duration = 30;
    }

    
    digitalWrite (enableLightPin, HIGH);
    delay (duration * 1000);
    digitalWrite (enableLightPin, LOW);
    delay (1000);
    
    #ifdef SERIAL_DEBUG_ENABLED
      Serial.println("Leaving Leds...");
      Serial.flush();
    #endif
}

#endif



#ifndef IS_MOTION_SENSOR
      #ifdef HAS_DISPLAY
      void showSensorValuesOnDisplay(){
      
           
           float hum = dht.readHumidity();
           float temp = dht.readTemperature();

           if (isnan(temp) || isnan(hum)) {
            hum = 0.0;
            temp = 0.0;
           } 
      
      
      
         char resultTemp[10];
          dtostrf(temp, 4, 1, resultTemp); //4 is mininum width, 2 is precision; float value is copied onto buff
      
          char resultHum[10];
          dtostrf(hum, 4, 0, resultHum);//4 is mininum width, 2 is precision; float value is copied onto buff
         
          sprintf(resultTemp,"%s ",trim(resultTemp));
          sprintf(resultHum,"%s %% ",trim(resultHum));
      
      
          showValueOnDisplay(resultTemp);
          showValueOnDisplay(resultHum);
          showMoistureOnDisplay(moisture_1_AnalogPin);
          showMoistureOnDisplay(moisture_2_AnalogPin);
          showMoistureOnDisplay(moisture_3_AnalogPin);
          showSolarVoltageOnDisplay();
      }
      
      
      void showSolarVoltageOnDisplay(){
        char resultSolar[10];
        dtostrf(getSolarSensorVoltage(), 4, 2, resultSolar); //4 is mininum width, 2 is precision; float value is copied onto buff

        u8x8.inverse();
        showValueOnDisplay(resultSolar);
        u8x8.noInverse();
      }


      void showMoistureOnDisplay(const int moisturePin){
          int moisture;   
          moisture = analogRead(moisturePin);
      
           float procentage = moistureToProcentage(moisture);
      
           #ifdef SERIAL_DEBUG_ENABLED
            Serial.println(F("Procentage is"));
            Serial.println(procentage);
          #endif
       
           char resultProcentageChars[10];
           dtostrf(procentage, 3, 0, resultProcentageChars); //3 is mininum width, 0 is precision; float value is copied onto buff
          
          strcat(resultProcentageChars, "% ");
          u8x8.inverse();
          showValueOnDisplay(resultProcentageChars);
          u8x8.noInverse();

          
          if( (procentage <  badMoistureProcentageThreshold) && (badMoistureCounter >= badMoistureThreshold) ){
            playBadSong();
            badMoistureCounter = 0;
            delay(1000);
          }
          
      }
      
      
      void showValueOnDisplay(const char* value){
          u8x8.setFont(u8x8_font_inr46_4x8_f);    
          u8x8.drawString(0, 0, value);
          delay(1000);
      }
      #endif
#endif

void loop() {

    Serial.println(F("In main loop...."));
    Serial.print(F("Sleep counter: "));
    Serial.println(sleepCounter);

    
    #ifdef SERIAL_DEBUG_ENABLED
    Serial.flush();
    #endif
    ESerial.flush();
    
 
     
    // Enter power down state with ADC and BOD module disabled.
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 


    if( (sleepCounter % 10) == 0){ //hungs on without these lines
      digitalWrite(enableSensorsPin, HIGH);
      delay(100);
      digitalWrite(enableSensorsPin, LOW);
    }


    if( !motion_interrupt_in_progress && !lora_interrupt_in_progress){ //do not count interrupts
      sleepCounter++;
      beepCounter++;
      badMoistureCounter++;
    }


  
    
    if( sleepCounter >= sleepThresholdInIterations ){

      /* for some reason this hungs up */
      
      #ifdef SERIAL_DEBUG_ENABLED
       Serial.println(F("It is time to send report"));
      #endif

      //flush buffer before
      #ifdef SERIAL_DEBUG_ENABLED
        Serial.flush();
      #endif
      ESerial.flush();
      
      sendReport();
      sleepCounter = 0;
      lora_interrupt_in_progress = false;
      motion_interrupt_in_progress = false;
    }



    #ifdef SERIAL_DEBUG_ENABLED
      Serial.println(F("PART2--------------------------"));
     Serial.print(F("lora_interrupt_in_progress: "));
     Serial.println(lora_interrupt_in_progress);
      
     Serial.print(F("motion_interrupt_in_progress: "));
     Serial.println(motion_interrupt_in_progress);
    
     Serial.println(F("--------------------------"));
   #endif


   if( lora_interrupt_in_progress == true ){
      motion_interrupt_in_progress = false;
   }

    

    if( lora_interrupt_in_progress   ){

          #ifdef SERIAL_DEBUG_ENABLED
           Serial.print(F("Starting execute lora code..."));
          #endif

          *json = '\0';
          
          int i = 0;
          while (ESerial.available() > 0 && ( i < (sizeof json))){
            json[i] = ESerial.read();
            i++;
          }


          #ifdef SERIAL_DEBUG_ENABLED
                Serial.print(F("Recieved msg from server: "));
                Serial.println(json);
          #endif

          
         
          if( strlen(trim(json)) < 5 ){
            #ifdef SERIAL_DEBUG_ENABLED
                Serial.print(F("Msg is too short! Skip.."));
            #endif

            lora_interrupt_in_progress = false;
            return;
          }


          if (strchr(json, '{') == NULL){
            #ifdef SERIAL_DEBUG_ENABLED
                Serial.println(F("No { in str"));
            #endif
            lora_interrupt_in_progress = false;
            return;
          }

          if (strchr(json, '}') == NULL){
            #ifdef SERIAL_DEBUG_ENABLED
                Serial.println(F("No } in str"));
            #endif
            lora_interrupt_in_progress = false;
            return;
          }
         

          // Allocate the JSON document
          // Use arduinojson.org/v6/assistant to compute the capacity.
          const size_t capacity = JSON_OBJECT_SIZE(12) + 80;
          DynamicJsonDocument doc(capacity);
          DeserializationError error = deserializeJson(doc, trim(json));
          
            // Test if parsing succeeds.
            if (error) {
              #ifdef SERIAL_DEBUG_ENABLED
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
              #endif
            }else{

                char* action = doc["a"];
                char* device = doc["d"];
 
                if( isItForMe(device) ){

                       if( strcmp(trim(action), {"report"}) == 0 ){
                          if( (strcmp(trim(device), {"all"}) == 0) ){ //report with random delay, if report for all devices are requested => a lot of devices can send report parallel
                            delay(random(1000, 10000));
                          }
                          sendReport(); 
                       }


                       #ifdef IS_MOTION_SENSOR

                       if( strcmp(trim(action), {"lights_greenhouse_1"}) == 0 || strcmp(trim(action), {"lights"}) == 0){
                          int duration = doc["dur"];
                          turnOnLed(duration);
                      }

                      #endif
                }
            }
            lora_interrupt_in_progress = false;
            motion_interrupt_in_progress = false;
            //clear json buffer
          memset(json, 0, sizeof json);
          loraTimeoutOver = millis() + 100;
          return;
    }

  

  
    if( motion_interrupt_in_progress  ){


      if( millis() < loraTimeoutOver){
         #ifdef SERIAL_DEBUG_ENABLED
              Serial.println(F("Looks like it was lora interupt.. leaving motion"));
          #endif
          lora_interrupt_in_progress = false;
          motion_interrupt_in_progress = false;
          return;
      }


      //clear all json content
      memset(json, 0, sizeof json);

      char resultVol[10];
      dtostrf((readVcc()/1000.f), 4, 2, resultVol); //4 is mininum width, 2 is precision; float value is copied onto buff
      

      char resultSolar[10];
      dtostrf(getSolarSensorVoltage(), 4, 2, resultSolar); //4 is mininum width, 2 is precision; float value is copied onto buff


        #ifdef IS_MOTION_SENSOR
          sprintf(json,"{\"m\":1,\"rd\":\"%s\",\"d\":\"all\",\"a\":\"lights\",\"vol\":%s,\"solar\":%s,\"ver\":\"%s\"}\n",deviceId, resultVol,resultSolar,codeVersion);
           
            Transceiver.SetMode(MODE_WAKEUP);
             Transceiver.SendStruct(&json, strlen(json));
             #ifdef SERIAL_DEBUG_ENABLED
              Serial.println(F("Lora msg has been sent"));
             #endif
             ESerial.flush();
             delay(100);
             Transceiver.SetMode(MODE_POWERDOWN);

            if( isItDarkOutside(getSolarSensorVoltage()) ){
                 #ifdef SERIAL_DEBUG_ENABLED
                  Serial.println(F("It is dark outside. Turn on lights"));
                 #endif
                turnOnLed(30);
             }else{
                delay(10);
                #ifdef SERIAL_DEBUG_ENABLED
                  Serial.println(F("It is light outside. Just send lora msg"));
                #endif
             }
             
        #endif


       #ifndef IS_MOTION_SENSOR

            digitalWrite(enableSensorsPin, HIGH);
      
      
            if( beepCounter >= beepThresholdInIterations ){
              tone(beepPin,4500,700);
              beepCounter = 0;
            }


            char DS18B20Val[10] = "0.0";
            #ifdef HAS_DS18B20
              DS18B20_sensor.begin();
              DS18B20_sensor.requestTemperatures();
              #ifdef SERIAL_DEBUG_ENABLED
                Serial.print(F("DS18B20_sensor temperature is: "));
                Serial.println(DS18B20_sensor.getTempC());
              #endif
              
              dtostrf(DS18B20_sensor.getTempC(), 4, 2, DS18B20Val);
            #endif
      
            
            //show values on display
            #ifdef HAS_DISPLAY
              u8x8.setPowerSave(false);
            #endif
      
            delay(1000); //dht to warm up, according to doc should be 1s
            
      
      
            float hum = dht.readHumidity();
            float temp = dht.readTemperature();
           if (isnan(temp) || isnan(hum)) {
            hum = 0.0;
            temp = 0.0;
           } 
      
      
           char resultTemp[10];
            dtostrf(temp, 4, 1, resultTemp); //4 is mininum width, 2 is precision; float value is copied onto buff
        
            char resultHum[10];
            dtostrf(hum, 4, 0, resultHum);//4 is mininum width, 0 is precision; float value is copied onto buff
      
            char resultHumSend[10];
            dtostrf(hum, 4, 1, resultHumSend);//4 is mininum width, 1 is precision; float value is copied onto buff
      
            
            

            #ifdef HAS_DISPLAY
              sprintf(resultTemp,"%s ",resultTemp);
              showValueOnDisplay(resultTemp);
            #endif
      


            char result_m1[5];
            dtostrf(moistureToProcentage(analogRead(moisture_1_AnalogPin)), 4, 0, result_m1);


            char result_m2[5];
            dtostrf(moistureToProcentage(analogRead(moisture_2_AnalogPin)), 4, 0, result_m2);


            char result_m3[5];
            dtostrf(moistureToProcentage(analogRead(moisture_3_AnalogPin)), 4, 0, result_m3);
      

      

            //send data to server
            sprintf(json,"{\"m\":1,\"rd\":\"%s\",\"vol\":%s,\"solar\":%s,\"ver\":\"%s\",\"t\":%s,\"hum\":%s,\"m_1\":%s,\"m_2\":%s,\"m_3\":%s,\"t2\":%s}\n",
            deviceId,resultVol,resultSolar,codeVersion,resultTemp,trim(resultHumSend),result_m1,result_m2,result_m3,DS18B20Val);
            
      
            #ifdef SERIAL_DEBUG_ENABLED
              Serial.print(F("Going to send json:"));
              Serial.println(json);
            #endif
      
            //wakeup other devices
            //Transceiver.SetMode(MODE_WAKEUP); - server listens always
             Transceiver.SetMode(MODE_NORMAL);
             Transceiver.SendStruct(&json, strlen(json));
             #ifdef SERIAL_DEBUG_ENABLED
              Serial.println(F("Lora msg has been sent"));
             #endif
             ESerial.flush();

             
             Transceiver.SetMode(MODE_POWERDOWN);
      

              #ifdef HAS_DISPLAY
                char resultHumidity[20];
                sprintf(resultHumidity,"%s %% ",trim(resultHum));
                showValueOnDisplay(resultHumidity);
                delay(1000);//sensor can read each 1s
        
              
                showMoistureOnDisplay(moisture_1_AnalogPin);
                delay(1000);
        
                showMoistureOnDisplay(moisture_2_AnalogPin);
                delay(1000);
        
                showMoistureOnDisplay(moisture_3_AnalogPin);
                delay(1000);
        
                showSolarVoltageOnDisplay();
                delay(1000);
                
                
                for(int i=0; i<2;i++){
                  showSensorValuesOnDisplay();
                }
                showValueOnDisplay({"    "});
                u8x8.setPowerSave(true);
              #endif
              
             digitalWrite(enableSensorsPin, LOW);
         #endif

         motion_interrupt_in_progress = false;
         lora_interrupt_in_progress = false;
    }


    motion_interrupt_in_progress = false;
    lora_interrupt_in_progress = false;
  
  
}



//=============================
// HELPERS
//=============================


char *ltrim(char *s){
    while(isspace(*s)) s++;
    return s;
}

char *rtrim(char *s){
    char* back = s + strlen(s);
    while(isspace(*--back));
    *(back+1) = '\0';
    return s;
}

char *trim(char *s){
    return rtrim(ltrim(s)); 
}


long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}
