from socket import timeout
import serial
import serial.tools.list_ports
import time
import datetime
import os

# helpers
cls = lambda: os.system('cls')

# CONSTANTS
BAUDRATE = 115200
SER_TIMEOUT = 10

# Instructions
instructions = {
    "COUNT_SAVED_DATAPOINTS": "06",
    "TOGGLE_DATA_CAPTURE": "00",
    "MANUAL_CAPTURE": "01",
    "EXPORT_ALL": "02",
    "DELETE_ALL": "03",
    "SET_COLLECTION_INTERVAL": "04",
    "SET_DATETIME": "05"
    }

ports = serial.tools.list_ports.comports()

inp = ""
while True:
    if (len(ports) == 0):
        print("There are no serial ports available. \n Retrying")
        for i in range(5):
            time.sleep(1)
            print('.', end='')
    else:
        print("Available serial ports:")
        for i in range(len(ports)):
            print("[" + str(i) + "] " + ports[i].name)
        break
        
while True:
    inp = input("Please pick a port to connect to by the number on the left\n>")
    if (inp.isnumeric() and int(inp) >= 0 and int(inp) < len(ports)): break
    print("Invalid input. Please try again.")

# open the serial port selected
s = serial.Serial(port=ports[int(inp)].name, baudrate=BAUDRATE, timeout=SER_TIMEOUT)

# immediately send a time update command
instruction = instructions["SET_DATETIME"] + "%s%s%s%s%s%s" %(str(datetime.datetime.now().year).zfill(4), str(datetime.datetime.now().month).zfill(2), str(datetime.datetime.now().day).zfill(2), str(datetime.datetime.now().hour).zfill(2), str(datetime.datetime.now().minute).zfill(2), str(datetime.datetime.now().second).zfill(2))
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
    # inp = input("Enter a command to send...")
    # inp += '\n';
    # s.write(bytes(inp, 'utf-8'))
    time.sleep(0.05)
    
    
    while not responded or s.in_waiting > 0:
        responded = True
        res = s.readline()
        res_str = res.decode().strip()
        print('\n' + res_str + '\n')
        

