'''
Command-line tool suite for Open Spectral Sensing (OSS) device.
'''

from asyncore import write
from curses import baudrate
from logging import exception
from operator import truediv
from socket import timeout
from tkinter.tix import MAX
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
MIN_WAVELENGTH = 340
MAX_WAVELENGTH = 1010
WAVELENGTH_STEPSIZE = 5

# serial
s = None                                    # the currently selected serial device
instruction = ""                            # the serial instruction to send to device

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
    "PAUSE_COLLECTION": "12",
    "RESUME_COLLECTION": "12",
}

local_functions = [
    "DISCONNECT",
]

def connect_to_device(port_name):
    s = None
    try:
        s = serial.Serial(port=port_name, baudrate=BAUDRATE, timeout=SER_TIMEOUT)
        return s
    except Exception as e:
        return None

# write to all devices, or a specific one if id is supplied
def write_to_device(msg, responses = None, ind = -1, port_name = None, serial_obj = None):
        try:
            if (port_name and serial_obj is None):
                serial_obj = serial.Serial(port=port_name, baudrate=BAUDRATE, timeout=SER_TIMEOUT)
            serial_obj.write(bytes(msg + '\n', 'utf-8'))
            time.sleep(0.1)
            if (responses is not None and ind != -1):
                responses[ind] = True
            else:
                return True
        except Exception as e:
            return False

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
        
    # add the file header
    f.write(file_header())

# produce the file header
def file_header():
    line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,IS_SATURATED,IS_DARK,X,Y,Z,"
    for i in range (MIN_WAVELENGTH, MAX_WAVELENGTH + WAVELENGTH_STEPSIZE, WAVELENGTH_STEPSIZE):
        line += str(i) + ","
    line += "\n"
    return line
    
# cmd is the index displayed to user. The user selects this, and it is corresponded to an instruction
def decode_cmd(cmd):
    return list(instructions.keys())[cmd]

# Pings all serial devices connected, saves responses of valid OSS devices. If response was valid, table contains name, else None
def say_hello(port, response, ind):
    try:
        with serial.Serial(port=port, baudrate=BAUDRATE, timeout=SER_TIMEOUT) as _s:
            _s.write(bytes(instructions["_SAY_HELLO"] + '\n', 'utf-8'))
            time.sleep(0.1)
            
            res = _s.readline().decode().strip()
            
            if res.lower() == "hello":
                # if hello is the first response, second response is the device name
                device_name = _s.readline().decode().strip()
                logging_interval = _s.readline().decode().strip()
                device_status = _s.readline().decode().strip()
                # save the name to the response
                response[ind] = {"port_name": port, "device_name": device_name, "logging_interval": logging_interval, "device_status": device_status}
    except Exception as e:
        pass      
    
def find_devices():
    # list of serial ports connected to the computer
    ports = [None]
    
    while True:
        ports = serial.tools.list_ports.comports()
        
        if len(ports) > 0:
            break
        
        print("There are no serial ports available. Retrying in 1 second.")
        time.sleep(1)
        
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
            devices.append({
                "device_name": responses[i]["device_name"],
                "port_name": responses[i]["port_name"],
                "logging_interval": responses[i]["logging_interval"],
                "device_status": responses[i]["device_status"],
                "serial_object": None
            })
            
    return devices     

def set_date(devices):
    # immediately send a time update command to all connected devices
    date = str(datetime.datetime.now().year).zfill(4) + str(datetime.datetime.now().month).zfill(2) + str(datetime.datetime.now().day).zfill(2) + str(datetime.datetime.now().hour).zfill(2) + str(datetime.datetime.now().minute).zfill(2) + str(datetime.datetime.now().second).zfill(2)
    instruction = "05" + date
    
    # resize threads for each detected OSS device
    threads = [None] * len(devices)
    results = [None] * len(devices
                           )
    # send a serial command to sync time with all connected devices
    for i, device in enumerate(devices):
        threads[i] = Thread(target=write_to_device, args=(instruction, ), kwargs={"port_name": device["port_name"]})
        threads[i].start()
    
    # close the serial device after setting time
    for i, device in enumerate(devices):
        threads[i].join()
        device["serial_object"].close()
        
if __name__ == "__main__":
    
    devices = find_devices()
    set_date(devices)
          
    # start main loop
    while True:
        
        while True:
            print("Detected devices:")
            # first, select a device to communicate with
            for i, key in enumerate(devices):
                print("[" + str(i) + "] " + device["device_name"])
                
            inp = input("Select a device to connect to")
            if (not inp.strip().isnumeric() or int(inp) < 0 or int(inp) > len(devices)):
                print("Invalid entry. Please try again.")
                continue
            
            inp = int(inp.strip())
            
            # S is the selected OSS device
            s = devices[inp]
            # connect to serial
            s["serial_object"] = connect_to_device(s["port_name"])
            
            if (s["serial_object"] is None):
                print("Could not connect, please try again.")
                continue
            
            break
            
            
        while True:
            # second, choose instructions to send to the device selected
            print("Selected device: " + s["device_name"] + " on port: " + s["port_name"] + ". \n Currently RECORDING every ")
            for i, key in enumerate(instructions.keys()):
                print("[" + str(i) + "] " + key)
                
                
            inp = input("Choose a command by entering the number in front\n>")
            if (not inp.strip().isnumeric() or int(inp) < 0 or int(inp) > len(instructions.keys())):
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
            write_to_device(instruction)
            
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
                        if (res_str != "DATA" and res_str != "OK"):
                            f.write(res_str)
                            f.write('\n')
            # outside the loop means device done sending
            print("Command completed.")
            if (f): f.close();

