from utility import *
import time
import random
import numpy as np

port = SerialPort("COM59", baudrate=921600)
# port = SerialPort("COM59", baudrate=9600)
# NUM_RUNS = 50
NUM_RUNS = 1
try:
    tot_sys_del = tot_fp_del = 0
    total_corr = 0
    for i in range(NUM_RUNS):
        port.reset()
        print(f"================== Performing run {i+1}... ==================")
        corr, fdel, sysdel = rand_test_matrix_fpga(port, N=2) 
        print("Result:","OK" if corr else "FAIL", end=" ")
        print(f"sys: {sysdel:.2f}s, fpga: {fdel:.5f}s")
        tot_sys_del += sysdel
        tot_fp_del += fdel
        if corr:
            total_corr += 1
        time.sleep(0.1)
    print(f"{total_corr}/{NUM_RUNS} correct, total sys: {tot_sys_del:.2f}, total fp: {tot_fp_del:.5f}")

except KeyboardInterrupt:
    port.close()
    exit()
