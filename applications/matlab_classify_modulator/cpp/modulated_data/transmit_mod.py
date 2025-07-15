import adi
import scipy.io as sio
import os

sdr = adi.adrv9002(uri="ip:192.168.1.169")
# If these error you need an older version of pyadi-iio
# as the driver changed
sdr.tx0_lo = 2.4e9
sdr.tx1_lo = 2.4e9
sdr.tx_hardwaregain_chan0 = -10
sdr.tx_cyclic_buffer = True

# Load the transmit signal from a .mat file
file_name = "input_mod_16QAM.mat"

if not os.path.isfile(file_name):
    print("File not found")
    exit()

mat = sio.loadmat(file_name)
data = mat.get("yc")
data = data.flatten()

sdr.tx(data)

input("Press Enter to stop the transmission")