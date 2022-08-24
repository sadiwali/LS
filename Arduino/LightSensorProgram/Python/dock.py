from socket import timeout
import serial
import serial.tools.list_ports
import time

ports = serial.tools.list_ports.comports()

inp = "";

if (len(ports) == 0):
    print("There are no serial ports available.")
    exit()
else:
    print("Available serial ports:")
    for i in range(len(ports)):
        print("[" + str(i) + "] " + ports[i].name)
        
while True:
    inp = input("Please pick a port to connect to by the number on the left\n");
    if (inp.isnumeric() and int(inp) >= 0 and int(inp) < len(ports)): break
    print("Invalid input. Please try again.")

# open the serial port selected
s = serial.Serial(port=ports[int(inp)].name, baudrate=115200, timeout=10)

while True:
    inp = input("Enter a command to send...")
    inp += '\n';
    s.write(bytes(inp, 'utf-8'))
    time.sleep(0.05)
    
    responded = False
    while not responded or s.in_waiting > 0:
        responded = True;
        res = s.readline()
        res_str = res.decode().strip()
        print(res_str)

