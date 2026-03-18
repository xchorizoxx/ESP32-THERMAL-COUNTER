#pragma once

// =============================================================================
//  web_ui_html.h  —  Dashboard HUD Táctico Térmico
//  Generado para ESP32-S3 + MLX90640 — Detector de Puerta
// =============================================================================

const char *WEB_UI_HTML = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>THERMAL COUNTER</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;700&family=Orbitron:wght@700;900&display=swap" rel="stylesheet">
    <style>
        /* =====================================================================
           DESIGN TOKENS
           ===================================================================== */
        :root {
            --bg-primary:    #070c18;
            --bg-panel:      #0d1526;
            --bg-panel-gls:  rgba(13, 21, 38, 0.85);
            --border-dim:    #1e2d4a;
            --border-glow:   #1a3a6e;
            --text-primary:  #c8d8f0;
            --text-muted:    #4a6080;
            --text-mono:     'JetBrains Mono', monospace;
            --text-hud:      'Orbitron', sans-serif;

            --neon-green:    #00ff88;
            --neon-green-bg: rgba(0, 255, 136, 0.08);
            --neon-green-glow: rgba(0, 255, 136, 0.4);

            --neon-cyan:     #00d4ff;
            --neon-cyan-bg:  rgba(0, 212, 255, 0.08);
            --neon-cyan-glow: rgba(0, 212, 255, 0.4);

            --neon-amber:    #ffb300;
            --neon-red:      #ff3d5a;
            --neon-red-bg:   rgba(255, 61, 90, 0.10);

            --track-color:   #00ff88;
            --line-entry:    #00ff88;
            --line-exit:     #00d4ff;

            --transition-fast: 150ms ease;
            --transition-med: 300ms ease;
        }

        /* =====================================================================
           RESET & BASE
           ===================================================================== */
        *, *::before, *::after {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            background-color: var(--bg-primary);
            color: var(--text-primary);
            font-family: var(--text-mono);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            overflow-x: hidden;
            background-image:
                radial-gradient(ellipse at 20% 0%, rgba(0,100,200,0.06) 0%, transparent 60%),
                radial-gradient(ellipse at 80% 100%, rgba(0,180,100,0.04) 0%, transparent 50%);
        }

        /* =====================================================================
           TOP BAR
           ===================================================================== */
        #top-bar {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 10px 20px;
            background: rgba(10, 20, 40, 0.95);
            border-bottom: 1px solid var(--border-glow);
            backdrop-filter: blur(8px);
            position: sticky;
            top: 0;
            z-index: 100;
            gap: 12px;
            flex-wrap: wrap;
        }

        #top-bar h1 {
            font-family: var(--text-hud);
            font-size: clamp(0.9rem, 2vw, 1.2rem);
            font-weight: 900;
            letter-spacing: 0.15em;
            color: #fff;
            text-shadow: 0 0 14px rgba(0, 212, 255, 0.6);
            white-space: nowrap;
        }

        .top-badges {
            display: flex;
            gap: 10px;
            align-items: center;
            flex-wrap: wrap;
        }

        .badge {
            display: flex;
            align-items: center;
            gap: 7px;
            padding: 5px 12px;
            background: var(--bg-panel);
            border: 1px solid var(--border-dim);
            border-radius: 20px;
            font-size: 0.75rem;
            letter-spacing: 0.05em;
            white-space: nowrap;
        }

        .dot {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            background: var(--neon-red);
            flex-shrink: 0;
            transition: background var(--transition-med);
        }
        .dot.online  { background: var(--neon-green); box-shadow: 0 0 6px var(--neon-green-glow); animation: pulse-dot 2s infinite; }
        .dot.warning { background: var(--neon-amber); box-shadow: 0 0 6px rgba(255,179,0,0.5); }

        @keyframes pulse-dot {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.4; }
        }

        #fps-display {
            font-family: var(--text-mono);
            font-size: 0.72rem;
            color: var(--text-muted);
        }

        /* =====================================================================
           MAIN LAYOUT GRID
           ===================================================================== */
        #app-grid {
            display: grid;
            grid-template-columns: 180px 1fr 260px;
            grid-template-rows: auto;
            gap: 12px;
            padding: 12px;
            flex: 1;
            min-height: 0;
            align-items: start;
        }

        @media (max-width: 1100px) {
            #app-grid {
                grid-template-columns: 1fr 1fr;
                grid-template-rows: auto auto;
            }
            #panel-metrics  { grid-column: 1 / 3; grid-row: 1; display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
            #panel-viewer   { grid-column: 1 / 3; grid-row: 2; }
            #panel-calib    { grid-column: 1 / 3; grid-row: 3; }
        }

        @media (max-width: 640px) {
            #app-grid {
                grid-template-columns: 1fr;
                padding: 8px;
            }
            #panel-metrics { grid-column: 1; }
        }

        /* =====================================================================
           PANEL BASE
           ===================================================================== */
        .panel {
            background: var(--bg-panel-gls);
            border: 1px solid var(--border-glow);
            border-radius: 10px;
            padding: 14px;
            backdrop-filter: blur(6px);
            box-shadow: 0 4px 24px rgba(0,0,0,0.5), inset 0 1px 0 rgba(255,255,255,0.03);
        }

        .panel-title {
            font-family: var(--text-hud);
            font-size: 0.6rem;
            letter-spacing: 0.2em;
            color: var(--text-muted);
            text-transform: uppercase;
            margin-bottom: 12px;
            padding-bottom: 6px;
            border-bottom: 1px solid var(--border-dim);
        }

        /* =====================================================================
           METRICS PANEL — Left
           ===================================================================== */
        #panel-metrics {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .counter-card {
            flex: 1;
            border-radius: 10px;
            padding: 16px 12px;
            text-align: center;
            position: relative;
            overflow: hidden;
            transition: transform var(--transition-fast);
        }
        .counter-card::before {
            content: '';
            position: absolute;
            inset: 0;
            border-radius: 10px;
            border: 1px solid transparent;
            pointer-events: none;
        }
        .counter-card:hover { transform: scale(1.02); }

        .counter-card.in {
            background: var(--neon-green-bg);
            border: 1px solid rgba(0, 255, 136, 0.25);
            box-shadow: 0 0 20px rgba(0, 255, 136, 0.07), inset 0 0 20px rgba(0, 255, 136, 0.04);
        }
        .counter-card.out {
            background: var(--neon-cyan-bg);
            border: 1px solid rgba(0, 212, 255, 0.25);
            box-shadow: 0 0 20px rgba(0, 212, 255, 0.07), inset 0 0 20px rgba(0, 212, 255, 0.04);
        }

        .counter-label {
            font-family: var(--text-hud);
            font-size: 0.55rem;
            letter-spacing: 0.25em;
            margin-bottom: 8px;
        }
        .counter-card.in  .counter-label { color: var(--neon-green); }
        .counter-card.out .counter-label { color: var(--neon-cyan); }

        .counter-value {
            font-family: var(--text-hud);
            font-size: clamp(2rem, 4vw, 3.5rem);
            font-weight: 900;
            line-height: 1;
            display: block;
            transition: transform 0.15s ease, opacity 0.15s ease;
        }
        .counter-card.in  .counter-value { color: var(--neon-green); text-shadow: 0 0 20px var(--neon-green-glow); }
        .counter-card.out .counter-value { color: var(--neon-cyan);  text-shadow: 0 0 20px var(--neon-cyan-glow); }

        .counter-value.bump {
            animation: counter-bump 0.3s ease;
        }
        @keyframes counter-bump {
            0%   { transform: scale(1.0); }
            40%  { transform: scale(1.15); }
            100% { transform: scale(1.0); }
        }

        .counter-sublabel {
            font-size: 0.65rem;
            color: var(--text-muted);
            margin-top: 6px;
        }

        /* Sensor Alert */
        #sensor-alert {
            display: none;
            background: var(--neon-red-bg);
            border: 1px solid var(--neon-red);
            color: var(--neon-red);
            border-radius: 8px;
            padding: 10px;
            font-size: 0.72rem;
            letter-spacing: 0.05em;
            text-align: center;
            font-weight: 700;
            animation: blink-alert 1.5s step-end infinite;
        }
        @keyframes blink-alert {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.5; }
        }

        /* =====================================================================
           VIEWER PANEL — Center
           ===================================================================== */
        #panel-viewer {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }

        /* HUD Canvas Frame */
        .canvas-frame {
            position: relative;
            background: #000;
            border-radius: 6px;
            overflow: hidden;
            border: 1px solid var(--border-glow);
            box-shadow: 0 0 30px rgba(0,100,200,0.15);
        }

        /* Corner HUD brackets */
        .canvas-frame::before,
        .canvas-frame::after,
        .canvas-frame .corner-bl,
        .canvas-frame .corner-br {
            content: '';
            position: absolute;
            width: 16px;
            height: 16px;
            z-index: 10;
            pointer-events: none;
        }
        .canvas-frame::before {
            top: 6px; left: 6px;
            border-top: 2px solid var(--neon-cyan);
            border-left: 2px solid var(--neon-cyan);
            box-shadow: -2px -2px 6px rgba(0,212,255,0.3);
        }
        .canvas-frame::after {
            top: 6px; right: 6px;
            border-top: 2px solid var(--neon-cyan);
            border-right: 2px solid var(--neon-cyan);
            box-shadow: 2px -2px 6px rgba(0,212,255,0.3);
        }
        .corner-bl {
            bottom: 6px; left: 6px;
            border-bottom: 2px solid var(--neon-cyan);
            border-left: 2px solid var(--neon-cyan);
            box-shadow: -2px 2px 6px rgba(0,212,255,0.3);
        }
        .corner-br {
            bottom: 6px; right: 6px;
            border-bottom: 2px solid var(--neon-cyan);
            border-right: 2px solid var(--neon-cyan);
            box-shadow: 2px 2px 6px rgba(0,212,255,0.3);
        }

        #display-canvas {
            display: block;
            width: 100%;
            image-rendering: -webkit-optimize-contrast;
            image-rendering: smooth;
        }

        /* Scanline overlay */
        .canvas-frame .scanlines {
            position: absolute;
            inset: 0;
            background: repeating-linear-gradient(
                0deg,
                transparent,
                transparent 2px,
                rgba(0,0,0,0.06) 2px,
                rgba(0,0,0,0.06) 4px
            );
            pointer-events: none;
            z-index: 5;
        }

        /* No-signal overlay */
        #no-signal {
            position: absolute;
            inset: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            background: rgba(7, 12, 24, 0.92);
            z-index: 20;
            font-family: var(--text-hud);
            color: var(--text-muted);
            gap: 8px;
        }
        #no-signal span { font-size: 2rem; }
        #no-signal p    { font-size: 0.7rem; letter-spacing: 0.2em; }

        /* Canvas info bar */
        .canvas-info {
            display: flex;
            justify-content: space-between;
            font-size: 0.65rem;
            color: var(--text-muted);
            padding: 0 4px;
        }

        /* =====================================================================
           TOGGLE ROW
           ===================================================================== */
        .toggle-row {
            display: flex;
            flex-wrap: wrap;
            gap: 8px;
        }

        .toggle-item {
            display: flex;
            align-items: center;
            gap: 6px;
            cursor: pointer;
            user-select: none;
            padding: 5px 10px;
            background: var(--bg-panel);
            border: 1px solid var(--border-dim);
            border-radius: 20px;
            font-size: 0.70rem;
            transition: border-color var(--transition-fast), background var(--transition-fast);
        }
        .toggle-item:hover { border-color: var(--neon-cyan); }
        .toggle-item.active {
            background: rgba(0,212,255,0.08);
            border-color: var(--neon-cyan);
            color: var(--neon-cyan);
        }
        .toggle-item input[type="checkbox"] { display: none; }
        .toggle-dot {
            width: 7px;
            height: 7px;
            border-radius: 50%;
            background: var(--text-muted);
            transition: background var(--transition-fast), box-shadow var(--transition-fast);
        }
        .toggle-item.active .toggle-dot {
            background: var(--neon-cyan);
            box-shadow: 0 0 5px var(--neon-cyan-glow);
        }

        /* =====================================================================
           CALIBRATION PANEL — Right
           ===================================================================== */
        #panel-calib {
            display: flex;
            flex-direction: column;
            gap: 0;
        }

        .calib-section {
            margin-bottom: 14px;
        }

        .calib-section-title {
            font-size: 0.60rem;
            letter-spacing: 0.15em;
            color: var(--text-muted);
            text-transform: uppercase;
            margin-bottom: 10px;
            padding-bottom: 5px;
            border-bottom: 1px solid var(--border-dim);
        }

        .slider-group {
            margin-bottom: 12px;
        }

        .slider-header {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            margin-bottom: 4px;
        }

        .slider-label {
            font-size: 0.68rem;
            color: var(--text-primary);
        }

        .slider-value {
            font-family: var(--text-mono);
            font-size: 0.75rem;
            color: var(--neon-cyan);
            font-weight: 700;
            min-width: 40px;
            text-align: right;
        }

        input[type="range"] {
            -webkit-appearance: none;
            appearance: none;
            width: 100%;
            height: 4px;
            background: var(--border-dim);
            border-radius: 4px;
            outline: none;
            cursor: pointer;
        }
        input[type="range"]::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 14px;
            height: 14px;
            border-radius: 50%;
            background: var(--neon-cyan);
            box-shadow: 0 0 6px var(--neon-cyan-glow);
            cursor: pointer;
            transition: transform var(--transition-fast);
        }
        input[type="range"]::-webkit-slider-thumb:hover { transform: scale(1.3); }
        input[type="range"]::-moz-range-thumb {
            width: 14px;
            height: 14px;
            border-radius: 50%;
            background: var(--neon-cyan);
            border: none;
            cursor: pointer;
        }

        /* Track fill visual */
        input[type="range"]::-webkit-slider-runnable-track { border-radius: 4px; }

        /* =====================================================================
           BUTTONS
           ===================================================================== */
        .btn-row {
            display: flex;
            flex-direction: column;
            gap: 7px;
            margin-top: 4px;
        }

        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
            width: 100%;
            padding: 9px 14px;
            border: 1px solid transparent;
            border-radius: 7px;
            font-family: var(--text-mono);
            font-size: 0.72rem;
            font-weight: 700;
            letter-spacing: 0.08em;
            cursor: pointer;
            transition: all var(--transition-fast);
            position: relative;
            overflow: hidden;
        }
        .btn:active { transform: scale(0.97); }
        .btn::after {
            content: '';
            position: absolute;
            inset: 0;
            background: rgba(255,255,255,0);
            transition: background var(--transition-fast);
        }
        .btn:hover::after { background: rgba(255,255,255,0.04); }

        .btn-primary {
            background: rgba(0, 212, 255, 0.12);
            border-color: var(--neon-cyan);
            color: var(--neon-cyan);
        }
        .btn-primary:hover { box-shadow: 0 0 12px rgba(0,212,255,0.3); }

        .btn-success {
            background: rgba(0, 255, 136, 0.12);
            border-color: var(--neon-green);
            color: var(--neon-green);
        }
        .btn-success:hover { box-shadow: 0 0 12px rgba(0,255,136,0.3); }

        .btn-warning {
            background: rgba(255, 179, 0, 0.10);
            border-color: var(--neon-amber);
            color: var(--neon-amber);
        }
        .btn-warning:hover { box-shadow: 0 0 12px rgba(255,179,0,0.3); }

        .btn-danger {
            background: var(--neon-red-bg);
            border-color: var(--neon-red);
            color: var(--neon-red);
        }
        .btn-danger:hover { box-shadow: 0 0 12px rgba(255,61,90,0.3); }

        .btn-dim {
            background: rgba(30,45,74,0.6);
            border-color: var(--border-glow);
            color: var(--text-muted);
        }
        .btn-dim:hover { color: var(--text-primary); border-color: var(--border-glow); }

        /* Save feedback */
        #save-status {
            font-size: 0.65rem;
            text-align: center;
            color: var(--text-muted);
            min-height: 1rem;
            transition: color var(--transition-med);
        }
        #save-status.ok   { color: var(--neon-green); }
        #save-status.err  { color: var(--neon-red); }

        /* Divider */
        .divider {
            border: none;
            border-top: 1px solid var(--border-dim);
            margin: 12px 0;
        }

        /* =====================================================================
           OTA UPDATE PANEL
           ===================================================================== */
        #panel-ota {
            margin: 0 12px 16px;
            border: 1px solid var(--border-glow);
            border-radius: 10px;
            background: var(--bg-panel-gls);
            backdrop-filter: blur(6px);
            box-shadow: 0 4px 24px rgba(0,0,0,0.5), inset 0 1px 0 rgba(255,255,255,0.03);
            padding: 16px;
            position: relative;
            overflow: hidden;
        }
        #panel-ota::before {
            content: '';
            position: absolute;
            inset: 0;
            background: repeating-linear-gradient(
                90deg,
                transparent,
                transparent 18px,
                rgba(26,58,110,0.06) 18px,
                rgba(26,58,110,0.06) 19px
            );
            pointer-events: none;
        }

        .ota-header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            flex-wrap: wrap;
            gap: 8px;
            margin-bottom: 14px;
            padding-bottom: 8px;
            border-bottom: 1px solid var(--border-dim);
        }
        .ota-title {
            font-family: var(--text-hud);
            font-size: 0.6rem;
            letter-spacing: 0.2em;
            color: var(--text-muted);
            text-transform: uppercase;
        }
        .ota-version-badge {
            font-family: var(--text-mono);
            font-size: 0.65rem;
            padding: 3px 10px;
            border: 1px solid var(--border-dim);
            border-radius: 20px;
            color: var(--text-muted);
        }

        .ota-body {
            display: flex;
            gap: 14px;
            align-items: flex-start;
            flex-wrap: wrap;
        }

        #ota-dropzone {
            flex: 1;
            min-width: 200px;
            border: 1.5px dashed var(--border-glow);
            border-radius: 8px;
            padding: 20px 16px;
            text-align: center;
            cursor: pointer;
            transition: border-color var(--transition-med), background var(--transition-med);
            position: relative;
            background: transparent;
        }
        #ota-dropzone:hover,
        #ota-dropzone.drag-over {
            border-color: var(--neon-amber);
            background: rgba(255,179,0,0.06);
        }
        #ota-dropzone.file-ready {
            border-color: var(--neon-green);
            background: rgba(0,255,136,0.06);
        }
        #ota-dropzone input[type="file"] {
            position: absolute;
            inset: 0;
            width: 100%;
            height: 100%;
            opacity: 0;
            cursor: pointer;
        }
        .ota-drop-icon {
            font-size: 1.8rem;
            display: block;
            margin-bottom: 8px;
        }
        #ota-filename {
            font-family: var(--text-mono);
            font-size: 0.68rem;
            color: var(--text-muted);
            word-break: break-all;
        }
        #ota-dropzone.file-ready #ota-filename { color: var(--neon-green); }

        .ota-controls {
            display: flex;
            flex-direction: column;
            gap: 8px;
            min-width: 160px;
        }

        /* Progress bar */
        #ota-progress-wrap {
            display: none;
            flex-direction: column;
            gap: 6px;
            width: 100%;
            margin-top: 12px;
        }
        #ota-progress-label {
            display: flex;
            justify-content: space-between;
            font-size: 0.65rem;
            color: var(--text-muted);
        }
        #ota-progress-bar-bg {
            height: 6px;
            background: var(--border-dim);
            border-radius: 6px;
            overflow: hidden;
        }
        #ota-progress-bar {
            height: 100%;
            width: 0%;
            border-radius: 6px;
            background: linear-gradient(90deg, var(--neon-amber), var(--neon-green));
            box-shadow: 0 0 8px rgba(255,179,0,0.5);
            transition: width 0.2s linear;
        }
        #ota-progress-bar.done {
            background: var(--neon-green);
            box-shadow: 0 0 10px var(--neon-green-glow);
        }
        #ota-progress-bar.error {
            background: var(--neon-red);
            box-shadow: 0 0 8px rgba(255,61,90,0.5);
        }

        #ota-status-msg {
            font-size: 0.68rem;
            color: var(--text-muted);
            text-align: center;
            min-height: 1.1rem;
            transition: color var(--transition-med);
        }
        #ota-status-msg.uploading { color: var(--neon-amber); }
        #ota-status-msg.success   { color: var(--neon-green);  font-weight: 700; }
        #ota-status-msg.error     { color: var(--neon-red);    font-weight: 700; }

        /* =====================================================================
           SCROLLBAR
           ===================================================================== */
        ::-webkit-scrollbar { width: 5px; }
        ::-webkit-scrollbar-track { background: var(--bg-primary); }
        ::-webkit-scrollbar-thumb { background: var(--border-glow); border-radius: 4px; }

    </style>
</head>
<body>

<!-- =========================================================================
     TOP BAR
     ========================================================================= -->
<div id="top-bar">
    <h1>&#9651; THERMAL COUNTER HUD</h1>
    <div class="top-badges">
        <div class="badge" id="ws-badge">
            <div class="dot" id="ws-dot"></div>
            <span id="ws-status">DESCONECTADO</span>
        </div>
        <div class="badge" id="sensor-badge">
            <div class="dot warning" id="sensor-dot"></div>
            <span id="sensor-label">SENSOR ··</span>
        </div>
        <div id="fps-display">-- FPS</div>
    </div>
</div>

<!-- =========================================================================
     MAIN GRID
     ========================================================================= -->
<div id="app-grid">

    <!-- -----------------------------------------------------------------------
         LEFT — METRICS
         ----------------------------------------------------------------------- -->
    <div id="panel-metrics">

        <!-- ENTRADAS -->
        <div class="counter-card in">
            <div class="counter-label">&#8645; ENTRADAS</div>
            <span class="counter-value" id="count-in">0</span>
            <div class="counter-sublabel">personas detectadas</div>
        </div>

        <!-- SALIDAS -->
        <div class="counter-card out">
            <div class="counter-label">&#8645; SALIDAS</div>
            <span class="counter-value" id="count-out">0</span>
            <div class="counter-sublabel">personas detectadas</div>
        </div>

        <!-- SENSOR ALERTA -->
        <div id="sensor-alert">&#9888; SENSOR I2C FAIL</div>

        <!-- TODO: Time-series log module — placeholder for future chart panel -->
    </div>

    <!-- -----------------------------------------------------------------------
         CENTER — TACTICAL VIEWER
         ----------------------------------------------------------------------- -->
    <div id="panel-viewer" class="panel">
        <div class="panel-title">VISOR TÁCTICO  — 32×24 @ 16 Hz</div>

        <!-- Canvas frame -->
        <div class="canvas-frame" id="canvas-frame">
            <canvas id="display-canvas" width="640" height="480"></canvas>
            <div class="scanlines"></div>
            <div class="corner-bl"></div>
            <div class="corner-br"></div>
            <div id="no-signal">
                <span>&#9612;</span>
                <p>SIN SEÑAL — CONECTANDO...</p>
            </div>
        </div>

        <!-- Info bar under canvas -->
        <div class="canvas-info" style="margin-top:6px;">
            <span id="info-tracks">TRACKS: 0</span>
            <span id="info-temp-range">TEMP: --°C / --°C</span>
            <span id="info-frame">FRAME: --</span>
        </div>

        <!-- TOGGLE ROW -->
        <div class="toggle-row" style="margin-top:10px;">
            <label class="toggle-item active" id="tgl-heatmap">
                <input type="checkbox" id="cb-heatmap" checked>
                <div class="toggle-dot"></div>
                HEATMAP
            </label>
            <label class="toggle-item active" id="tgl-grid">
                <input type="checkbox" id="cb-grid" checked>
                <div class="toggle-dot"></div>
                GRID
            </label>
            <label class="toggle-item active" id="tgl-tracks">
                <input type="checkbox" id="cb-tracks" checked>
                <div class="toggle-dot"></div>
                TRACKS
            </label>
            <label class="toggle-item active" id="tgl-lines">
                <input type="checkbox" id="cb-lines" checked>
                <div class="toggle-dot"></div>
                LÍNEAS
            </label>
        </div>
    </div>

    <!-- -----------------------------------------------------------------------
         RIGHT — CALIBRATION PANEL
         ----------------------------------------------------------------------- -->
    <div id="panel-calib" class="panel">
        <div class="panel-title">CALIBRACIÓN</div>

        <!-- Detection params -->
        <div class="calib-section">
            <div class="calib-section-title">Detección</div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label">Temp. Biológica Mín</span>
                    <span class="slider-value" id="disp-temp_bio">25.0</span>
                </div>
                <input type="range" id="sl-temp_bio" min="20.0" max="35.0" step="0.5" value="25.0">
            </div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label">Delta Fondo (°C)</span>
                    <span class="slider-value" id="disp-delta_t">1.5</span>
                </div>
                <input type="range" id="sl-delta_t" min="0.5" max="5.0" step="0.1" value="1.5">
            </div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label">Adaptación EMA</span>
                    <span class="slider-value" id="disp-alpha_ema">0.05</span>
                </div>
                <input type="range" id="sl-alpha_ema" min="0.01" max="0.50" step="0.01" value="0.05">
            </div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label" style="color:var(--neon-amber)">Radio NMS Centro</span>
                    <span class="slider-value" id="disp-nms_center">16</span>
                </div>
                <input type="range" id="sl-nms_center" min="1" max="25" step="1" value="16">
            </div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label" style="color:var(--neon-amber)">Radio NMS Borde</span>
                    <span class="slider-value" id="disp-nms_edge">4</span>
                </div>
                <input type="range" id="sl-nms_edge" min="1" max="16" step="1" value="4">
            </div>
        </div>

        <!-- Vista -->
        <div class="calib-section">
            <div class="calib-section-title">Modo de Vista</div>
            <div class="slider-group">
                <select id="sel-view_mode" style="width:100%; padding:5px; font-family:var(--text-mono); background:var(--bg-panel); color:var(--text-primary); border:1px solid var(--border-dim); border-radius:4px; outline:none; cursor:pointer;">
                    <option value="0">Cámara Térmica Normal</option>
                    <option value="1">Modo Radar (Sustracción)</option>
                    <option value="2">Sensor en Bruto (Píxeles exactos)</option>
                </select>
            </div>
        </div>

        <!-- Counting zones -->
        <div class="calib-section">
            <div class="calib-section-title">Zonas de Conteo</div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label" style="color:var(--neon-green)">Línea Entrada (Y)</span>
                    <span class="slider-value" id="disp-line_entry">11</span>
                </div>
                <input type="range" id="sl-line_entry" min="1" max="22" step="1" value="11">
            </div>

            <div class="slider-group">
                <div class="slider-header">
                    <span class="slider-label" style="color:var(--neon-cyan)">Línea Salida (Y)</span>
                    <span class="slider-value" id="disp-line_exit">13</span>
                </div>
                <input type="range" id="sl-line_exit" min="1" max="22" step="1" value="13">
            </div>
        </div>

        <!-- Action buttons -->
        <div class="btn-row">
            <button class="btn btn-primary" id="btn-apply" onclick="applyConfig()">
                &#10003; APLICAR AJUSTES
            </button>
            <button class="btn btn-success" id="btn-save" onclick="saveConfig()">
                &#128190; GUARDAR EN FLASH
            </button>
            <div id="save-status"></div>

            <hr class="divider">

            <button class="btn btn-warning" id="btn-reset" onclick="sendCmd('RESET_COUNTS')">
                &#8635; RESETEAR CONTADORES
            </button>
            <button class="btn btn-dim" id="btn-retry" onclick="sendCmd('RETRY_SENSOR')">
                &#128267; REINTENTAR SENSOR
            </button>
        </div>
    </div>

</div> <!-- #app-grid -->

<!-- =========================================================================
     OTA UPDATE PANEL
     ========================================================================= -->
<div id="panel-ota">
    <div class="ota-header">
        <span class="ota-title">&#128257; ACTUALIZACIÓN OTA — Firmware Inalámbrico</span>
        <span class="ota-version-badge" id="ota-ver-badge">v1.0 · ESP32-S3</span>
    </div>

    <div class="ota-body">
        <!-- Drop zone -->
        <div id="ota-dropzone" onclick="document.getElementById('ota-file-input').click()">
            <input type="file" id="ota-file-input" accept=".bin" style="display:none">
            <span class="ota-drop-icon">&#9096;</span>
            <div id="ota-filename">Selecciona o arrastra un archivo <strong>.bin</strong></div>
        </div>

        <!-- Control buttons -->
        <div class="ota-controls">
            <button class="btn btn-warning" id="btn-flash" onclick="uploadFirmware()" disabled>
                &#9889; FLASH FIRMWARE
            </button>
            <div id="ota-status-msg">Sin archivo seleccionado</div>
        </div>
    </div>

    <!-- Progress bar (shown during upload) -->
    <div id="ota-progress-wrap">
        <div id="ota-progress-label">
            <span id="ota-progress-text">Enviando...</span>
            <span id="ota-progress-pct">0%</span>
        </div>
        <div id="ota-progress-bar-bg">
            <div id="ota-progress-bar"></div>
        </div>
    </div>
</div>


<!-- =========================================================================
     Invisible overlap — keeps original JS script comment -->
<!-- =========================================================================

     JAVASCRIPT
     ========================================================================= -->
<script>
"use strict";

// ===========================================================================
//  CONSTANTS & STATE
// ===========================================================================
const CANVAS_W     = 640;
const CANVAS_H     = 480;
const SENSOR_COLS  = 32;
const SENSOR_ROWS  = 24;
const SCALE_X      = CANVAS_W / SENSOR_COLS;  // 20
const SCALE_Y      = CANVAS_H / SENSOR_ROWS;  // 20
const HEADER_BYTE  = 0x11;

const state = {
    ws: null,
    frameCount:     0,
    lastFpsTime:    performance.now(),
    fps:            0,
    pendingFrame:   null,   // Latest decoded frame ready to render
    rafId:          null,
    sensorOk:       false,
    receivedFirst:  false,
    // Counting lines (rendered values, updated from slider or config msg)
    lineEntry: 11,
    lineExit:  13,
    // Visibility toggles
    showHeatmap: true,
    showGrid:    true,
    showTracks:  true,
    showLines:   true,
    // Slider values (local mirrors, not yet applied)
    sliders: {
        temp_bio:   25.0,
        delta_t:    1.5,
        alpha_ema:  0.05,
        line_entry: 11,
        line_exit:  13,
        nms_center: 16,
        nms_edge:   4
    }
};

// ===========================================================================
//  DOM REFERENCES
// ===========================================================================
const displayCanvas = document.getElementById('display-canvas');
const ctx           = displayCanvas.getContext('2d');
const srcCanvas     = new OffscreenCanvas(SENSOR_COLS, SENSOR_ROWS);
const srcCtx        = srcCanvas.getContext('2d');

const elCountIn     = document.getElementById('count-in');
const elCountOut    = document.getElementById('count-out');
const elWsDot       = document.getElementById('ws-dot');
const elWsStatus    = document.getElementById('ws-status');
const elSensorDot   = document.getElementById('sensor-dot');
const elSensorLabel = document.getElementById('sensor-label');
const elSensorAlert = document.getElementById('sensor-alert');
const noSignal      = document.getElementById('no-signal');
const fpsDisplay    = document.getElementById('fps-display');
const elInfoTracks  = document.getElementById('info-tracks');
const elInfoRange   = document.getElementById('info-temp-range');
const elInfoFrame   = document.getElementById('info-frame');
const elSaveStatus  = document.getElementById('save-status');

// ===========================================================================
//  INFERNO COLORMAP  (sampled 256-entry LUT)
// ===========================================================================
function buildInfernoLut() {
    const lut = new Uint8Array(256 * 3);
    const stops = [
        [0,   0, 0,  3],
        [40,  20, 11, 53],
        [80,  120, 28, 109],
        [120, 187, 55, 84],
        [160, 239, 99, 39],
        [200, 254, 171, 41],
        [240, 252, 237, 161],
        [255, 252, 255, 255],
    ];
    for (let i = 0; i < 255; i++) {
        let s = 0;
        while (s < stops.length - 2 && i > stops[s + 1][0]) s++;
        const [ti, ri, gi, bi] = stops[s];
        const [tj, rj, gj, bj] = stops[s + 1];
        const t = (i - ti) / (tj - ti);
        const idx = i * 3;
        lut[idx]     = Math.round(ri + t * (rj - ri));
        lut[idx + 1] = Math.round(gi + t * (gj - gi));
        lut[idx + 2] = Math.round(bi + t * (bj - bi));
    }
    const last = 255 * 3;
    lut[last] = 252; lut[last+1] = 255; lut[last+2] = 255;
    return lut;
}
const INFERNO_LUT = buildInfernoLut();

// ===========================================================================
//  BINARY FRAME DECODER
// ===========================================================================
function processFrame(buffer) {
    const dv     = new DataView(buffer);
    const byteLen = dv.byteLength;

    if (byteLen < 11 || dv.getUint8(0) !== HEADER_BYTE) return;

    let ofs = 1;
    const sensorOk    = dv.getUint8(ofs++) === 1;
    const ambientTemp = dv.getFloat32(ofs, true); ofs += 4;
    const countIn     = dv.getUint16(ofs, true); ofs += 2;
    const countOut    = dv.getUint16(ofs, true); ofs += 2;
    const numTracks   = dv.getUint8(ofs++);

    const tracks = [];
    for (let i = 0; i < numTracks; i++) {
        if (ofs + 9 > byteLen) break;
        const id = dv.getUint8(ofs++);
        const x  = dv.getInt16(ofs, true) / 100.0; ofs += 2;
        const y  = dv.getInt16(ofs, true) / 100.0; ofs += 2;
        const vx = dv.getInt16(ofs, true) / 100.0; ofs += 2;
        const vy = dv.getInt16(ofs, true) / 100.0; ofs += 2;
        tracks.push({ id, x, y, vx, vy });
    }

    // --- Pixel block: 768 × int16_t (temp × 100) ---
    const MIN_PIXELS = 768;
    const pixelBytes = MIN_PIXELS * 2;
    if (ofs + pixelBytes > byteLen) return;

    let minTemp =  9999.0;
    let maxTemp = -9999.0;
    const temps = new Float32Array(MIN_PIXELS);

    for (let i = 0; i < MIN_PIXELS; i++) {
        const t = dv.getInt16(ofs, true) / 100.0; ofs += 2;
        temps[i] = t;
        if (t < minTemp) minTemp = t;
        if (t > maxTemp) maxTemp = t;
    }

    // Ensure minimum color range to avoid amplifying noise
    const MIN_RANGE = 5.0;
    let dispMin = minTemp;
    let dispMax = maxTemp;
    
    // Auto-detect radar mode by temperature range (usually around 0°C) or element state
    const elMode = document.getElementById('sel-view_mode');
    const isRadarFallback = (maxTemp < 15.0 && minTemp < 5.0 && minTemp > -5.0);
    const isRadar = (elMode && elMode.value === "1") || isRadarFallback;

    if (isRadar) {
        // En modo radar (sustracción), el fondo oscila en torno a 0.0 debido al ruido.
        // Forzamos dispMin positivo para clippear el ruido a color negro.
        dispMin = 0.8;
        if (dispMax < dispMin + 2.0) dispMax = dispMin + 2.0;
    } else {
        if ((dispMax - dispMin) < MIN_RANGE) {
            const center = (dispMin + dispMax) * 0.5;
            dispMin = center - MIN_RANGE * 0.5;
            dispMax = center + MIN_RANGE * 0.5;
        }
    }

    const range = dispMax - dispMin || 1.0;

    // Build RGBA ImageData for 32×24 source canvas
    const imgData = srcCtx.createImageData(SENSOR_COLS, SENSOR_ROWS);
    const d = imgData.data;
    for (let i = 0; i < MIN_PIXELS; i++) {
        const norm   = Math.max(0, Math.min(1, (temps[i] - dispMin) / range));
        const lutIdx = Math.floor(norm * 255) * 3;
        const px     = i * 4;
        d[px]     = INFERNO_LUT[lutIdx];
        d[px + 1] = INFERNO_LUT[lutIdx + 1];
        d[px + 2] = INFERNO_LUT[lutIdx + 2];
        d[px + 3] = 255;
    }
    srcCtx.putImageData(imgData, 0, 0);

    // Store decoded packet for next rAF render tick
    state.pendingFrame = { sensorOk, countIn, countOut, tracks, ambientTemp,
                           minTemp, maxTemp, frameId: state.frameCount };
    state.frameCount++;

    // Update FPS
    const now = performance.now();
    const dt  = now - state.lastFpsTime;
    if (dt >= 500) {
        state.fps = Math.round(state.frameCount / (dt / 1000));
        state.lastFpsTime = now;
        state.frameCount  = 0;
        fpsDisplay.textContent = state.fps + ' FPS';
    }

    // Show canvas on first frame
    if (!state.receivedFirst) {
        state.receivedFirst = true;
        noSignal.style.display = 'none';
    }
}

// ===========================================================================
//  RENDER LOOP  (requestAnimationFrame, decoupled from WS)
// ===========================================================================
function renderLoop() {
    state.rafId = requestAnimationFrame(renderLoop);

    const frame = state.pendingFrame;
    if (!frame) return;
    state.pendingFrame = null; // consume

    // ---- Update counters ----
    updateCounter(elCountIn,  frame.countIn);
    updateCounter(elCountOut, frame.countOut);

    // ---- Update sensor status ----
    if (state.sensorOk !== frame.sensorOk) {
        state.sensorOk = frame.sensorOk;
        setSensorBadge(frame.sensorOk);
    }

    // ---- Update info bar ----
    elInfoTracks.textContent = 'TRACKS: ' + frame.tracks.length;
    elInfoRange.innerHTML  = 'Ta: <span style="color:var(--neon-amber)">' + frame.ambientTemp.toFixed(1) + '°C</span> | RANGO: ' + frame.minTemp.toFixed(1) + '°C / ' + frame.maxTemp.toFixed(1) + '°C';
    elInfoFrame.textContent  = 'FRAME: ' + frame.frameId;

    // ---- Render Heatmap (GPU bilinear interpolation vs Raw Pixels) ----
    const elMode = document.getElementById('sel-view_mode');
    const isRaw = (elMode && elMode.value === "2");
    
    ctx.imageSmoothingEnabled = !isRaw;
    ctx.imageSmoothingQuality = isRaw ? 'low' : 'high';

    if (state.showHeatmap) {
        ctx.drawImage(srcCanvas, 0, 0, CANVAS_W, CANVAS_H);
    } else {
        ctx.fillStyle = '#050c1a';
        ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);
    }

    // ---- Render Grid ----
    if (state.showGrid) {
        renderGrid();
    }

    // ---- Render Counting Lines ----
    if (state.showLines) {
        renderCountingLines(state.lineEntry, state.lineExit);
    }

    // ---- Render Tracks ----
    if (state.showTracks) {
        renderTracks(frame.tracks);
    }
}

// ---------------------------------------------------------------------------
function renderGrid() {
    ctx.save();
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.08)';
    ctx.lineWidth   = 0.5;
    for (let col = 1; col < SENSOR_COLS; col++) {
        ctx.beginPath();
        ctx.moveTo(col * SCALE_X, 0);
        ctx.lineTo(col * SCALE_X, CANVAS_H);
        ctx.stroke();
    }
    for (let row = 1; row < SENSOR_ROWS; row++) {
        ctx.beginPath();
        ctx.moveTo(0,       row * SCALE_Y);
        ctx.lineTo(CANVAS_W, row * SCALE_Y);
        ctx.stroke();
    }
    ctx.restore();
}

// ---------------------------------------------------------------------------
function renderCountingLines(entryY, exitY) {
    ctx.save();
    ctx.setLineDash([10, 6]);
    ctx.lineWidth = 2;

    // Entry line (green)
    const pyEntry = entryY * SCALE_Y;
    ctx.strokeStyle = 'rgba(0, 255, 136, 0.80)';
    ctx.shadowColor = '#00ff88';
    ctx.shadowBlur  = 6;
    ctx.beginPath();
    ctx.moveTo(0,       pyEntry);
    ctx.lineTo(CANVAS_W, pyEntry);
    ctx.stroke();
    ctx.fillStyle = 'rgba(0, 255, 136, 0.9)';
    ctx.font = '600 11px JetBrains Mono, monospace';
    ctx.fillText('ENTRADA', 6, pyEntry - 4);

    // Exit line (cyan)
    const pyExit = exitY * SCALE_Y;
    ctx.strokeStyle = 'rgba(0, 212, 255, 0.80)';
    ctx.shadowColor = '#00d4ff';
    ctx.beginPath();
    ctx.moveTo(0,       pyExit);
    ctx.lineTo(CANVAS_W, pyExit);
    ctx.stroke();
    ctx.fillStyle = 'rgba(0, 212, 255, 0.9)';
    ctx.fillText('SALIDA', 6, pyExit - 4);

    ctx.restore();
}

// ---------------------------------------------------------------------------
function renderTracks(tracks) {
    if (!tracks.length) return;
    ctx.save();

    const BOX_HALF = Math.round(1.5 * SCALE_X); // 3×3 thermal pixels wide

    tracks.forEach(t => {
        const px = t.x * SCALE_X;
        const py = t.y * SCALE_Y;
        const bx = px - BOX_HALF;
        const by = py - BOX_HALF;
        const bw = BOX_HALF * 2;
        const bh = BOX_HALF * 2;

        // Glow rectangle
        ctx.shadowColor = '#00ff88';
        ctx.shadowBlur  = 10;
        ctx.strokeStyle = '#00ff88';
        ctx.lineWidth   = 1.5;
        ctx.strokeRect(bx, by, bw, bh);

        // Corner ticks
        ctx.strokeStyle = '#ffffff';
        ctx.lineWidth   = 1;
        ctx.shadowBlur  = 4;
        const TICK = 5;
        // TL
        ctx.beginPath(); ctx.moveTo(bx, by + TICK);          ctx.lineTo(bx, by);          ctx.lineTo(bx + TICK, by);          ctx.stroke();
        // TR
        ctx.beginPath(); ctx.moveTo(bx+bw-TICK, by);          ctx.lineTo(bx+bw, by);       ctx.lineTo(bx+bw, by+TICK);         ctx.stroke();
        // BL
        ctx.beginPath(); ctx.moveTo(bx, by+bh-TICK);          ctx.lineTo(bx, by+bh);       ctx.lineTo(bx+TICK, by+bh);         ctx.stroke();
        // BR
        ctx.beginPath(); ctx.moveTo(bx+bw-TICK, by+bh);       ctx.lineTo(bx+bw, by+bh);   ctx.lineTo(bx+bw, by+bh-TICK);     ctx.stroke();

        // Crosshair dot
        ctx.shadowBlur  = 6;
        ctx.fillStyle   = '#00ff88';
        ctx.beginPath();
        ctx.arc(px, py, 3, 0, Math.PI * 2);
        ctx.fill();

        // Velocity vector arrow
        const vlen = Math.sqrt(t.vx*t.vx + t.vy*t.vy);
        if (vlen > 0.05) {
            const vxpx = t.vx * SCALE_X * 5; // exagerar vector para visualización
            const vypy = t.vy * SCALE_Y * 5;
            
            ctx.strokeStyle = '#ffb300';
            ctx.lineWidth = 1.5;
            ctx.beginPath();
            ctx.moveTo(px, py);
            ctx.lineTo(px + vxpx, py + vypy);
            ctx.stroke();

            // Arrow head
            const angle = Math.atan2(vypy, vxpx);
            ctx.beginPath();
            ctx.moveTo(px + vxpx, py + vypy);
            ctx.lineTo(px + vxpx - 5 * Math.cos(angle - Math.PI/6), py + vypy - 5 * Math.sin(angle - Math.PI/6));
            ctx.lineTo(px + vxpx - 5 * Math.cos(angle + Math.PI/6), py + vypy - 5 * Math.sin(angle + Math.PI/6));
            ctx.fillStyle = '#ffb300';
            ctx.fill();
        }

        // ID label
        ctx.shadowBlur = 0;
        ctx.fillStyle  = 'rgba(0,0,0,0.6)';
        ctx.fillRect(bx, by - 16, 46, 14);
        ctx.fillStyle  = '#00ff88';
        ctx.font       = 'bold 10px JetBrains Mono, monospace';
        ctx.fillText('ID:' + t.id + ' (' + t.x.toFixed(1) + ',' + t.y.toFixed(1) + ')', bx + 2, by - 5);
    });

    ctx.restore();
}

// ===========================================================================
//  COUNTER ANIMATION
// ===========================================================================
let prevCountIn  = -1;
let prevCountOut = -1;

function updateCounter(el, newVal) {
    if (el === elCountIn  && newVal === prevCountIn)  return;
    if (el === elCountOut && newVal === prevCountOut) return;
    el.textContent = newVal;
    el.classList.remove('bump');
    void el.offsetWidth; // reflow to restart animation
    el.classList.add('bump');
    if (el === elCountIn)  prevCountIn  = newVal;
    if (el === elCountOut) prevCountOut = newVal;
}

// ===========================================================================
//  WEBSOCKET
// ===========================================================================
function connectWs() {
    const url = `ws://${window.location.host}/ws`;
    state.ws  = new WebSocket(url);
    state.ws.binaryType = 'arraybuffer';

    state.ws.onopen = () => {
        elWsDot.className    = 'dot online';
        elWsStatus.textContent = 'CONECTADO';
        // Request current config from ESP32
        state.ws.send(JSON.stringify({ cmd: 'GET_CONFIG' }));
    };

    state.ws.onclose = () => {
        elWsDot.className    = 'dot';
        elWsStatus.textContent = 'RECONECTANDO...';
        state.receivedFirst  = false;
        noSignal.style.display  = '';
        // Exponential backoff: 2s
        setTimeout(connectWs, 2000);
    };

    state.ws.onerror = () => { /* onclose will fire anyway */ };

    state.ws.onmessage = (ev) => {
        if (ev.data instanceof ArrayBuffer) {
            processFrame(ev.data);
        } else {
            handleTextMessage(ev.data);
        }
    };
}

function handleTextMessage(text) {
    try {
        const msg = JSON.parse(text);
        if (msg.type === 'config') {
            applyIncomingConfig(msg);
        } else if (msg.type === 'config_saved') {
            setSaveStatus(msg.ok ? 'ok' : 'err',
                          msg.ok ? '&#10003; Guardado en flash' : '&#10007; Error al guardar');
        }
    } catch (e) { console.warn('Bad JSON from WS:', text); }
}

function applyIncomingConfig(cfg) {
    if (cfg.temp_bio   !== undefined) setSlider('temp_bio',   cfg.temp_bio);
    if (cfg.delta_t    !== undefined) setSlider('delta_t',    cfg.delta_t);
    if (cfg.alpha_ema  !== undefined) setSlider('alpha_ema',  cfg.alpha_ema);
    if (cfg.nms_center !== undefined) setSlider('nms_center', cfg.nms_center);
    if (cfg.nms_edge   !== undefined) setSlider('nms_edge',   cfg.nms_edge);
    if (cfg.line_entry !== undefined) { setSlider('line_entry', cfg.line_entry); state.lineEntry = cfg.line_entry; }
    if (cfg.line_exit  !== undefined) { setSlider('line_exit',  cfg.line_exit);  state.lineExit  = cfg.line_exit;  }
    
    if (cfg.view_mode !== undefined) {
        const el = document.getElementById('sel-view_mode');
        if (el) el.value = cfg.view_mode;
    }
}

function setSlider(param, val) {
    const el   = document.getElementById('sl-' + param);
    const disp = document.getElementById('disp-' + param);
    if (!el || !disp) return;
    el.value        = val;
    disp.textContent = Number(val).toFixed(param === 'alpha_ema' ? 2 : (param.startsWith('line') ? 0 : 1));
    state.sliders[param] = val;
}

function sendWs(obj) {
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.send(JSON.stringify(obj));
    }
}

function sendCmd(cmd) {
    sendWs({ cmd });
}

// ===========================================================================
//  CONFIG WORKFLOW
// ===========================================================================
// "Aplicar Ajustes" — send each changed slider to the pipeline via SET_PARAM
function applyConfig() {
    const PARAMS = ['temp_bio', 'delta_t', 'alpha_ema', 'line_entry', 'line_exit', 'nms_center', 'nms_edge'];
    PARAMS.forEach(param => {
        const el  = document.getElementById('sl-' + param);
        if (!el) return;
        const val = parseFloat(el.value);
        sendWs({ cmd: 'SET_PARAM', param, val });
        // Update live render state for counting lines immediately
        if (param === 'line_entry') state.lineEntry = val;
        if (param === 'line_exit')  state.lineExit  = val;
        state.sliders[param] = val;
    });
    
    // View mode selector
    const selViewMode = document.getElementById('sel-view_mode');
    if (selViewMode) {
        sendWs({ cmd: 'SET_PARAM', param: 'view_mode', val: parseInt(selViewMode.value) });
    }

    setSaveStatus('ok', '&#10003; Ajustes aplicados');
}

// "Guardar en Flash" — persist config to NVS on the ESP32
function saveConfig() {
    sendWs({ cmd: 'SAVE_CONFIG' });
    setSaveStatus(null, 'Guardando...');
}

function setSaveStatus(cls, html) {
    elSaveStatus.className = 'save-status' + (cls ? ' ' + cls : '');
    elSaveStatus.innerHTML = html;
    if (cls) {
        setTimeout(() => { elSaveStatus.innerHTML = ''; elSaveStatus.className = ''; }, 3000);
    }
}

// ===========================================================================
//  SENSOR BADGE
// ===========================================================================
function setSensorBadge(ok) {
    elSensorDot.className   = ok ? 'dot online' : 'dot warning';
    elSensorLabel.textContent = ok ? 'SENSOR OK' : 'SENSOR FAIL';
    elSensorAlert.style.display = ok ? 'none' : 'block';
}

// ===========================================================================
//  SLIDER UI EVENTS
//  oninput → update display label immediately (no send)
//  No auto-send on change — user clicks "Aplicar Ajustes"
// ===========================================================================
[
    { param: 'temp_bio',   decimals: 1 },
    { param: 'delta_t',    decimals: 1 },
    { param: 'alpha_ema',  decimals: 2 },
    { param: 'line_entry', decimals: 0 },
    { param: 'line_exit',  decimals: 0 },
    { param: 'nms_center', decimals: 0 },
    { param: 'nms_edge',   decimals: 0 }
].forEach(({ param, decimals }) => {
    const el   = document.getElementById('sl-' + param);
    const disp = document.getElementById('disp-' + param);
    if (!el || !disp) return;
    el.addEventListener('input', () => {
        disp.textContent = Number(el.value).toFixed(decimals);
    });
});

// ===========================================================================
//  TOGGLE EVENTS
// ===========================================================================
function setupToggle(cbId, tglId, stateKey) {
    const cb  = document.getElementById(cbId);
    const lbl = document.getElementById(tglId);
    if (!cb || !lbl) return;
    lbl.addEventListener('click', () => {
        cb.checked         = !cb.checked;
        state[stateKey]    = cb.checked;
        lbl.classList.toggle('active', cb.checked);
    });
}
setupToggle('cb-heatmap', 'tgl-heatmap', 'showHeatmap');
setupToggle('cb-grid',    'tgl-grid',    'showGrid');
setupToggle('cb-tracks',  'tgl-tracks',  'showTracks');
setupToggle('cb-lines',   'tgl-lines',   'showLines');

// ===========================================================================
//  OTA UPLOAD
// ===========================================================================
const otaDropzone    = document.getElementById('ota-dropzone');
const otaFileInput   = document.getElementById('ota-file-input');
const otaFilename    = document.getElementById('ota-filename');
const btnFlash       = document.getElementById('btn-flash');
const otaStatusMsg   = document.getElementById('ota-status-msg');
const otaProgressWrap= document.getElementById('ota-progress-wrap');
const otaProgressBar = document.getElementById('ota-progress-bar');
const otaPct         = document.getElementById('ota-progress-pct');
const otaProgressTxt = document.getElementById('ota-progress-text');

let selectedFirmware = null;

// --- Drag-and-drop listeners ---
otaDropzone.addEventListener('dragover', e => { e.preventDefault(); otaDropzone.classList.add('drag-over'); });
otaDropzone.addEventListener('dragleave', () => otaDropzone.classList.remove('drag-over'));
otaDropzone.addEventListener('drop', e => {
    e.preventDefault();
    otaDropzone.classList.remove('drag-over');
    const f = e.dataTransfer.files[0];
    if (f) setFirmwareFile(f);
});
otaFileInput.addEventListener('change', () => {
    if (otaFileInput.files[0]) setFirmwareFile(otaFileInput.files[0]);
});

function setFirmwareFile(file) {
    if (!file.name.endsWith('.bin')) {
        otaStatusMsg.textContent  = '⚠ Solo se aceptan archivos .bin';
        otaStatusMsg.className    = 'error';
        return;
    }
    selectedFirmware              = file;
    const sizeKb                  = (file.size / 1024).toFixed(1);
    otaFilename.textContent       = `${file.name}  (${sizeKb} KB)`;
    otaDropzone.classList.add('file-ready');
    btnFlash.disabled             = false;
    otaStatusMsg.textContent      = 'Listo para flashear';
    otaStatusMsg.className        = '';
}

function setOtaUiLocked(locked) {
    btnFlash.disabled   = locked;
    otaFileInput.disabled = locked;
    otaDropzone.style.pointerEvents = locked ? 'none' : '';
    // Lock main controls during OTA to prevent concurrent WS traffic
    [document.getElementById('btn-apply'),
     document.getElementById('btn-save'),
     document.getElementById('btn-reset'),
     document.getElementById('btn-retry')].forEach(b => { if (b) b.disabled = locked; });
}

function uploadFirmware() {
    if (!selectedFirmware) return;

    // --- UI: uploading state ---
    otaProgressWrap.style.display = 'flex';
    otaProgressBar.className      = '';
    otaProgressBar.style.width    = '0%';
    otaPct.textContent            = '0%';
    otaProgressTxt.textContent    = 'Enviando firmware...';
    otaStatusMsg.textContent      = 'Flasheando... no desconectar';
    otaStatusMsg.className        = 'uploading';
    setOtaUiLocked(true);

    // Close WebSocket to free HTTP server sockets for OTA
    if (state.ws && state.ws.readyState === WebSocket.OPEN) {
        state.ws.close();
    }

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.setRequestHeader('Content-Type', 'application/octet-stream');

    xhr.upload.onprogress = e => {
        if (e.lengthComputable) {
            const pct = Math.round((e.loaded / e.total) * 100);
            otaProgressBar.style.width = pct + '%';
            otaPct.textContent         = pct + '%';
        }
    };

    xhr.onload = () => {
        if (xhr.status === 200) {
            otaProgressBar.classList.add('done');
            otaProgressBar.style.width  = '100%';
            otaPct.textContent          = '100%';
            otaProgressTxt.textContent  = 'Escrito correctamente';
            otaStatusMsg.className      = 'success';
            let countdown = 5;
            const tick = () => {
                otaStatusMsg.textContent = `✔ Flash OK — Reiniciando en ${countdown}s...`;
                if (countdown-- > 0) { setTimeout(tick, 1000); }
                else {
                    // Attempt automatic reconnection
                    otaStatusMsg.textContent = 'Reconectando...';
                    setTimeout(() => location.reload(), 2000);
                }
            };
            tick();
        } else {
            otaProgressBar.classList.add('error');
            otaProgressTxt.textContent = 'Error del servidor';
            otaStatusMsg.textContent   = `✖ Error HTTP ${xhr.status}: ${xhr.responseText}`;
            otaStatusMsg.className     = 'error';
            setOtaUiLocked(false);
            setTimeout(connectWs, 1500);  // Restore WebSocket
        }
    };

    xhr.onerror = () => {
        otaProgressBar.classList.add('error');
        otaProgressTxt.textContent = 'Error de red';
        otaStatusMsg.textContent   = '✖ Conexión perdida durante la subida';
        otaStatusMsg.className     = 'error';
        setOtaUiLocked(false);
        setTimeout(connectWs, 2000);
    };

    xhr.send(selectedFirmware);
}

// ===========================================================================
//  BOOT
// ===========================================================================
ctx.fillStyle = '#050c1a';
ctx.fillRect(0, 0, CANVAS_W, CANVAS_H);

renderLoop();  // Start rAF loop
connectWs();   // Open WebSocket

// TODO: Daily reset scheduler — future module for time-based counter reset
// TODO: Time-series log module — add a chart panel below the metrics
</script>

</body>
</html>
)rawliteral";
