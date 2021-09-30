#include <TimerOne.h>
#include "LowPower.h"
#include <SoftwareSerial.h>
#include "EBYTE.h"
#include <ArduinoJson.h>


#define SERIAL_DEBUG_ENABLED
//#define USE_DEVELOPER_TEST    1      // uncomment for new perimeter signal test (developers) 
//---------------------------------------------------------------------------------------------------

#define USE_DOUBLE_AMPLTIUDE    1         // uncomment to use +/- input voltage for amplitude (default), 
#define pinIN1       11
#define pinIN2       12
#define pinPWM       9
#define pinPWM2      10
#define pinRelay     8

char deviceId[4] = "lmw";        
char codeVersion[6] = "1.0.1";
char json[243] = {'\0'}; //243 - lora message maximum bytes
int sleepCounter = 0;
const int reportThresholdInIterations = 450; /* each iteration happens every 8s - every 1h will be (60*60/8)=450 */
bool isOn = false;

/* 
  volatile  - it directs the compiler to load the variable from RAM. We are changing this value in interapt
 */
volatile bool lora_interrupt_in_progress = false;
volatile int step = 0;
int dutyPWM = 255;

#ifdef USE_DEVELOPER_TEST
// a more motor driver friendly signal (sender)
int8_t sigcode[] = {  
  1,0,0,0,0,
  1,0,0,0,0,
  -1,0,0,0,0,
  1,0,0,0,0   };
#else
int8_t sigcode[] = { 
  1,1,-1,-1,1,-1,1,-1,-1,1,-1,1,1,-1,-1,1,-1,-1,1,-1,-1,1,1,-1 };
#endif


/* Setup Lora messaging serial interface
Now you MUST create the transceiver object and you must pass in the serail object use the & to pass by reference
 usage for teensy is the exact same
 M0, M1, and Aux pins are next */

SoftwareSerial ESerial(4, 5);
EBYTE Transceiver(&ESerial, 6, 7, 3);


void wakeUpOnLoraMsg(){
     lora_interrupt_in_progress = true;  
}


void timerCallback(){       
    if (sigcode[step] == 1) {      
      analogWrite(pinPWM, 0);                         
#ifdef USE_DOUBLE_AMPLTIUDE      
      analogWrite(pinPWM2, dutyPWM);
      digitalWrite(pinIN1, HIGH);
      digitalWrite(pinIN2, HIGH);
#endif             
    } 
    else if (sigcode[step] == -1) {              
      digitalWrite(pinIN1, HIGH);
      digitalWrite(pinIN2, HIGH);
      
      analogWrite(pinPWM, dutyPWM); 
      analogWrite(pinPWM2, 0);

    } 
    else {
      //digitalWrite(pinEnable, LOW);
      analogWrite(pinPWM2, 0);
      analogWrite(pinPWM, 0);
      digitalWrite(pinIN1, LOW);
      digitalWrite(pinIN2, LOW);
    } 
    step ++;    
    if (step == sizeof sigcode) {      
      step = 0;      
    }    
   
}


void setup() {  
  pinMode(pinIN1, OUTPUT);    
  pinMode(pinIN2, OUTPUT);  
  pinMode(pinPWM, OUTPUT);  
  pinMode(pinRelay, OUTPUT);  
  
  digitalWrite(pinIN1, HIGH);
  digitalWrite(pinIN2, HIGH);
 
  // sample rate 9615 Hz (19230,76923076923 / 2 => 9615.38)
  int T = 1000.0*1000.0/ 9615.38;
  Serial.begin(115200);

  Serial.println("START");
  Serial.print("Ardumower Sender ");
 
#ifdef USE_DEVELOPER_TEST
  Serial.println("Warning: USE_DEVELOPER_TEST activated");
#endif
  

  Serial.print("T=");
  Serial.println(T);    
  Serial.print("f=");
  Serial.println(1000.0*1000.0/T);    
  Timer1.initialize(T);         // initialize timer1, and set period

  Timer1.attachInterrupt(timerCallback);  
  

  // http://playground.arduino.cc/Main/TimerPWMCheatsheet
  // timer 2 pwm freq 31 khz  
  //cli();
  TCCR2B = TCCR2B & 0b11111000 | 0x01;
  //TIMSK2 |= (1 << OCIE2A);     // Enable Output Compare Match A Interrupt  
  //OCR2A = 255;                 // Set compared value
  //sei();

    #ifdef SERIAL_DEBUG_ENABLED
      Serial.begin(9600);
    #endif

    ESerial.begin(9600);
    Transceiver.init();
    Transceiver.PrintParameters();
    Transceiver.SetMode(MODE_POWERDOWN);
    attachInterrupt(1, wakeUpOnLoraMsg, FALLING); //AUX drops 2-3ms before sending msg in serial 
}






void loop(){
  
    if( sleepCounter >= reportThresholdInIterations ){
      #ifdef SERIAL_DEBUG_ENABLED
       Serial.println(F("It is time to send status report"));
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
                          int rnd = random(1, 15000);
                          #ifdef SERIAL_DEBUG_ENABLED
                            Serial.print(F("Delay for "));
                            Serial.print(rnd);
                            Serial.print(F(" milliseconds. To avoid colisions with other devices..."));
                          #endif
                          delay(rnd); //a lot of devices can send report parallel
                          sendReport(); 
                       }

                       if( strcmp(trim(action), {"on"}) == 0 ){
                          digitalWrite(pinRelay, HIGH);
                          isOn = true;
                          sendReport();
                       }

                       if( strcmp(trim(action), {"off"}) == 0 ){
                          digitalWrite(pinRelay, LOW);
                          isOn = false; //send status closed to other devices even process still in progress
                          sendReport();
                       }

                }else{
                  #ifdef SERIAL_DEBUG_ENABLED
                    Serial.println(F("Lora msg recieved, but it is not for me!"));
                  #endif
                }
            }
            lora_interrupt_in_progress = false;
            //clear json buffer
          memset(json, 0, sizeof json);
          return;
    }
}


void sendReport(){

      #ifdef SERIAL_DEBUG_ENABLED
        Serial.println(F("Going to send report..."));
      #endif
      
      sprintf(json,"{\"rd\":\"%s\",\"on\":%d}\n",deviceId, isOn);


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
