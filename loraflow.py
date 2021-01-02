#!/usr/bin/env python3
import os
import errno
import time
from threading import Thread
import json
import string
import serial
import RPi.GPIO as GPIO
import websocket
try:
    import thread
except ImportError:
    import _thread as thread
    
#OnlineDB.NET API KEY    

OnlineDBKEY = '<YOUR_API_KEY>'


#input pipe
#you can send JSON messages to arduino nodes by writing commands in pipe
#
# For instance:
# echo -n "{\"a\":\"report\",\"d\":\"all\",\"w\":1}" > /tmp/loraflow
# w - wake up

FIFO = '/tmp/loraflow'



ser = serial.Serial(
 port='/dev/ttyS0', #might be  port='/dev/ttyAMA0' as well
 #port='/dev/ttyAMA0',
 baudrate = 9600,
 parity=serial.PARITY_NONE,
 stopbits=serial.STOPBITS_ONE,
 bytesize=serial.EIGHTBITS,
 timeout=1
)


#RPi pins
M0 = 17
M1 = 27


# global variables
pipeMsg = "";



def set_lora_module_normal_mode():
    GPIO.setmode(GPIO.BCM)
    #set lora module to normal mode
    GPIO.setup(M0,GPIO.OUT)
    GPIO.setup(M1,GPIO.OUT)

    GPIO.output(M0,GPIO.LOW)
    GPIO.output(M1,GPIO.LOW)
    
    
def set_lora_module_to_wake_up_other():
    GPIO.setmode(GPIO.BCM)
    #add wakeup packet, because arduino lora module is sleeping
    GPIO.setup(M0,GPIO.OUT)
    GPIO.setup(M1,GPIO.OUT)

    GPIO.output(M0,GPIO.HIGH)
    GPIO.output(M1,GPIO.LOW)

    #send whitespaces to wake up node
    ser.write("     ".encode('ascii'));
    ser.flush()

def pipe_thread(threadname):
    global pipeMsg
    try:
        os.mkfifo(FIFO, 0o777)
    except OSError as oe:
        if oe.errno != errno.EEXIST:
            raise

    os.chmod(FIFO,0o777)

    while True:
        with open(FIFO) as fifo:
            while True:
                data = fifo.read()
                if len(data) == 0:
                    break
                pipeMsg = data
                

def send_lora_msg( msg ):
    print(msg)
    

def clean_string( strg ):  
    newstrg = ""
    acc = """ '",{}[].`;:_-<>  """
    for x in strg:
        if x in string.ascii_letters or x in string.digits or x in acc:
            newstrg += x
    return newstrg

    

def serial_thread(threadname):
    global pipeMsg
    global lastValidSerialMsg
    global ws
    
    while True:
        
        #SERIAL READ
        
        data = ser.readline()
        serialMsg = data.decode('ascii','ignore')
        
        if serialMsg != "" :
            
            print('Got lora msg: "{0}"'.format(serialMsg))
        
            serialMsg = clean_string(serialMsg)
            #ensure that we have json with double quotes
            serialMsg = serialMsg.replace('\'','\"')

            try:
                jsonData = json.loads(serialMsg)
                lastValidSerialMsg = json.dumps(jsonData);
                try:
                    print('Sending lora msg to socket "{0}"'.format(json.dumps(jsonData)))
                    ws.send(lastValidSerialMsg)
                except Exception as ex:
                    print("looks like websocket is down")
                
            except ValueError:
                print('Decoding JSON has failed')
            
        
        #SERIAL WRITE
         
        if pipeMsg != "" :
                print('Recieved message from pipe: "{0}"'.format(pipeMsg))
                
                #check if json has wake up flag -> "w":1
                #we should remove it before passing to lora node as it is not neccessary for node
                try:
                    jsonData = json.loads(pipeMsg)
                    if ("w" in jsonData) and (jsonData["w"] == 1) :
                        print("Setting wake-up mode");
                        set_lora_module_to_wake_up_other()
                        jsonData.pop('w', None) #remove w field
                except ValueError:
                    print('Decoding JSON has failed')
                
                
                msgToSend = str(jsonData)
                
                print('Going to send message to lora module: "{0}"'.format(msgToSend))
                
                ser.write(msgToSend.encode('ascii'))
                
                ser.flush()
                set_lora_module_normal_mode()
                pipeMsg = ""
                
        time.sleep(0.1)
        
        
#SOCKET RELATED

def on_message(ws, message):
    global pipeMsg
    global lastValidSerialMsg
    if message != lastValidSerialMsg :
        pipeMsg = message
    

def on_error(ws, error):
    print(error)

def on_close(ws):
    print("### ws closed ###")

def on_open(ws):
    print("### ws opened ###")


def websocket_thread(threadname):
    global ws
    while 1:
        ws = websocket.WebSocketApp("ws://www.onlinedb.net/" + str(OnlineDBKEY) + "/socket/",
                                  on_message = on_message,
                                  on_error = on_error,
                                  on_close = on_close)
        ws.on_open = on_open
        ws.run_forever()
        time.sleep(10)
        print("Reconnecting to websocket...");



thread1 = Thread( target=pipe_thread, args=("Pipe Thread", ) )
thread1.start()

thread2 = Thread( target=serial_thread, args=("Serial Thread", ) )
thread2.start()

thread3 = Thread( target=websocket_thread, args=("WebSocket Thread", ) )
thread3.start()

thread1.join()
thread2.join()
thread3.join()

