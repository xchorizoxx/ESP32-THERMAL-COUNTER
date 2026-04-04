#!/usr/bin/env python3
"""
ota_upload.py — OTA Flash for Thermal Door Detector
----------------------------------------------------------
Usage:
    python scripts/ota_upload.py [IP]        # Default IP: 192.168.4.1 (SoftAP)
    python scripts/ota_upload.py 192.168.1.5 # Different network
    
Requirements: Python 3.6+ (stdlib only, no external dependencies)
"""

import sys
import os
import time
import http.client
import urllib.error

# ===========================================================================
#  CONFIGURATION
# ===========================================================================
DEFAULT_IP      = "192.168.4.1"          # ESP32 SoftAP IP
OTA_ENDPOINT    = "/update"
TIMEOUT_S       = 90                     # seconds — firmware ~625 KB @ WiFi 11Mbps
CHUNK_SIZE      = 4096                   # bytes per chunk to show progress
# The .bin file name matches the project_name in CMakeLists.txt
BIN_NAME        = "DetectorPuerta.bin"   # MUST match idf project name exactly

# ANSI colors (Windows 10+ and all modern terminals)
GREEN  = "\033[92m"
YELLOW = "\033[93m"
RED    = "\033[91m"
CYAN   = "\033[96m"
DIM    = "\033[2m"
RESET  = "\033[0m"

def progress_bar(done: int, total: int, width: int = 40) -> str:
    pct   = done / total
    filled = int(width * pct)
    bar   = "█" * filled + "░" * (width - filled)
    return f"[{bar}] {pct*100:5.1f}%  {done//1024:4d}/{total//1024} KB"

def main():
    esp_ip = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_IP

    # Locate the .bin relative to project root
    project_root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    bin_path     = os.path.join(project_root, "build", BIN_NAME)

    print(f"\n{CYAN}╔══════════════════════════════════════════════════╗")
    print(f"║    OTA FLASH — Thermal Door Detector             ║")
    print(f"╚══════════════════════════════════════════════════╝{RESET}")
    print(f"  Target : {YELLOW}http://{esp_ip}{OTA_ENDPOINT}{RESET}")
    print(f"  Firmware: {DIM}{bin_path}{RESET}\n")

    if not os.path.exists(bin_path):
        print(f"{RED}✖  Firmware not found:{RESET}  {bin_path}")
        print("   → Build the project first (ESP-IDF: Build)\n")
        sys.exit(1)

    total_bytes = os.path.getsize(bin_path)
    mtime       = os.path.getmtime(bin_path)
    age_s       = time.time() - mtime
    print(f"  Size   : {total_bytes/1024:.1f} KB")
    print(f"  Created: {age_s/60:.0f} min ago")

    if age_s > 600:
        print(f"\n{YELLOW}⚠  The .bin is more than 10 minutes old. Did you build before flashing?{RESET}")

    print()

    # Read binary into memory (625 KB — fits perfectly)
    with open(bin_path, "rb") as f:
        firmware = f.read()

    # Manual HTTP connection to show real-time progress
    conn = http.client.HTTPConnection(esp_ip, timeout=TIMEOUT_S)

    try:
        print(f"  Sending firmware...")
        t_start = time.time()

        conn.connect()
        conn.putrequest("POST", OTA_ENDPOINT)
        conn.putheader("Content-Type",   "application/octet-stream")
        conn.putheader("Content-Length", str(len(firmware)))
        conn.endheaders()

        # Send in chunks with progress bar
        sent = 0
        for i in range(0, len(firmware), CHUNK_SIZE):
            chunk = firmware[i : i + CHUNK_SIZE]
            conn.sock.sendall(chunk)
            sent += len(chunk)
            print(f"\r  {progress_bar(sent, len(firmware))}", end="", flush=True)

        print()  # new line after the bar

        # Wait for server response
        resp      = conn.getresponse()
        resp_body = resp.read().decode("utf-8", errors="replace").strip()
        elapsed   = time.time() - t_start

        if resp.status == 200:
            speed = (len(firmware) / 1024) / elapsed
            print(f"\n  {GREEN}✔  Flash successful — {resp_body}{RESET}")
            print(f"  {DIM}Speed: {speed:.1f} KB/s  Time: {elapsed:.1f}s{RESET}")
            print(f"\n  {CYAN}The ESP32 is rebooting... wait ~5s and reconnect.{RESET}\n")
        else:
            print(f"\n  {RED}✖  HTTP Error {resp.status}: {resp_body}{RESET}\n")
            sys.exit(1)

    except (ConnectionRefusedError, TimeoutError, OSError) as e:
        print(f"\n\n  {RED}✖  No connection to ESP32 at {esp_ip}{RESET}")
        print(f"  {DIM}Are you connected to the 'ThermalCounter' Wi-Fi?  Detail: {e}{RESET}\n")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n  {RED}✖  Unexpected error: {e}{RESET}\n")
        sys.exit(1)
    finally:
        conn.close()

if __name__ == "__main__":
    main()
