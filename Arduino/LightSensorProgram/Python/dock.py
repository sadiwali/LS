'''
Command-line tool suite for Open Spectral Sensing (OSS) device.
'''

import serial
import serial.tools.list_ports
import time
import datetime
import os
from threading import Thread
from os.path import exists

# helpers
cls = lambda: os.system('cls')              # clear console

# CONSTANTS
BAUDRATE = 115200                           # baudrate
SER_TIMEOUT = 10                            # serial timeout (should be a few seconds longer than device sleep time)

# FIILE IO
SAVE_DIR = "./data/"                        # directory for saving files
f = None                                    # for writing files
FILE_EXT = ".CSV"                           # file extension

# user input
inp = ""

# Instructions that the OSS device understands
instructions = {
    "TOGGLE_DATA_CAPTURE": "00",
    "MANUAL_CAPTURE": "01",
    "EXPORT_ALL": "02",
    "DELETE_ALL": "03",
    "SET_COLLECTION_INTERVAL": "04",
    "_SET_DATETIME": "05",
    "COUNT_SAVED_DATAPOINTS": "06",
    "_SAY_HELLO": "07",
    "SET_DEVICE_NAME": "08",
    "GET_INFO": "09",
    "DISCONNECT_DEVICE": "10",
    "DISCONNECT_ALL": "11",
    "PAUSE_COLLECTION": "12",
    "RESUME_COLLECTION": "12",
}


# write to all devices, or a specific one if id is supplied
# TODO: reasoning invalid. Should only write to one device at a time, device selector needed
def write_to_devices(msg, id = -1):
    global devices

    for device in devices:
        if (id != -1 and device["id"] != id):
            continue
        d = device["serial_object"]
        d.write(bytes(msg + '\n', 'utf-8'))

        time.sleep(0.1)

        res_str = d.readline().decode().strip()

        if res_str == "OK":
            return True
        elif res_str == "DATA":
            res_str = d.readline().decode().strip()
            return res_str
        else:
            return False


        if (id != -1):
            break

# open a new file for writing. If file name already exists, append a number
def open_file(filename):
    global f
    
    # open a file to write response into
    file_suffix = 0
    while True:
        save_filename = SAVE_DIR + str(filename) + str(file_suffix) + FILE_EXT
        if exists(save_filename):
            file_suffix += 1
        else:
            f = open(save_filename, 'w')
            break
    
# cmd is the index displayed to user. The user selects this, and it is corresponded to an instruction
def decode_cmd(cmd):
    return list(instructions.keys())[cmd]

# Pings all serial devices connected, saves responses of valid OSS devices
def say_hello(port, response, ind):
    try:
        with serial.Serial(port=port, baudrate=BAUDRATE, timeout=SER_TIMEOUT) as _s:
            _s.write(bytes(instructions["_SAY_HELLO"] + '\n', 'utf-8'))
            time.sleep(0.1)
            
            res = _s.readline()
            res = res.decode().strip()
            if "Hello" in res:
                res = _s.readline()
                res = res.decode().strip()
                # save the name to the response
                response[ind] = res 
    except Exception as e:
        pass                
        
        
if __name__ == "__main__":
    # list of serial ports connected to the computer
    ports = serial.tools.list_ports.comports()
    while len(ports) <= 0:
        print("There are no serial ports available. Retrying in 1 second.")
        time.sleep(1)
        ports = serial.tools.list_ports.comports()
        
    # for detecting which serial devices are open spectral sensors  
    responses = [None] * len(ports)
    threads = [None] * len(ports)

    # say hello to each device in a thread
    for i in range(len(threads)):
        threads[i] = Thread(target=say_hello, args=(ports[i].name, responses, i))
        threads[i].start()
    
    # wait for every device to respond
    for i in range(len(threads)):
        threads[i].join()
        
    # detected spectral sensors
    devices = []
    # look through the responses, and find spectral sensors and connect to them
    for i in range(len(responses)):
        if responses[i] is not None:
            devices.append({"id": i, "device_name": responses[i],"port_name": ports[i].name, "serial_object": serial.Serial(port=ports[i].name, baudrate=BAUDRATE, timeout=SER_TIMEOUT)})   
        print(ports[i].name + ": " + str(responses[i]))
            
    instruction = ""

    # immediately send a time update command to all connected devices
    date = str(datetime.datetime.now().year).zfill(4) + str(datetime.datetime.now().month).zfill(2) + str(datetime.datetime.now().day).zfill(2) + str(datetime.datetime.now().hour).zfill(2) + str(datetime.datetime.now().minute).zfill(2) + str(datetime.datetime.now().second).zfill(2)
    instruction = "05" + date
    
    write_to_devices(instruction)
    

        
  
        
    while True:
        
        for i, key in enumerate(instructions.keys()):
            print("[" + str(i) + "] " + key)
            
            
        inp = input("Choose a command by entering the number in front\n>")
        if (not inp.strip().isnumeric()):
            print("Invalid entry. Please try again.")
            continue
        
        # flag to wait for response
        responded = False
        
        inp = int(inp.strip())
        
        # find the right command to send
        cmd = decode_cmd(inp)
        
        if (cmd == "MANUAL_CAPTURE"):
            instruction = instructions["MANUAL_CAPTURE"]
        elif (cmd == "EXPORT_ALL"):
            open_file(date)
            instruction = instructions["EXPORT_ALL"]
        elif (cmd == "SET_COLLECTION_INTERVAL"):
            while True:
                inp = input("Enter the interval in ms\n>").strip()
                if inp.lower() == "cancel" or inp.lower() == "exit":
                    print("Command cancelled. Capture frequency unchanged.")
                    # set responded to true so we don't wait for a response
                    responded = True
                    break
                
                if (not inp.isnumeric()):
                    print("Enter a numeric value greater than 5000")
                    continue
                elif (int(inp) < 5000):
                    print("Enter a value greater than 5000")
                    continue
                
                instruction = instructions["SET_COLLECTION_INTERVAL"] + "_" + inp
                break
        elif (cmd == "SET_DEVICE_NAME"):
            while True:
                inp = input("Enter the device name\n").strip()
                if (inp.lower() == "cancel" or inp.lower() == "exit"):
                    print("Command canelled. Name not set.")
                    responded = True
                    break   
                
                if (len(inp) > 12):
                    print("Name too long. Please try again.")
                    continue
                
                inp = inp.replace(' ', '_')
                instruction = instructions["SET_DEVICE_NAME"] + "_" + inp
                break
        else:
            instruction = instructions[cmd]
        
        # write the command to the device
        print("Writing: '" + instruction + "'")
        write_to_devices(instruction)
        
        save_flag = None
        
        while not responded or s.in_waiting > 0:
            
            res = s.readline()
            if (res): responded = True
            res_str = res.decode().strip()
            print('----\n' + res_str + '\n----')
            
            if (cmd == "EXPORT_ALL"):
                # if the previous command was export all, write all lines to it
                if (res_str != "DATA"):
                    f.write(res_str)
                    f.write('\n')
            elif (cmd == "MANUAL_CAPTURE"):
                if (save_flag is None):
                    inpa = input("Do you want to save? Y or n\n").strip()

                    if inpa.lower() == 'y': 
                        open_file("MANUAL")
                        save_flag = True
                    else: save_flag = False
                    
                if save_flag == True:
                    if (res_str != "DATA"):
                        f.write(res_str)
                        f.write('\n')
        # outside the loop means device done sending
        print("Command completed.")
        if (f): f.close();

