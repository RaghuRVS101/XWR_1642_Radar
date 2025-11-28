import os
import random
import socket
import threading
import time
from collections import defaultdict, deque

import matplotlib.pyplot as plt
import numpy as np
from scipy.stats import rayleigh

# === CONFIG ===
HOST = '127.0.0.1'
PORT = 9000
SERVICE_RATE = 40
upper_threshold = 0.09
MAX_QUEUE_LEN = 30

# === Shared State ===
message_queue = deque(maxlen=MAX_QUEUE_LEN)
queue_lock = threading.Lock()
client_count = 0
client_count_lock = threading.Lock()

corruption_counts = defaultdict(lambda: {'ok': 0, 'corrupted': 0, 'lost': 0})
delay_logs = defaultdict(list)   # << stores delays per radar


def queue_processor():
    while True:
        with queue_lock:
            if message_queue:
                radar_id, time_queued, x, y = message_queue.popleft()
            else:
                radar_id = None

        if radar_id:
            time_processed = time.time()
            delay = time_processed - time_queued

            # store delay
            delay_logs[radar_id].append(delay)

            # corruption rule (your version)
            corrupted = delay > upper_threshold

            if corrupted:
                x += random.uniform(-1, 1)
                y += random.uniform(-1, 1)

            with client_count_lock:
                corruption_counts[radar_id]['corrupted' if corrupted else 'ok'] += 1

            print(f"[{radar_id}] Delay: {delay:.3f}s | "
                  f"({x:.2f}, {y:.2f}) | Status: {'CORRUPTED' if corrupted else 'OK'}")

        time.sleep(1 / SERVICE_RATE)


def handle_client(conn, addr):
    global client_count
    with client_count_lock:
        client_count += 1
        print(f"[CLIENTS] Connected clients: {client_count}")

    buffer = ""
    try:
        with conn:
            while True:
                data = conn.recv(1024)
                if not data:
                    break

                buffer += data.decode('utf-8')
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    try:
                        radar_id, timestamp_str, x_str, y_str = line.strip().split(',')
                        x = float(x_str)
                        y = float(y_str)

                        now = time.time()

                        with queue_lock:
                            if len(message_queue) < MAX_QUEUE_LEN:
                                message_queue.append((radar_id, now, x, y))
                            else:
                                corruption_counts[radar_id]['lost'] += 1
                                print(f"[{radar_id}] Packet DROPPED due to full queue")

                    except Exception as e:
                        print(f"[ERROR] Invalid message: {e}")

    finally:
        with client_count_lock:
            client_count -= 1
            print(f"[CLIENTS] Disconnected: {addr}, Remaining: {client_count}")

            if client_count == 0:
                print("\n=== FINAL SUMMARY ===")
                for radar_id, counts in corruption_counts.items():
                    total = counts['ok'] + counts['corrupted']
                    total_received = total + counts['lost']
                    err_rate = (counts['corrupted'] / total) * 100 if total > 0 else 0
                    loss_rate = (counts['lost'] / total_received) * 100 if total_received > 0 else 0

                    print(f">> {radar_id}: OK = {counts['ok']}, Corrupted = {counts['corrupted']}, "
                          f"Lost = {counts['lost']}, Total = {total_received} | "
                          f"Error Rate = {err_rate:.2f}%, Loss Rate = {loss_rate:.2f}%")

                plot_histograms()
                os._exit(0)


def plot_histograms():
    print("\n[PLOTTING] Delay histograms for each radar...")

    for radar_id, delays in delay_logs.items():

        if len(delays) == 0:
            print(f"[SKIPPED] No delays for {radar_id}")
            continue

        plt.figure()
        plt.hist(delays, bins=30, edgecolor='black', alpha=0.7)
        plt.title(f"Delay Histogram for {radar_id}")
        plt.xlabel("Delay (seconds)")
        plt.ylabel("Frequency")
        plt.grid(True)
        plt.tight_layout()

        filename = f"delay_histogram_{radar_id}.png"
        plt.savefig(filename)

        print(f"[SAVED] {filename}")

    print("[DONE] Histograms saved.\n")


def main():
    print(f"[STARTING] Server on {HOST}:{PORT}")
    threading.Thread(target=queue_processor, daemon=True).start()

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((HOST, PORT))
    server.listen(5)
    print("[LISTENING] Waiting for clients...")

    try:
        while True:
            conn, addr = server.accept()
            threading.Thread(target=handle_client, args=(conn, addr)).start()
    except KeyboardInterrupt:
        print("[SHUTTING DOWN]")
        server.close()


if __name__ == "__main__":
    main()
