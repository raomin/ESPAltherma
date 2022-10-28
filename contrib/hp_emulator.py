"""
Heat pump simulator.

Can be done locally using com0com driver on windows: https://sourceforge.net/projects/com0com/

Create a virtual COM ports pair (COM6/7 in this example).
Connect Daikin DChecker to COM7, this script to COM6 (simulating the HP).
"""

import time
import serial

def main():
    ser = serial.Serial('COM8', 9600, timeout=0.1,
                    parity=serial.PARITY_EVEN, stopbits=1)
    while True:
        data = ser.read(100)
        if data:
            print(f"Received: {data}") 
            reply = None
            # Reply to S protocol
            if data == b'\x02\x53\xaa':
                print("Match 53!")
                reply = b"\x53\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xab"
            elif data == b'\x02\x50\xad':
                print("Match 50!")
                # 6 only
                reply = b"\x50\x01\x00\x00\x00\xae"
            elif data == b'\x02\x54\xa9':
                print("Match 54!")
                reply = b"\x54\xb4\x0d\xa8\x17\xd0\x18\x24\x1d\x3c\x2c\x01\x00\x20\x00\x00\x00\x79"
            elif data == b'\x02\x55\xa8':
                print("Match 55!")
                reply = b"\x55\x01\x00\x00\x00\x00\x1e\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x8b"
            elif data == b'\x02\x56\xa7':
                print("Match 56!")
                # 6 only
                reply = b"\x56\x01\x00\x00\x00\xa8"
            else:
                print("Error reply...")
                reply = b"\x15\xea"

            if reply:
                ser.write(reply)

        else:
            time.sleep(0.1)

if __name__ == "__main__":
    main()

