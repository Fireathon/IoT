import serial
ser = serial.Serial('/dev/ttyUSB1', 9600, timeout=1)
while True:
    reading = ser.readline()
    print(reading)
    contents = urllib.request.urlopen(reading).read()
