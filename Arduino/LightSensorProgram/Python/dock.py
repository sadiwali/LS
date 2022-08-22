import serial
import serial.tools.list_ports

ports = serial.tools.list_ports.comports()

print([port.name for port in ports])

for port in ports:
    ser = serial.Serial(port.name, 115200, timeout=2)
    ser.write(b'abcdefghijklmn')
    ser.close();