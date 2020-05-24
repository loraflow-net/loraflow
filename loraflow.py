#!/usr/bin/env python3
import os
import errno
import time
from threading import Thread
import json
import string
import pymysql
import serial
import RPi.GPIO as GPIO


#input pipe
#we will send messages to arduino nodes by writing commands in pipe
#
# For instance:
# echo -n "{\"a\":\"report\",\"d\":\"all\"}" > /tmp/loraflow

FIFO = '/tmp/loraflow'


db_host = '127.0.0.1'; #127.0.0.1 = localhost, leave empty to force local .sock conection
db_user = 'lora';
db_psw = '<put_your_password_here>';
db_select_db = 'loraflow';



ser = serial.Serial(
 port='/dev/ttyS0', #might be  port='/dev/ttyAMA0' as well
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
pipe_msg = "";
db_fields_array = []



def set_lora_module_normal_mode():
    GPIO.setmode(GPIO.BCM)
    #set lora module to nortmal mode
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

    #send empty to wake up node
    ser.write("     ".encode('ascii'));
    ser.flush()

def pipe_thread(threadname):
    global pipe_msg
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
                pipe_msg = data
                

def send_lora_msg( msg ):
    print(msg)
    
    
    
def save_sensor_raw_message( raw_str ):
    db = pymysql.connect(db_host, db_user ,db_psw, db_select_db )
    cursor = db.cursor()
    
    
    # Prepare SQL query to INSERT a record into the database.
    sql = """INSERT INTO raw_msg_received(msg,received)
             VALUES (%s, NOW())"""
            
    try:
       # Execute the SQL command
       cursor.execute(sql, (raw_str))
       # Commit your changes in the database
       db.commit()
    except:
       # Rollback in case there is any error
       print("DB insert failed!")
       print(cursor._last_executed);
       db.rollback()

    # disconnect from server
    db.close()
    
    
def get_existing_columns():

    print("Reading columns from DB...");
    
    connection = pymysql.connect(db_host, db_user ,db_psw, db_select_db, cursorclass=pymysql.cursors.DictCursor )
    
   
    try:
        with connection.cursor() as cursor:
            # Read a single record
            sql = "SHOW COLUMNS FROM sensor_data"
            cursor.execute(sql)
            db_fields = cursor.fetchall()
            for field in db_fields:
                db_fields_array.append(field["Field"])
    finally:
        connection.close()
        
    print('Field count from db: ',len(db_fields_array));
    
    
def add_field( fieldName, fieldValue ):
    
    dbType = '';
    if isinstance(fieldValue, str):
        dbType = "VARCHAR(254) NOT NULL DEFAULT ''"
    if isinstance(fieldValue, bool):
        dbType = "BOOL NOT NULL"
    if isinstance(fieldValue, float):
        dbType = "DECIMAL(11,2) NOT NULL"
    if isinstance(fieldValue, int):
        dbType = "INT NOT NULL"
    
    sql = "ALTER TABLE `sensor_data` ADD " + fieldName + " " + dbType;
    print("New field SQL steatment: ", sql)
    
    db = pymysql.connect(db_host, db_user ,db_psw, db_select_db )
    cursor = db.cursor()
    try:
       cursor.execute(sql)
       db.commit()
    except:
       # Rollback in case there is any error
       print("Failed to add new field into db: ", fieldName)
       print(cursor._last_executed);
       
    
    
def save_sensor_details_data( json ):
    

    #replace d to device
    json["device"] = json["d"]
    json.pop('d', None)
    
    
    reloadColumns = False
    
    user_fields_for_sql = "";
    placeholder_values_for_sql = "";
    user_values = []
    
    for field in json:
        
        if field not in db_fields_array:
            print("Field is missing in DB: ",field)
            print("Field type is: ",type(json[field]))
            add_field(field, json[field])
            reloadColumns = True
        
        user_fields_for_sql += ",`"+field+"`"
        placeholder_values_for_sql += ",%s"
        user_values.append(json[field])
        
   
    if reloadColumns :
        get_existing_columns()
        
        
    sql = """INSERT INTO sensor_data(received""" + user_fields_for_sql + """) 
             VALUES(NOW()"""+placeholder_values_for_sql+")""";
             
 
    db = pymysql.connect(db_host, db_user ,db_psw, db_select_db )
    cursor = db.cursor()
        
    try:
       # Execute the SQL command
       cursor.execute(sql, user_values)
       # Commit your changes in the database
       db.commit()
    except:
       # Rollback in case there is any error
       print("DB insert failed!")
       print(cursor._last_executed);
       db.rollback()
    
    db.close()
    
    

def clean_string( strg ):  
    newstrg = ""
    acc = """ '",{}[].`;:_-<>  """
    for x in strg:
        if x in string.ascii_letters or x in string.digits or x in acc:
            newstrg += x
    return newstrg

    

def serial_thread(threadname):
    global pipe_msg
    while True:
        
        #SERIAL READ
        #print("reading serial...")
        
        data = ser.readline()
        serialMsg = data.decode('ascii','ignore')
        
        if serialMsg != "" :
            
            print('Got lora msg: "{0}"'.format(serialMsg))
        
            #ser.readline();
            #serialMsg = "Ãƒ?Ã‚Â©{'rd':'motion_sensor_1','b':0.21}";
            save_sensor_raw_message(serialMsg);
            serialMsg = clean_string(serialMsg)
            #ensure that we have json with double quotes
            serialMsg = serialMsg.replace('\'','\"')

            try:
                jsonData = json.loads(serialMsg)
                print(jsonData['rd'])
                save_sensor_details_data( jsonData )

            except ValueError:
                print('Decoding JSON has failed')
            
        
        #SERIAL WRITE
         
        if pipe_msg != "" :
                print('Recieved message from pipe: "{0}"'.format(pipe_msg))
                
                #check if json has wake up flag -> "w":1
                #we should remove it before passing to lora node as it is not neccessary for node
                try:
                    jsonData = json.loads(pipe_msg)
                    if ("w" in jsonData) and (jsonData["w"] == 1) :
                        print("Setting wake up mode");
                        set_lora_module_to_wake_up_other()
                except ValueError:
                    print('Decoding JSON has failed')
                
                
                jsonData.pop('w', None) #remove w field
                msgToSend = str(jsonData)
                
                print('Going to send message to lora module: "{0}"'.format(msgToSend))
                
                ser.write(msgToSend.encode('ascii'))
                
                ser.flush()
                set_lora_module_normal_mode()
                pipe_msg = ""
                
        time.sleep(0.1)
    





get_existing_columns()
thread1 = Thread( target=pipe_thread, args=("Pipe Thread", ) )
thread1.start()

thread2 = Thread( target=serial_thread, args=("Serial Thread", ) )
thread2.start()

thread1.join()
thread2.join()




'''
pip3 install pymysql

MySQL tabbles

CREATE TABLE `raw_msg_received` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `received` datetime DEFAULT NULL,
  `msg` text,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=28 DEFAULT CHARSET=utf8;

CREATE TABLE `sensor_data` (
  `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
  `device` varchar(254) NOT NULL DEFAULT '',
  `received` datetime NOT NULL,
  PRIMARY KEY (`id`),
  KEY `device` (`device`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

Pipe comand example in cli:
echo -n "lights_on" > /tmp/loraflow

'''