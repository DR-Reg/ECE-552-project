import serial
import time

# TODO: variable bytesize
class SerialPort:
    def __init__(self, port, bytesize=serial.EIGHTBITS, baudrate=9600, timeout=1):
        self.ser = serial.Serial(
            port = port,
            baudrate=baudrate,
            parity=serial.PARITY_EVEN,
            stopbits=serial.STOPBITS_ONE,
            bytesize=bytesize,
            timeout=timeout
        )
        self.bytesize = bytesize
        self.baudrate = baudrate
        self.timeout = timeout
       
        # Allow for connection to stabilise
        time.sleep(2)
        
        if self.ser.isOpen():
            self.ser.flushInput()
            self.ser.flushOutput()
            print(f"Connected to {port} at {baudrate} baud")
            print("Listening...") 
        else:
            raise Exception(f"Unable to open {port}")

    def read_data(self, result_size, verb=0):
        result = [None for i in range(result_size)]
        while True:
            byte = self.ser.read(4)
            if byte:
                if verb:
                    print("Received:", byte.hex())
                continue
                # Extract result index:
                res_ix = (n & 0xf0) >> 4
                res_val = n & 0x0f
                if res_ix >= result_size:
                    print("Received corrupted data:", n, hex(n), bin(n))
                result[res_ix] = res_val

            # Once we've read all results:
            if len([e for e in result if e is None]) == 0:
                return result
        
    def write_data(self, data):
        for val in data:
            self.ser.write(bytes([val]))
            print(f"Sent {hex(val)}")
            time.sleep(0.01)

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            print(f"\nClosed port {port}")
