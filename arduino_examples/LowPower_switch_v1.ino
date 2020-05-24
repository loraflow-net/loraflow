// **** INCLUDES *****
#include "LowPower.h"
#include <SoftwareSerial.h>
#include "EBYTE.h"
#include <ArduinoJson.h>




#define SERIAL_DEBUG_ENABLED


char deviceId[20] = "switch_1";        
char codeVersion[10] = "1.0.0";


const int switch_1 = 11;
const int switch_2 = 12;
const int reportThresholdInIterations = 225*4; /* each iteration happens every 8s - every 30 min (30*60/8)=225 */
const int turnOffThresholdIterations = 5; /* 5*8 = approx 40 sec */
int sleepCounter = 0;
int turnOffTimeOutCounter = 0; /* as this sketch is used for water pump, it might be dengerous if switch is always on*/
bool isOn = false;


char json[243] = {'\0'}; //243 - lora message maximum bytes



/* 
  volatile  - it directs the compiler to load the variable from RAM. We are changing this value in interapt
 */

volatile bool lora_interrupt_in_progress = false; 



/* Setup Lora messaging serial interface
Now you MUST create the transceiver object and you must pass in the serail object use the & to pass by reference
 usage for teensy is the exact same
 M0, M1, and Aux pins are next */

SoftwareSerial ESerial(4, 5);
EBYTE Transceiver(&ESerial, 6, 7, 3);



void wakeUpOnLoraMsg(){
     lora_interrupt_in_progress = true;  
}

void setup(){

  
    pinMode(switch_1, OUTPUT);
    pinMode(switch_2, OUTPUT);


    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);
    pinMode(3, INPUT);
    
    turnSwitchesOff();

    
    #ifdef SERIAL_DEBUG_ENABLED
      Serial.begin(9600);
    #endif

    ESerial.begin(9600);
    Transceiver.init();


    //Uncomment these lines and values if you want messaging happen to other channel
    Transceiver.SetAddressL(0);
    Transceiver.SetAddressH(0);
    Transceiver.SetChannel(6); //we are using 6
    Transceiver.SaveParameters(PERMANENT);
    Transceiver.PrintParameters();
    

    Transceiver.SetMode(MODE_POWERDOWN);
    attachInterrupt(1, wakeUpOnLoraMsg, FALLING); //AUX drops 2-3ms before sending msg in serial 
    

}





void loop() {

    
    #ifdef SERIAL_DEBUG_ENABLED
      Serial.println(F("In main loop...."));
      Serial.print(F("Sleep counter: "));
      Serial.println(sleepCounter);
    #endif

    
    Serial.flush();
    
 
    // Enter power down state with ADC and BOD module disabled.
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 



    if( !lora_interrupt_in_progress){ //do not count interrupts
      sleepCounter++;
      turnOffTimeOutCounter++;
    }


    if( turnOffTimeOutCounter >= turnOffThresholdIterations ){
      turnSwitchesOff();
      turnOffTimeOutCounter = 0;
    }
    
    
    if( sleepCounter >= reportThresholdInIterations ){

      
      #ifdef SERIAL_DEBUG_ENABLED
       Serial.println(F("It is time to send report"));
      #endif

      //flush buffer before
      Serial.flush();
      ESerial.flush();
      
      sendReport();
      sleepCounter = 0;
      lora_interrupt_in_progress = false;
    }




    

    if( lora_interrupt_in_progress   ){



          #ifdef SERIAL_DEBUG_ENABLED
           Serial.print(F("Lora interrupt has been detected..."));
          #endif

          /* clear json buffer, just to be sure */
          memset(json, 0, sizeof json);



          delay(100);


          /*read everything what is in serial buffer*/
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
                          delay(random(1000, 10000)); //a lot of devices can send report parallel
                          sendReport(); 
                       }

                       if( strcmp(trim(action), {"is_running"}) == 0 ){
                          //which device requested it
                          char* recieverDevice = doc["rd"];
                          sendStatusToDevice( recieverDevice ); 
                       }


                       if( strcmp(trim(action), {"stop"}) == 0 ){
                          turnSwitchesOff();
                       }

                       if( strcmp(trim(action), {"start"}) == 0 ){
                          turnSwitchesOn();
                       }

                       if( strcmp(trim(action), {"update_timeout"}) == 0 ){
                          updateTimeOut();
                      }

                }else{
                  #ifdef SERIAL_DEBUG_ENABLED
                    Serial.println("Lora msg recieved, but it is not for me!");
                  #endif
                }
            }
            lora_interrupt_in_progress = false;
            //clear json buffer
          memset(json, 0, sizeof json);
          return;
    }

 
    lora_interrupt_in_progress = false;
   
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


void updateTimeOut(){
  turnOffTimeOutCounter = 0;
}


void turnSwitchesOff(){
  digitalWrite(switch_1, HIGH);
  digitalWrite(switch_2, HIGH);
  isOn = false;
   #ifdef SERIAL_DEBUG_ENABLED
    Serial.println(F("Turned switches Off"));
  #endif
}



void turnSwitchesOn(){
  digitalWrite(switch_1, LOW);
  digitalWrite(switch_2, LOW);
  isOn = true;
  turnOffTimeOutCounter = 0;
  #ifdef SERIAL_DEBUG_ENABLED
    Serial.println(F("Turned switches On"));
  #endif
  
}




void sendStatusToDevice( char *device ){

     if( device == NULL){
        device = {"all"};
     }

    #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Going to update with status..."));
      #endif
      
 
      char resultVol[10] = "0.0";
      dtostrf((readVcc()/1000.f), 4, 2, resultVol); //4 is mininum width, 2 is precision; float value is copied onto buff

      sprintf(json,"{\"rd\":\"%s\",\"d\":\"%s\",\"on\":%d}\n",deviceId,device,isOn);


      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Going to send such JSON to reciever: "));
        Serial.println(json);
      #endif


      Transceiver.SetMode(MODE_WAKEUP);
      Transceiver.SendStruct(&json, strlen(json));

       ESerial.flush();
       Transceiver.SetMode(MODE_POWERDOWN);
  
}

void sendReport(){

      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Going to send report..."));
      #endif
      
 
      char resultVol[10] = "0.0";
      dtostrf((readVcc()/1000.f), 4, 2, resultVol); //4 is mininum width, 2 is precision; float value is copied onto buff

      sprintf(json,"{\"rd\":\"%s\",\"vol\":%s,\"ver\":\"%s\",\"on\":%d}\n",deviceId, resultVol, codeVersion, isOn);


      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Going to send such JSON to reciever: "));
        Serial.println(json);
      #endif


      Transceiver.SetMode(MODE_NORMAL);
      Transceiver.SendStruct(&json, strlen(json));

       ESerial.flush();
       Transceiver.SetMode(MODE_POWERDOWN);
}



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
