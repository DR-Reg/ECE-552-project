from utility import *

port = SerialPort("COM59")
while True:
    try:
        # port.write_data([0xDA, 0x22, 0x1D])
        port.read_data(10, verb = 1)
    except KeyboardInterrupt:
        port.close()
# result = read_data("COM59", 2, verb=1)
# print("Received:", result)
