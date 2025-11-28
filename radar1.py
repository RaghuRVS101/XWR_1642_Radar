
import binascii
import codecs
import socket
import struct
import time

import numpy as np
import serial

# === CONFIG ===
SERVER_IP = '127.0.0.1'
SERVER_PORT = 9000
RADAR_ID = 'R1'
CLI_PORT = "COM13"
DATA_PORT = "COM14"
CLI_BAUD = 115200
DATA_BAUD = 921600
CONFIG_FILE = "IWR1642_R2_SDK3_config.cfg"
MAGIC_WORD = b'\x02\x01\x04\x03\x06\x05\x08\x07'
HEADER_SIZE = 40
RUN_DURATION = 30
arrival_rate = 10  # packets/sec

def getUint32(data):
    return data[0] + (data[1] << 8) + (data[2] << 16) + (data[3] << 24)

def checkMagicPattern(data):
    return int(data[0:8] == MAGIC_WORD)

def parser_get_xy(data, readNumBytes):
    headerStartIndex = -1
    headerNumBytes = 40

    for idx in range(readNumBytes - 8):
        if checkMagicPattern(data[idx:idx+8]):
            headerStartIndex = idx
            break

    if headerStartIndex == -1:
        return []

    totalPacketNumBytes = getUint32(data[headerStartIndex+12:headerStartIndex+16])
    numDetObj = getUint32(data[headerStartIndex+28:headerStartIndex+32])
    numTlv = getUint32(data[headerStartIndex+32:headerStartIndex+36])
    tlvStart = headerStartIndex + headerNumBytes
    tlvType = getUint32(data[tlvStart:tlvStart+4])
    offset = 8

    xy_coords = []
    if tlvType == 1 and numDetObj > 0:
        for _ in range(numDetObj):
            try:
                x_bytes = data[tlvStart + offset : tlvStart + offset + 4]
                y_bytes = data[tlvStart + offset + 4 : tlvStart + offset + 8]
                x = struct.unpack('<f', codecs.decode(binascii.hexlify(x_bytes), 'hex'))[0]
                y = struct.unpack('<f', codecs.decode(binascii.hexlify(y_bytes), 'hex'))[0]
                xy_coords.append((x, y))
                offset += 16
            except:
                break
    return xy_coords

def connect_to_radar(cli_port, data_port):
    try:
        cli = serial.Serial(cli_port, CLI_BAUD)
        data = serial.Serial(data_port, DATA_BAUD)
        print(f"[CONNECTED] CLI: {cli_port}, DATA: {data_port}")
        return cli, data
    except Exception as e:
        print(f"[ERROR] Could not connect to radar: {e}")
        return None, None

def send_configuration(cli, config_file):
    try:
        with open(config_file, 'r') as f:
            for line in f:
                cmd = line.strip()
                if cmd:
                    cli.write((cmd + '\n').encode())
                    time.sleep(0.1)
        print(f"[CONFIGURED] Radar using {config_file}")
    except Exception as e:
        print(f"[ERROR] Failed to send config: {e}")

def main():
    cli, data = connect_to_radar(CLI_PORT, DATA_PORT)
    if not cli or not data:
        return

    send_configuration(cli, CONFIG_FILE)
    time.sleep(1)

    leftover = bytearray()
    start_time = time.time()

    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        client_socket.connect((SERVER_IP, SERVER_PORT))
        print(f"[CONNECTED] to server at {SERVER_IP}:{SERVER_PORT}")

        while time.time() - start_time < RUN_DURATION:
            if data.in_waiting > 0:
                chunk = data.read(data.in_waiting)
                leftover += chunk

                while True:
                    idx = leftover.find(MAGIC_WORD)
                    if idx == -1 or len(leftover) - idx < HEADER_SIZE:
                        break

                    total_len = getUint32(leftover[idx+12:idx+16])
                    if len(leftover) >= idx + total_len:
                        frame = leftover[idx:idx+total_len]
                        xy_coords = parser_get_xy(frame, len(frame))
                        for x, y in xy_coords:
                            timestamp = time.time()
                            msg = f"{RADAR_ID},{timestamp},{x:.2f},{y:.2f}\n"
                            client_socket.sendall(msg.encode('utf-8'))
                            print(f"[SENT] {msg.strip()}")

                            wait = np.random.exponential(1 / arrival_rate)
                            time.sleep(wait)

                        leftover = leftover[idx + total_len:]
                    else:
                        break
    except Exception as e:
        print(f"[ERROR] Communication failed: {e}")
    finally:
        cli.close()
        data.close()
        client_socket.close()
        print("[CLOSED] Radar + socket closed.")

if __name__ == "__main__":
    main()
