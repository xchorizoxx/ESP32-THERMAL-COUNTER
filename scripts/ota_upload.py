#!/usr/bin/env python3
"""
ota_upload.py — Flash OTA para Detector de Puerta Térmica
----------------------------------------------------------
Uso:
    python scripts/ota_upload.py [IP]        # IP por defecto: 192.168.4.1 (SoftAP)
    python scripts/ota_upload.py 192.168.1.5 # Red distinta

Requisitos: Python 3.6+ (stdlib únicamente, sin dependencias externas)
"""

import sys
import os
import time
import http.client
import urllib.error

# ===========================================================================
#  CONFIGURACIÓN
# ===========================================================================
DEFAULT_IP      = "192.168.4.1"          # IP del SoftAP del ESP32
OTA_ENDPOINT    = "/update"
TIMEOUT_S       = 90                     # segundos — firmware ~625 KB @ WiFi 11Mbps
CHUNK_SIZE      = 4096                   # bytes por chunk para mostrar progreso
# El archivo .bin se llama igual que el project_name en CMakeLists.txt
BIN_NAME        = "DetectorPuerta.bin"   # MUST match idf project name exactly

# ANSI colors (Windows 10+ y todas las terminales modernas)
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

    # Localizar el .bin relativo a la raíz del proyecto
    project_root = os.path.normpath(os.path.join(os.path.dirname(__file__), ".."))
    bin_path     = os.path.join(project_root, "build", BIN_NAME)

    print(f"\n{CYAN}╔══════════════════════════════════════════════════╗")
    print(f"║    OTA FLASH — Detector de Puerta Térmica        ║")
    print(f"╚══════════════════════════════════════════════════╝{RESET}")
    print(f"  Target : {YELLOW}http://{esp_ip}{OTA_ENDPOINT}{RESET}")
    print(f"  Firmware: {DIM}{bin_path}{RESET}\n")

    if not os.path.exists(bin_path):
        print(f"{RED}✖  No se encontró el firmware:{RESET}  {bin_path}")
        print("   → Compila el proyecto primero (ESP-IDF: Build)\n")
        sys.exit(1)

    total_bytes = os.path.getsize(bin_path)
    mtime       = os.path.getmtime(bin_path)
    age_s       = time.time() - mtime
    print(f"  Tamaño : {total_bytes/1024:.1f} KB")
    print(f"  Creado : hace {age_s/60:.0f} min")

    if age_s > 600:
        print(f"\n{YELLOW}⚠  El .bin tiene más de 10 minutos. ¿Compilaste antes de flashear?{RESET}")

    print()

    # Leer binario en memoria (625 KB — cabe perfectamente)
    with open(bin_path, "rb") as f:
        firmware = f.read()

    # Conexión HTTP manual para poder mostrar progreso real
    conn = http.client.HTTPConnection(esp_ip, timeout=TIMEOUT_S)

    try:
        print(f"  Enviando firmware...")
        t_start = time.time()

        conn.connect()
        conn.putrequest("POST", OTA_ENDPOINT)
        conn.putheader("Content-Type",   "application/octet-stream")
        conn.putheader("Content-Length", str(len(firmware)))
        conn.endheaders()

        # Enviar en chunks con barra de progreso
        sent = 0
        for i in range(0, len(firmware), CHUNK_SIZE):
            chunk = firmware[i : i + CHUNK_SIZE]
            conn.sock.sendall(chunk)
            sent += len(chunk)
            print(f"\r  {progress_bar(sent, len(firmware))}", end="", flush=True)

        print()  # nueva línea tras la barra

        # Esperar respuesta del servidor
        resp      = conn.getresponse()
        resp_body = resp.read().decode("utf-8", errors="replace").strip()
        elapsed   = time.time() - t_start

        if resp.status == 200:
            speed = (len(firmware) / 1024) / elapsed
            print(f"\n  {GREEN}✔  Flash exitoso — {resp_body}{RESET}")
            print(f"  {DIM}Velocidad: {speed:.1f} KB/s  Tiempo: {elapsed:.1f}s{RESET}")
            print(f"\n  {CYAN}El ESP32 se está reiniciando... espera ~5s y reconecta.{RESET}\n")
        else:
            print(f"\n  {RED}✖  Error HTTP {resp.status}: {resp_body}{RESET}\n")
            sys.exit(1)

    except (ConnectionRefusedError, TimeoutError, OSError) as e:
        print(f"\n\n  {RED}✖  Sin conexión al ESP32 en {esp_ip}{RESET}")
        print(f"  {DIM}¿Estás conectado al Wi-Fi 'ThermalCounter'?  Detalle: {e}{RESET}\n")
        sys.exit(1)
    except Exception as e:
        print(f"\n\n  {RED}✖  Error inesperado: {e}{RESET}\n")
        sys.exit(1)
    finally:
        conn.close()

if __name__ == "__main__":
    main()
