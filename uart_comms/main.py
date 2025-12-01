from utility import *
import time
import random
import numpy as np

# port.close()
# REMEMBER: only send in groups of 4 bytes!
# port.write_data([0xBA, 0xDB, 0xAD, 0xFF])

N = 8
vector = [random.randint(0, 100) for i in range(N)]
matrix = [[random.randint(0, 100) for i in range(N)] for j in range(N)]
npmat = np.array(matrix).T
npvec = np.array(vector)
nresult = np.matmul(npmat, npvec)
print(vector)
print(matrix)
print()
print(nresult)

port = SerialPort("COM59", baudrate=921600)
try:
    # remember matrix should come in transposed
    sys_start = time.time()
    # start_time = port.write_data([2,1], [[2,4],[3,5]], 2)
    start_time = port.write_data(vector, matrix, N)
    result, end_time = port.read_data(N, verb = 1)
    sys_end = time.time()
    
    fpga_delta = end_time - start_time
    sys_delta = sys_end - sys_start
    print("Received:", result, "sys: %.2f s, fpga: %.6f s" % (sys_delta, fpga_delta))
    print("Expected:", nresult)
except KeyboardInterrupt:
    port.close()
    exit()
