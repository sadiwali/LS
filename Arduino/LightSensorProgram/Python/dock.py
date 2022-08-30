from socket import timeout
import serial
import serial.tools.list_ports
import time
import datetime
import os
from threading import Thread
from os.path import exists

# helpers
cls = lambda: os.system('cls')

# CONSTANTS
BAUDRATE = 115200
SER_TIMEOUT = 10

# FIILE IO
f = None
FILE_EXT = ".CSV"

# Instructions
instructions = {
    "COUNT_SAVED_DATAPOINTS": "06",
    "TOGGLE_DATA_CAPTURE": "00",
    "MANUAL_CAPTURE": "01",
    "EXPORT_ALL": "02",
    "DELETE_ALL": "03",
    "SET_COLLECTION_INTERVAL": "04",
    #"SET_DATETIME": "05",
    #"SAY_HELLO": "07",
    }

ports = serial.tools.list_ports.comports()

def say_hello(port, response, ind):
    s = serial.Serial(port=port, baudrate=BAUDRATE, timeout=SER_TIMEOUT)
    s.write(bytes("07" + '\n', 'utf-8'))
    res = s.readline()
    res = res.decode().strip()
    if (res == "Hello"):
        response[ind] = True
        
def check_serial_ports(old_ports):
    all_ports = serial.tools.list_ports.comports()
    new_ports = list(set(all_ports) - set(old_ports))
    


inp = ""

while True:
    if (len(ports) == 0):
        print("There are no serial ports available. \n Retrying in 5 seconds.")
        time.sleep(5)
    else:
        break
        
responses = [False] * len(ports)
threads = [None] * len(ports)
for i in range(len(threads)):
    threads[i] = Thread(target=say_hello, args=(ports[i].name, responses, i))
    threads[i].start()

for i in range(len(threads)):
    threads[i].join()
    
devices = []
    
for i in range(len(responses)):
    if responses[i] == True:
        devices.append({"id": i, "port_name": ports[i].name})
        
    print(ports[i].name + ": " + str(responses[i]))
        


# open the serial port
s = serial.Serial(port=devices[0]["port_name"], baudrate=BAUDRATE, timeout=SER_TIMEOUT)


# immediately send a time update command
date = str(datetime.datetime.now().year).zfill(4), str(datetime.datetime.now().month).zfill(2), str(datetime.datetime.now().day).zfill(2), str(datetime.datetime.now().hour).zfill(2), str(datetime.datetime.now().minute).zfill(2), str(datetime.datetime.now().second).zfill(2)
instruction = "05" + "%s%s%s%s%s%s" %(date)
s.write(bytes(instruction + '\n', 'utf-8'));
res = s.readline()
res_str = res.decode().strip()
time.sleep(0.1)
if res_str != "OK. Date set":
    print("Date time could not be set, please try again later.")
else:
    print("Date time set successfully.")
    
while True:
    
    for i, key in enumerate(instructions.keys()):
        print("[" + str(i) + "] " + key)
        
        
    inp = input("Choose a command by entering the number in front\n>")
    if (not inp.strip().isnumeric()):
        print("Invalid entry. Please try again.")
        continue
    
    # flag to wait for response
    responded = False
    
    print("Selected command :" + inp)
    inp = int(inp.strip())
    
    # find the right command to send
    if (list(instructions.keys())[inp] == "COUNT_SAVED_DATAPOINTS"):
        s.write(bytes(instructions["COUNT_SAVED_DATAPOINTS"] + '\n', 'utf-8'))
    elif (list(instructions.keys())[inp] == "TOGGLE_DATA_CAPTURE"):
        s.write(bytes(instructions["TOGGLE_DATA_CAPTURE"] + '\n', 'utf-8'))
    elif (list(instructions.keys())[inp] == "MANUAL_CAPTURE"):
        s.write(bytes(instructions["MANUAL_CAPTURE"] + '\n', 'utf-8'))
    elif (list(instructions.keys())[inp] == "EXPORT_ALL"):
        s.write(bytes(instructions["EXPORT_ALL"] + '\n', 'utf-8'))
        # open a file to write response into
        file_suffix = 0
        while exists(date + file_suffix + FILE_EXT):
            file_suffix += 1
        f = open(date + file_suffix + FILE_EXT, 'w')
    elif (list(instructions.keys())[inp] == "DELETE_ALL"):
        s.write(bytes(instructions["DELETE_ALL"] + '\n', 'utf-8'))
    elif (list(instructions.keys())[inp] == "SET_COLLECTION_INTERVAL"):  
        while True:
            inp = input("Enter the interval in ms\n>")
            if inp.lower() == "cancel" or inp.lower() == "exit":
                print("Command cancelled.")
                # set responded to true so we don't wait for a response
                responded = True
                break
            if (inp.isnumeric() and int(inp) >= 5000):
                instruction = instructions["SET_COLLECTION_INTERVAL"] + "_" + inp + '\n'
                print(instruction)
                s.write(bytes(instruction, 'utf-8'))
                break
            if (not inp.isnumeric()):
                print("Enter a numeric value greater than 5000")
            elif (int(inp) < 5000):
                print("Enter a value greater than 5000")

    time.sleep(0.05)
    
    
    while not responded or s.in_waiting > 0:
        responded = True
        res = s.readline()
        res_str = res.decode().strip()
        
        if (list(instructions.keys())[inp] == "EXPORT_ALL"):
            # if the previous command was export all, open a file and write all lines to it
            f.write(res_str)
            f.write('\n') 
        print('----\n' + res_str + '\n----')
        
    f.close();

