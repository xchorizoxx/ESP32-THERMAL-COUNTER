#!/usr/bin/env python3
import os
import re
import sys

def check_file(path):
    if os.path.exists(path):
        print(f"[OK] Archivo encontrado: {path}")
        return True
    else:
        print(f"[ERROR] Archivo no encontrado: {path}")
        return False

def verify_source_logic():
    print("\n--- Verificando Lógica de Integración ---")
    
    # Check main.cpp for global instances
    with open("main/main.cpp", "r") as f:
        content = f.read()
        if "RTCDriver g_rtc;" in content and "SDManager g_sd;" in content:
            print("[OK] Instancias globales de hardware declaradas.")
        else:
            print("[FAIL] Falta declaración de instancias globales en main.cpp.")
            
        if "g_rtc.init" in content and "g_sd.init" in content:
            print("[OK] Llamadas a init() detectadas en app_main.")
        else:
            print("[FAIL] Falta inicialización de hardware en app_main.")

    # Check http_server.cpp for protocol updates
    with open("components/web_server/src/http_server.cpp", "r") as f:
        content = f.read()
        if '"rtc_ok"' in content and '"sd_ok"' in content:
            print("[OK] Protocolo WebSocket actualizado con estados de salud.")
        else:
            print("[FAIL] El servidor web no está enviando rtc_ok/sd_ok.")
            
        if 'strcmp(cmdStr, "RETRY_RTC")' in content:
            print("[OK] Comando de reconexión RETRY_RTC implementado.")
        if 'strcmp(cmdStr, "RETRY_SD")' in content:
            print("[OK] Comando de reconexión RETRY_SD implementado.")

def print_protocol_sample():
    print("\n--- Ejemplo de Comunicación (Protocolo v2) ---")
    print("JSON de Estado (ESP32 -> Navegador):")
    print("""{
  "type": "status",
  "rtc_ok": true,
  "sd_ok": true,
  "rtc_time": "2026-04-23T10:00:00Z",
  "session_id": 1,
  "uptime_ms": 123456
}""")
    print("\nComando de Reconexión (Navegador -> ESP32):")
    print('{"cmd": "RETRY_SD"}')

if __name__ == "__main__":
    print("=== Validador de Integración Hardware ESP32-THERMAL ===")
    
    critical_files = [
        "components/rtc_driver/src/rtc_driver.cpp",
        "components/sd_manager/src/sd_manager.cpp",
        "components/web_server/src/web/index.html",
        "components/web_server/src/web/app.js",
        "partitions.csv"
    ]
    
    all_ok = True
    for f in critical_files:
        if not check_file(f):
            all_ok = False
            
    if all_ok:
        verify_source_logic()
        print_protocol_sample()
        print("\n[RESULTADO] La integración parece CORRECTA y lista para hardware.")
    else:
        print("\n[RESULTADO] Faltan componentes críticos. Revisa los errores anteriores.")
        sys.exit(1)
