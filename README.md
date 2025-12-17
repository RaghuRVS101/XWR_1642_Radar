# Capacity Analysis for Multi-Radar Streaming (TI IWR1642BOOST)

This repo contains a **small end-to-end testbed** for studying *capacity* and *queueing delay* when multiple **TI IWR1642BOOST** radars stream detections (x,y) to a **single server**.  
The server models an **M/M/1/K**-style system (Poisson arrivals, exponential service times, finite waiting buffer) and reports:

- **End-to-end delay** per packet (client send timestamp → server processing time)
- **“Corruption” rate** (packets whose delay exceeds a threshold; the server perturbs x,y to emulate stale/invalid data)
- **Loss rate** (packets dropped when the server queue is full)
- **Delay histograms** saved as PNGs per radar

---

## Repository layout

- `server.py` — TCP server + finite queue + service process + final summary + histogram plots
- `radar1.py` — Radar client for **R1** (reads frames over serial, extracts x,y, sends to server with Poisson traffic)
- `radar2.py` — Radar client for **R2**
- `IWR1642_R1_SDK3_config.cfg`, `IWR1642_R2_SDK3_config.cfg` — mmWave configuration files (sent on the CLI port)
- `run_all.bat` — Windows helper to start server + both radars in separate terminals
- `makefile` — **Note:** currently references older filenames; see the Linux/macOS notes below.

---

## How the system works

### Data path (high level)
1. **Radar (IWR1642BOOST)** streams binary frames on the **DATA** serial port (921600 baud by default).
2. Each radar script:
   - detects the **magic word** (`0x0201040306050807`)
   - reads the frame header
   - parses **TLV type 1** (detected points)
   - extracts x,y for detections and **uses the first detection** as payload
3. The radar script sends newline-delimited messages over TCP:
   ```
   RADAR_ID,unix_timestamp,x,y
   ```
4. The server:
   - enqueues each message into a finite queue of length **K**
   - drops packets if the queue is full (**lost**)
   - a background worker processes queued packets with **exponential service time** (rate μ)
   - computes the **end-to-end delay**
   - marks a packet **corrupted** if delay > `upper_threshold` and perturbs x,y

### Queueing model (what is being simulated)
- **Arrivals:** each radar waits `Exp(λ)` seconds between sends (Poisson process per radar)
- **Service:** the server sleeps `Exp(μ)` seconds per job (single server)
- **Buffer:** maximum queue length **K** (`MAX_QUEUE_LEN`)
- **Metrics:** delay distribution, loss, and corruption vs. load (λ relative to μ)

---

## Requirements

- Python 3.9+ recommended
- Packages:
  - `numpy`
  - `matplotlib`
  - `pyserial`

Install:
```bash
pip install numpy matplotlib pyserial
```

Hardware (if using real radars):
- 2× TI **IWR1642BOOST** boards (or 1 board if you only run one client)
- Correct serial ports for each board (CLI + DATA)

---

## Configuration you are expected to edit

### 1) Serial ports per radar
In `radar1.py` / `radar2.py` update:
- `CLI_PORT`
- `DATA_PORT`

Example (Windows):
```python
CLI_PORT = "COM13"
DATA_PORT = "COM14"
```

Example (Linux):
```python
CLI_PORT = "/dev/ttyACM0"
DATA_PORT = "/dev/ttyACM1"
```

### 2) Pick the correct config file per radar
Each radar script sends `CONFIG_FILE` over the CLI port.

- `radar2.py` is already set to `IWR1642_R2_SDK3_config.cfg`
- **Check `radar1.py`**: it currently also points to `IWR1642_R2_SDK3_config.cfg`.  
  If you want different configs per radar, change `radar1.py` to:
```python
CONFIG_FILE = "IWR1642_R1_SDK3_config.cfg"
```

### 3) Traffic + server capacity parameters
**Radar side** (per client):
- `arrival_rate` (λ, packets/second)
- `RUN_DURATION` (seconds)

**Server side**:
- `SERVICE_RATE` (μ, jobs/second)
- `MAX_QUEUE_LEN` (K)
- `upper_threshold` (seconds; delay above this is counted “corrupted”)

---

## Run instructions

### Windows (recommended if your COM ports are Windows-style)
1. Open a terminal and start the server:
   ```bat
   python server.py
   ```
2. In two other terminals, start the radar clients:
   ```bat
   python radar1.py
   python radar2.py
   ```
Or run everything at once:
```bat
run_all.bat
```

### Linux / macOS
Start in three terminals:
```bash
python3 server.py
python3 radar1.py
python3 radar2.py
```

> The provided `makefile` is currently out of sync with the filenames in this repo.  
> You can still run the commands above directly.

---

## Outputs

When all clients disconnect, the server prints a final summary (OK / corrupted / lost per radar) and saves:
- `delay_histogram_R1.png`
- `delay_histogram_R2.png`  
(in the folder where you ran `server.py`)

---

## Troubleshooting

- **`serial.serialutil.SerialException` (port not found / access denied)**  
  Confirm the correct CLI/DATA ports and close any other serial monitors.
- **Nothing is parsed / x,y stays (0.0, 0.0)**  
  The TLV parser expects **TLV type 1** detected points. Ensure your mmWave config enables detected points output.
- **Server shows lots of DROPPED packets**  
  Increase `MAX_QUEUE_LEN`, decrease `arrival_rate`, or increase `SERVICE_RATE`.
- **Histograms not saved**  
  Ensure the server process has permission to write files to the current directory.

---

## What to try next

- Sweep `arrival_rate` across a range and plot **loss vs. load** and **corruption vs. load**
- Extend the payload to include more detections or velocities
- Replace the “delay > threshold” corruption rule with application-specific QoS constraints
