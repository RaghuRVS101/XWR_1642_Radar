# Capacity Analysis of IWR1642 Radar-Based Sensing System

This project implements a small end-to-end testbed to study **capacity and delay** in a radar sensing system using **TI IWR1642BOOST** mmWave radars and a Python-based **queueing server**.

Two radars (R1 and R2) detect objects, extract `(x, y)` coordinates from the raw frames, and send them as timestamped packets to a central server over TCP. The server models a finite-buffer queue with a given **service rate**, logs delays, marks “corrupted” samples when the delay is too high, and finally plots delay histograms for each radar.

---

## 1. System Overview

### Components

- **Radar clients**
  - `radar1.py` – client for Radar R1 :contentReference[oaicite:0]{index=0}  
  - `radar2.py` – client for Radar R2 :contentReference[oaicite:1]{index=1}  
  - Connect to the IWR1642 over two serial ports:
    - CLI port – sends configuration commands (`.cfg` file)
    - DATA port – receives binary radar frames
  - Parse the binary stream, extract detected object positions `(x, y)`, and send them to the server as:
    ```text
    RADAR_ID,timestamp,x,y\n
    ```

- **Queueing server**
  - `server.py` – central TCP server and queue simulator :contentReference[oaicite:2]{index=2}  
  - Accepts connections from multiple radar clients.
  - Buffers incoming `(x, y)` messages in a finite FIFO queue.
  - Serves items at a fixed **service rate**.
  - Logs per-packet delay and classifies samples as:
    - **OK** – delay below threshold  
    - **CORRUPTED** – delay above threshold (coordinates are randomly perturbed)  
    - **LOST** – dropped when queue is full  

- **Configuration files**
  - `IWR1642_R1_SDK3_config.cfg`
  - `IWR1642_R2_SDK3_config.cfg`  
  These specify radar parameters such as chirp configuration, frame rate, etc. (standard TI mmWave cfg format).

- **Utility**
  - `run_all.bat` / `makefile` – helper scripts to run the server and clients (optional for your platform).

---

## 2. Queueing & Capacity Model

The project is designed to illustrate basic queueing theory concepts:

- **Arrival process**
  - Clients send messages at a configurable **arrival rate**:
    ```python
    arrival_rate = 10  # packets/sec
    ```
  - Inter-arrival times are drawn from an **Exponential** distribution (Poisson arrivals):
    ```python
    wait = np.random.exponential(1 / arrival_rate)
    ```

- **Service process**
  - The server processes items at a fixed **service rate**:
    ```python
    SERVICE_RATE = 40  # messages per second
    ```
  - The `queue_processor()` thread pops messages from the queue and sleeps for `1 / SERVICE_RATE` between services.

- **Finite queue**
  - Max length:
    ```python
    MAX_QUEUE_LEN = 30
    ```
  - If the queue is full, packets are counted as **LOST**.

- **Delay & corruption**
  - Per-packet delay = `time_processed - time_queued`.
  - If delay exceeds a threshold (e.g. `upper_threshold = 0.09` seconds), the sample is treated as **corrupted** and `(x, y)` is perturbed by random noise.

- **Capacity metrics**
  - For each radar, after all clients disconnect, the server prints:
    - Number of OK, CORRUPTED, and LOST packets.
    - Error rate (% corrupted) and loss rate (% dropped).
  - Histograms of delays are saved as:
    - `delay_histogram_R1.png`
    - `delay_histogram_R2.png`  
    Example plots are shown at the top of this README.

---

## 3. Repository Structure

```text
.
├── radar1.py                    # Radar client for R1
├── radar2.py                    # Radar client for R2
├── server.py                    # TCP queueing server + statistics
├── IWR1642_R1_SDK3_config.cfg   # Radar configuration for R1
├── IWR1642_R2_SDK3_config.cfg   # Radar configuration for R2
├── run_all.bat                  # (Optional) Windows helper script
├── makefile                     # (Optional) build/run helper
└── README.md                    # This file
