/**
 * app.js — ESP32 Thermal Vigilant v2.0
 *
 * Binary frame protocol v2 (magic 0x12):
 *   [0]      magic          0x12
 *   [1]      sensor_ok      uint8
 *   [2-5]    ambient_temp   float32 LE
 *   [6-7]    count_in       uint16 LE
 *   [8-9]    count_out      uint16 LE
 *   [10]     num_tracks     uint8
 *   [11-12]  session_id     uint16 LE     (W3)
 *   [13]     time_quality   uint8         (W3)  0=none 1=browser 2=rtc
 *   [14+]    per track 11 bytes:
 *              id(1)+x_100(2)+y_100(2)+vx_100(2)+vy_100(2)+peak_temp_100(2)
 *   [14+n*11] pixels: TOTAL_PIXELS * int16 LE
 */
'use strict';

// =============================================================================
//  CONSTANTS
// =============================================================================
const SENSOR_W    = 32;
const SENSOR_H    = 24;
const TOTAL_PIX   = SENSOR_W * SENSOR_H;
const UPSCALE     = 2;
const UP_W        = SENSOR_W * UPSCALE;
const UP_H        = SENSOR_H * UPSCALE;
const SCALE_X     = 320 / SENSOR_W;
const SCALE_Y     = 240 / SENSOR_H;
const MAX_EVENTS  = 500;
const FRAME_MAGIC = 0x12;

// =============================================================================
//  GLOBAL STATE
// =============================================================================
const CONFIG = {
    temp_bio: 25.0, delta_t: 1.5, alpha_ema: 0.05,
    line_entry: 8,  line_exit: 16,
    sensor_height: 3.0, person_diameter: 0.7,
    view_mode: 0, dead_left: 0, dead_right: 31,
    use_segments: false
};

// W3: Session / clock
let sessionId   = 0;
let timeQuality = 0;   // 0=none 1=browser 2=rtc
let nvsBaseIn   = 0;
let nvsBaseOut  = 0;

// W5: Event log
let crossingEvents = [];
let prevCountIn    = -1;
let prevCountOut   = -1;

// Line editor
let userLines      = [];
let isEditingLines = false;
let currentLine    = null;
let showTrail      = false;

// Color lookup table
let LUT = new Uint8Array(256 * 3);

// WebSocket
let ws             = null;
let reconnectTimer = null;

// Auto-gain
let autoMin = 20.0;
let autoMax = 35.0;

// Render decoupling
let latestFrameBuffer = null;
let isRendering       = false;

// Client tracks & trails
let clientTracks = {};
let trackTrails  = {};

// Clock UI timer
let clockTimer = null;

// App start time (for relative timestamps when no clock)
let appStartMs = Date.now();

// =============================================================================
//  CANVAS REFS
// =============================================================================
const canvas    = document.getElementById('thermalCanvas');
const ctx       = canvas.getContext('2d');
const uiCanvas  = document.getElementById('uiCanvas');
const uiCtx     = uiCanvas.getContext('2d');
const offCanvas = document.createElement('canvas');
offCanvas.width = UP_W; offCanvas.height = UP_H;
const offCtx    = offCanvas.getContext('2d', { willReadFrequently: true });
const imgData   = offCtx.createImageData(UP_W, UP_H);
const scaleCvs  = document.getElementById('scaleCanvas');
const scaleCtx  = scaleCvs.getContext('2d');

// =============================================================================
//  BOOT
// =============================================================================
window.onload = () => {
    logMsg('App v2.0 init');
    regenerateLUT();
    applyVisionClass('normal');
    clockTimer = setInterval(tickClock, 1000);
    tickClock();
    connectWebSocket();
};

// =============================================================================
//  DOM HELPERS
// =============================================================================
function setEl(id, text) {
    const el = document.getElementById(id);
    if (el) el.textContent = text;
}
function setDot(id, color) {
    const el = document.getElementById(id);
    if (el) el.className = `dot ${color}`;
}

// =============================================================================
//  W3: CLOCK DISPLAY
// =============================================================================
function tickClock() {
    const te = document.getElementById('lbl-clock-time');
    const de = document.getElementById('lbl-clock-date');
    if (!te || !de) return;
    if (timeQuality === 0) {
        te.textContent = '--:--:--';
        de.textContent = 'Sin referencia horaria';
        return;
    }
    const now = new Date();
    te.textContent = now.toLocaleTimeString('es', { hour12: false });
    de.textContent = now.toLocaleDateString('es', {
        day: '2-digit', month: 'long', year: 'numeric'
    });
}

function updateClockBadge() {
    const labels = ['Sin hora', 'Browser', 'RTC DS3231'];
    const dots   = ['gray',    'amber',   'green'];
    const lbl    = labels[timeQuality] ?? 'Sin hora';
    const dot    = dots[timeQuality]   ?? 'gray';
    setDot('dot-clock', dot);
    setEl('lbl-clock-quality', lbl);
    setEl('lbl-time-quality-full', lbl);
    const tqLabel = document.getElementById('stat-time-quality-label');
    if (tqLabel) tqLabel.textContent = lbl;
}

// =============================================================================
//  VIEW NAVIGATION
// =============================================================================
function switchMainView(viewId, btn) {
    document.querySelectorAll('.view-section').forEach(v => v.classList.remove('active'));
    document.getElementById(`view-${viewId}`).classList.add('active');
    document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
    if (viewId === 'stats') { drawMiniChart(); updateEventsTable(); }
}

// W2-3 FIX: receives event explicitly — no implicit global `event`
function switchSettings(panelId, ev) {
    document.querySelectorAll('.settings-panel').forEach(p => p.classList.remove('active'));
    document.getElementById(`set-${panelId}`).classList.add('active');
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    if (ev && ev.target) ev.target.classList.add('active');
}

// W2-3 FIX: receives event explicitly
function setVisionMode(mode, ev) {
    document.querySelectorAll('.vision-controls .btn-pill').forEach(b => b.classList.remove('active'));
    if (ev && ev.target) ev.target.classList.add('active');
    applyVisionClass(mode);
    const espMode = (mode === 'diff') ? 1 : 0;
    if (espMode !== CONFIG.view_mode) {
        CONFIG.view_mode = espMode;
        sendCmd({ cmd: 'SET_PARAM', param: 'view_mode', val: espMode });
    }
}

function applyVisionClass(mode) {
    if (mode === 'raw') {
        canvas.style.imageRendering = 'pixelated';
        ctx.imageSmoothingEnabled   = false;
        canvas.classList.remove('smooth-thermal');
    } else {
        canvas.style.imageRendering = 'auto';
        ctx.imageSmoothingEnabled   = true;
        canvas.classList.add('smooth-thermal');
    }
}

// =============================================================================
//  LUT — IRONBOW PALETTE
// =============================================================================
function interpolateColor(c1, c2, f) {
    return [
        Math.round(c1[0] + f * (c2[0] - c1[0])),
        Math.round(c1[1] + f * (c2[1] - c1[1])),
        Math.round(c1[2] + f * (c2[2] - c1[2]))
    ];
}

function regenerateLUT() {
    const c0=[0,0,0],c1=[35,0,80],c2=[140,0,130],c3=[230,75,0],c4=[255,200,20],c5=[255,255,255];
    for (let i = 0; i < 256; i++) {
        const f = i / 255.0;
        let rgb;
        if      (f < 0.20) rgb = interpolateColor(c0, c1, f / 0.20);
        else if (f < 0.50) rgb = interpolateColor(c1, c2, (f-0.20)/0.30);
        else if (f < 0.75) rgb = interpolateColor(c2, c3, (f-0.50)/0.25);
        else if (f < 0.90) rgb = interpolateColor(c3, c4, (f-0.75)/0.15);
        else               rgb = interpolateColor(c4, c5, (f-0.90)/0.10);
        LUT[i*3]=rgb[0]; LUT[i*3+1]=rgb[1]; LUT[i*3+2]=rgb[2];
    }
    // Redraw scale bar
    const sd = scaleCtx.createImageData(200, 10);
    for (let x = 0; x < 200; x++) {
        const li = Math.floor((x/199)*255)*3;
        for (let y = 0; y < 10; y++) {
            const px = (y*200+x)*4;
            sd.data[px]=LUT[li]; sd.data[px+1]=LUT[li+1];
            sd.data[px+2]=LUT[li+2]; sd.data[px+3]=255;
        }
    }
    scaleCtx.putImageData(sd, 0, 0);
}

// =============================================================================
//  WEBSOCKET
// =============================================================================
function connectWebSocket() {
    let wsUrl = `ws://${window.location.host}/ws`;
    if (window.location.protocol === 'file:') wsUrl = 'ws://192.168.4.1/ws';
    ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
        logMsg('WS conectado');
        setEl('lbl-ws', 'Conectado');
        setDot('dot-ws', 'green');
        if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; }
        // W3: Sync time immediately
        sendCmd({ cmd: 'SET_TIME', unix_ms: Date.now() });
        // GET_CONFIG after a tick so SET_TIME is processed first
        setTimeout(() => sendCmd({ cmd: 'GET_CONFIG' }), 200);
    };

    ws.onclose = () => {
        logMsg('WS cerrado — reconectando...', true);
        setEl('lbl-ws', 'Desconectado');
        setDot('dot-ws', 'red');
        setDot('dot-sensor', 'gray');
        reconnectTimer = setTimeout(connectWebSocket, 2000);
    };

    ws.onerror = () => logMsg('WS error', true);

    ws.onmessage = (e) => {
        if (typeof e.data === 'string') {
            try {
                const obj = JSON.parse(e.data);
                if (obj.type === 'config')       applyConfig(obj);
                else if (obj.type === 'status')  applyStatus(obj);
                else if (obj.type === 'config_saved')
                    logMsg(`Flash: ${obj.ok ? 'guardado OK' : 'ERROR'}`);
            } catch (_) { /* malformed JSON — ignore */ }
        } else if (e.data instanceof ArrayBuffer) {
            latestFrameBuffer = e.data;
            if (!isRendering) { isRendering = true; requestAnimationFrame(renderLoop); }
        }
    };
}

function sendCmd(obj) {
    if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(obj));
}

// =============================================================================
//  RENDER LOOP
// =============================================================================
function renderLoop() {
    isRendering = false;
    if (!latestFrameBuffer) return;
    const buf = latestFrameBuffer;
    latestFrameBuffer = null;
    processBinaryFrame(buf);
}

function processBinaryFrame(buffer) {
    try {
        const dv = new DataView(buffer);

        // Magic check — old 0x11 frames from mismatched firmware are silently dropped
        if (dv.getUint8(0) !== FRAME_MAGIC) {
            logMsg(`Frame magic 0x${dv.getUint8(0).toString(16)} — espera 0x${FRAME_MAGIC.toString(16)}`, true);
            return;
        }

        let ofs = 1;
        const sensorOk  = dv.getUint8(ofs++) === 1;
        const ambT      = dv.getFloat32(ofs, true); ofs += 4;
        const cntIn     = dv.getUint16(ofs, true);  ofs += 2;
        const cntOut    = dv.getUint16(ofs, true);  ofs += 2;
        const nTracks   = dv.getUint8(ofs++);
        const sessId    = dv.getUint16(ofs, true);  ofs += 2;  // W3
        const tqual     = dv.getUint8(ofs++);                  // W3

        // W3: Update session/clock if changed
        if (sessId !== sessionId || tqual !== timeQuality) {
            sessionId   = sessId;
            timeQuality = tqual;
            updateClockBadge();
            setEl('stat-session-id-display', sessId);
            setEl('lbl-session-id', sessId);
        }

        // ---- Parse tracks ----
        const unseenIds = new Set(Object.keys(clientTracks));
        const tracks = [];

        for (let i = 0; i < nTracks; i++) {
            const tid    = dv.getUint8(ofs++);
            const tx     = dv.getInt16(ofs, true) / 100.0; ofs += 2;
            const ty     = dv.getInt16(ofs, true) / 100.0; ofs += 2;
            const tvx    = dv.getInt16(ofs, true) / 100.0; ofs += 2;
            const tvy    = dv.getInt16(ofs, true) / 100.0; ofs += 2;
            const tpeak  = dv.getInt16(ofs, true) / 100.0; ofs += 2; // W4

            let t = clientTracks[tid];
            if (t) {
                t.x = tx; t.y = ty;
                t.vx = t.vx * 0.5 + tvx * 0.5;
                t.vy = t.vy * 0.5 + tvy * 0.5;
                t.peak_temp = tpeak;
            } else {
                t = { id: tid, x: tx, y: ty, vx: tvx, vy: tvy, peak_temp: tpeak };
                clientTracks[tid] = t;
            }
            // W2-11 FIX: use String(tid) consistently for Set membership
            unseenIds.delete(String(tid));

            t.zone_state = CONFIG.use_segments ? 2
                : (ty < CONFIG.line_entry ? 1 : (ty >= CONFIG.line_exit ? 3 : 2));
            tracks.push(t);
        }
        unseenIds.forEach(id => delete clientTracks[id]);

        // ---- W5: Detect crossings ----
        checkCrossings(cntIn, cntOut, ambT, nTracks);

        // ---- Update HUD ----
        setDot('dot-sensor', sensorOk ? 'green' : 'red');
        setEl('lbl-amb', `${ambT.toFixed(1)}°C`);
        updateCounterDisplay(cntIn, cntOut);
        
        if (sensorOk) {
            updateFPS();
        } else {
            setEl('lbl-fps', '-- Hz');
        }

        // ---- Pixel data starts at ofs ----
        renderThermal(dv, ofs);

        // ---- Overlays ----
        uiCtx.clearRect(0, 0, 320, 240);
        if (!CONFIG.use_segments) {
            renderZones(CONFIG.line_entry, CONFIG.line_exit,
                        CONFIG.dead_left,  CONFIG.dead_right);
        } else {
            renderDeadZones(CONFIG.dead_left, CONFIG.dead_right);
        }
        renderUserLines();
        renderTrails(tracks);
        renderTracks(tracks);

    } catch (e) {
        const now = performance.now();
        if (!window._lastErr || now - window._lastErr > 3000) {
            logMsg('Frame err: ' + e.message, true);
            window._lastErr = now;
        }
    }
}

// =============================================================================
//  THERMAL RENDER
// =============================================================================
function renderThermal(dv, pixOfs) {
    let fMin = 999.0, fMax = -999.0;
    for (let i = 0; i < TOTAL_PIX; i++) {
        const t = dv.getInt16(pixOfs + i*2, true) / 100.0;
        if (t < fMin) fMin = t;
        if (t > fMax) fMax = t;
    }
    autoMin = autoMin * 0.9 + fMin * 0.1;
    autoMax = autoMax * 0.9 + fMax * 0.1;

    if (CONFIG.view_mode === 1) {
        const ma = Math.max(Math.abs(autoMin), Math.abs(autoMax));
        autoMin = -ma; autoMax = ma;
    }
    if (autoMax - autoMin < 8.0) {
        const mid = (autoMax + autoMin) / 2.0;
        autoMin = mid - 4.0; autoMax = mid + 4.0;
    }
    const RANGE = autoMax - autoMin;
    const d = imgData.data;

    for (let y = 0; y < UP_H; y++) {
        const gy  = (y / (UP_H-1)) * (SENSOR_H-1);
        const gyi = Math.min(Math.floor(gy), SENSOR_H-2);
        const fy  = gy - gyi;
        for (let x = 0; x < UP_W; x++) {
            const gx   = (x / (UP_W-1)) * (SENSOR_W-1);
            const gxi  = Math.min(Math.floor(gx), SENSOR_W-2);
            const fx   = gx - gxi;
            const b    = pixOfs;
            const c00  = dv.getInt16(b+(gyi*SENSOR_W+gxi)*2,    true)/100.0;
            const c10  = dv.getInt16(b+(gyi*SENSOR_W+gxi+1)*2,  true)/100.0;
            const c01  = dv.getInt16(b+((gyi+1)*SENSOR_W+gxi)*2,true)/100.0;
            const c11  = dv.getInt16(b+((gyi+1)*SENSOR_W+gxi+1)*2,true)/100.0;
            let v = (c00*(1-fx)+c10*fx)*(1-fy)+(c01*(1-fx)+c11*fx)*fy;
            v = Math.max(autoMin, Math.min(autoMax, v));
            const li = Math.floor(((v-autoMin)/RANGE)*255)*3;
            const pi = (y*UP_W+x)*4;
            d[pi]=LUT[li]; d[pi+1]=LUT[li+1]; d[pi+2]=LUT[li+2]; d[pi+3]=255;
        }
    }
    offCtx.putImageData(imgData, 0, 0);
    ctx.drawImage(offCanvas, 0, 0, 320, 240);
    setEl('scale-min', `${autoMin.toFixed(1)}°C`);
    setEl('scale-max', `${autoMax.toFixed(1)}°C`);
}

// =============================================================================
//  W7: COUNTER DISPLAY
// =============================================================================
function updateCounterDisplay(cntIn, cntOut) {
    const tIn  = nvsBaseIn  + cntIn;
    const tOut = nvsBaseOut + cntOut;
    setEl('stat-in',          tIn);
    setEl('stat-out',         tOut);
    setEl('stat-net',         tIn - tOut);
    setEl('stat-session-in',  `Sesión: ${cntIn}`);
    setEl('stat-session-out', `Sesión: ${cntOut}`);
    setEl('overlay-in',  `In: ${tIn}`);
    setEl('overlay-out', `Out: ${tOut}`);
}

function updateFPS() {
    window._fpsCount = (window._fpsCount || 0) + 1;
    const now = performance.now();
    if (!window._fpsLast) window._fpsLast = now;
    if (now - window._fpsLast >= 1000) {
        setEl('lbl-fps', `${window._fpsCount} Hz`);
        window._fpsCount = 0;
        window._fpsLast = now;
    }
}

// =============================================================================
//  W5: CROSSING DETECTION
// =============================================================================
function checkCrossings(cntIn, cntOut, ambT, nTracks) {
    if (prevCountIn === -1) {
        prevCountIn = cntIn; prevCountOut = cntOut; return;
    }
    const dIn  = cntIn  - prevCountIn;
    const dOut = cntOut - prevCountOut;
    for (let i = 0; i < dIn;  i++) recordEvent('IN',  cntIn,  cntOut, ambT, nTracks);
    for (let i = 0; i < dOut; i++) recordEvent('OUT', cntIn,  cntOut, ambT, nTracks);
    if (dIn || dOut) { drawMiniChart(); updateEventsTable(); }
    prevCountIn  = cntIn;
    prevCountOut = cntOut;
}

function recordEvent(dir, cntIn, cntOut, ambT, nTracks) {
    crossingEvents.push({
        session_id:    sessionId,
        timestamp_ms:  Date.now(),
        time_quality:  timeQuality,
        direction:     dir,
        count_in:      cntIn,
        count_out:     cntOut,
        ambient_temp_c: ambT,
        active_tracks: nTracks
    });
    if (crossingEvents.length > MAX_EVENTS) crossingEvents.shift();
    setEl('chart-subtitle', `${crossingEvents.length} eventos en sesión`);
}

// =============================================================================
//  W7: MINI CHART
// =============================================================================
function drawMiniChart() {
    const cvs = document.getElementById('mini-chart');
    if (!cvs) return;
    const c2  = cvs.getContext('2d');
    const W   = cvs.offsetWidth || 320;
    const H   = 80;
    cvs.width = W; cvs.height = H;
    c2.clearRect(0, 0, W, H);

    const barsIn  = new Array(24).fill(0);
    const barsOut = new Array(24).fill(0);

    if (timeQuality > 0 && crossingEvents.length > 0) {
        crossingEvents.forEach(e => {
            const h = new Date(e.timestamp_ms).getHours();
            if (e.direction === 'IN')  barsIn[h]++;
            else                        barsOut[h]++;
        });
    } else {
        const n = crossingEvents.length;
        const chunk = Math.max(1, Math.ceil(n / 24));
        crossingEvents.forEach((e, i) => {
            const slot = Math.min(23, Math.floor(i / chunk));
            if (e.direction === 'IN')  barsIn[slot]++;
            else                        barsOut[slot]++;
        });
    }

    const maxVal = Math.max(1, ...barsIn.map((v,i) => v + barsOut[i]));
    const bw     = W / 24;
    const BOT    = 14;
    const avail  = H - BOT;

    for (let i = 0; i < 24; i++) {
        const x     = i * bw;
        const total = barsIn[i] + barsOut[i];
        const hTot  = (total  / maxVal) * avail;
        const hIn   = (barsIn[i] / maxVal) * avail;

        if (barsOut[i] > 0) {
            c2.fillStyle = 'rgba(16,185,129,0.80)';
            c2.fillRect(x+1, H-BOT-hTot, bw-2, hTot-hIn);
        }
        if (barsIn[i] > 0) {
            c2.fillStyle = 'rgba(59,130,246,0.85)';
            c2.fillRect(x+1, H-BOT-hIn, bw-2, hIn);
        }
        if (total === 0) {
            c2.fillStyle = 'rgba(255,255,255,0.04)';
            c2.fillRect(x+1, 0, bw-2, avail);
        }
        if (i % 6 === 0) {
            c2.fillStyle = '#6b7280';
            c2.font = '9px monospace';
            c2.fillText(timeQuality > 0 ? `${i}h` : `#${i+1}`, x+2, H-2);
        }
    }
}

// =============================================================================
//  W7: EVENTS TABLE
// =============================================================================
function updateEventsTable() {
    const tbody = document.getElementById('events-tbody');
    if (!tbody) return;
    const recent = crossingEvents.slice(-50).reverse();
    if (recent.length === 0) {
        tbody.innerHTML = '<tr><td colspan="6" class="no-events">Sin eventos en esta sesión</td></tr>';
        return;
    }
    let html = '';
    recent.forEach(e => {
        let tsStr;
        if (e.time_quality > 0 && e.timestamp_ms > 0) {
            tsStr = new Date(e.timestamp_ms).toLocaleTimeString('es', { hour12: false });
        } else {
            const elapsedS = Math.round((e.timestamp_ms - appStartMs) / 1000);
            tsStr = `+${elapsedS}s`;
        }
        const cls = e.direction === 'IN' ? 'dir-in' : 'dir-out';
        html += `<tr>
            <td>${tsStr}</td>
            <td class="${cls}">${e.direction}</td>
            <td>${nvsBaseIn  + e.count_in}</td>
            <td>${nvsBaseOut + e.count_out}</td>
            <td>${e.ambient_temp_c.toFixed(1)}°</td>
            <td>${e.active_tracks}</td>
        </tr>`;
    });
    tbody.innerHTML = html;
}

// =============================================================================
//  W5: CSV EXPORT
// =============================================================================
function generateCSV() {
    if (crossingEvents.length === 0) {
        alert('Sin eventos para exportar en esta sesión.');
        return;
    }
    const TQ = ['none','browser','rtc'];
    const rows = ['session_id,timestamp_iso,time_quality,direction,' +
                  'count_in_total,count_out_total,ambient_temp_c,active_tracks'];
    crossingEvents.forEach(e => {
        const tsISO = (e.time_quality > 0 && e.timestamp_ms > 0)
            ? new Date(e.timestamp_ms).toISOString() : '';
        rows.push([
            e.session_id,
            tsISO,
            TQ[e.time_quality] ?? 'none',
            e.direction,
            nvsBaseIn  + e.count_in,
            nvsBaseOut + e.count_out,
            e.ambient_temp_c.toFixed(3),
            e.active_tracks
        ].join(','));
    });
    const blob = new Blob([rows.join('\n')], { type: 'text/csv;charset=utf-8;' });
    const url  = URL.createObjectURL(blob);
    const a    = document.createElement('a');
    const ts   = timeQuality > 0
        ? new Date().toISOString().slice(0,10)
        : `session_${sessionId}`;
    a.href = url; a.download = `thermal_${ts}.csv`;
    document.body.appendChild(a); a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    logMsg(`CSV exportado: ${crossingEvents.length} eventos`);
}

// =============================================================================
//  CONFIG SYNC
// =============================================================================

// W2-1: Update badge AND input together
function updateBadge(key, value) {
    const el = document.getElementById(`val-${key}`);
    if (el) el.textContent = value;
}

function syncSlider(key, value) {
    const input = document.getElementById(`cfg-${key}`);
    const badge = document.getElementById(`val-${key}`);
    const str   = (typeof value === 'number' && !Number.isInteger(value))
        ? value.toFixed(key.includes('alpha') ? 2 : 1)
        : String(value);
    if (input) input.value = value;
    // W2-4 FIX: badge always synced when config arrives from server
    if (badge) badge.textContent = str;
}

function applyConfig(obj) {
    // W2-2 FIX: nullish coalescing so 0 is not silently replaced by local default
    syncSlider('temp_bio',       obj.temp_bio        ?? CONFIG.temp_bio);
    syncSlider('delta_t',        obj.delta_t         ?? CONFIG.delta_t);
    syncSlider('alpha_ema',      obj.alpha_ema       ?? CONFIG.alpha_ema);
    syncSlider('line_entry',     obj.line_entry       ?? CONFIG.line_entry);
    syncSlider('line_exit',      obj.line_exit        ?? CONFIG.line_exit);
    syncSlider('dead_left',      obj.dead_left        ?? CONFIG.dead_left);
    syncSlider('dead_right',     obj.dead_right       ?? CONFIG.dead_right);
    syncSlider('sensor_height',  obj.sensor_height   ?? CONFIG.sensor_height);
    syncSlider('person_diameter',obj.person_diameter ?? CONFIG.person_diameter);

    CONFIG.temp_bio        = obj.temp_bio        ?? CONFIG.temp_bio;
    CONFIG.delta_t         = obj.delta_t         ?? CONFIG.delta_t;
    CONFIG.alpha_ema       = obj.alpha_ema       ?? CONFIG.alpha_ema;
    CONFIG.line_entry      = obj.line_entry       ?? CONFIG.line_entry;
    CONFIG.line_exit       = obj.line_exit        ?? CONFIG.line_exit;
    CONFIG.dead_left       = obj.dead_left        ?? CONFIG.dead_left;
    CONFIG.dead_right      = obj.dead_right       ?? CONFIG.dead_right;
    CONFIG.sensor_height   = obj.sensor_height   ?? CONFIG.sensor_height;
    CONFIG.person_diameter = obj.person_diameter ?? CONFIG.person_diameter;
    CONFIG.view_mode       = obj.view_mode        ?? CONFIG.view_mode;
    CONFIG.use_segments    = obj.use_segments     ?? false;

    // W3/W6
    sessionId   = obj.session_id   ?? sessionId;
    timeQuality = obj.time_quality  ?? timeQuality;
    nvsBaseIn   = obj.nvs_base_in  ?? nvsBaseIn;
    nvsBaseOut  = obj.nvs_base_out ?? nvsBaseOut;
    appStartMs  = Date.now();

    setEl('lbl-session-id',        sessionId);
    setEl('stat-session-id-display', sessionId);
    setEl('lbl-nvs-counts', `In:${nvsBaseIn} Out:${nvsBaseOut}`);
    updateClockBadge();

    if (obj.lines && Array.isArray(obj.lines)) { userLines = obj.lines; updateLineList(); }

    // Vision mode button
    document.querySelectorAll('.vision-controls .btn-pill').forEach(b => b.classList.remove('active'));
    const modeKey = CONFIG.view_mode === 1 ? 'diff' : 'normal';
    const modeBtn = document.querySelector(`.vision-controls [data-mode="${modeKey}"]`);
    if (modeBtn) modeBtn.classList.add('active');
    applyVisionClass(modeKey);
}

function applyStatus(obj) {
    sessionId   = obj.session_id   ?? sessionId;
    timeQuality = obj.time_quality  ?? timeQuality;
    nvsBaseIn   = obj.nvs_base_in  ?? nvsBaseIn;
    nvsBaseOut  = obj.nvs_base_out ?? nvsBaseOut;
    updateClockBadge();
    setEl('lbl-session-id',        sessionId);
    setEl('lbl-nvs-counts', `In:${nvsBaseIn} Out:${nvsBaseOut}`);
    // W8 stub: RTC presence
    const rtcPresent = obj.rtc_present ?? false;
    setDot('dot-rtc', rtcPresent ? 'green' : 'gray');
    setEl('lbl-rtc-status', rtcPresent ? 'DS3231 conectado' : 'DS3231 no detectado');
}

// Input range → send to ESP on 'change' (not oninput)
document.addEventListener('DOMContentLoaded', () => {
    document.querySelectorAll('input[type="range"]').forEach(inp => {
        inp.addEventListener('change', (e) => {
            const key = e.target.id.replace('cfg-', '');
            if (key === 'color_low' || key === 'color_high') return; // local only
            sendCmd({ cmd: 'SET_PARAM', param: key, val: parseFloat(e.target.value) });
            logMsg(`SET ${key}=${e.target.value}`);
        });
    });
});

// =============================================================================
//  ACTIONS
// =============================================================================
function saveConfig()    { sendCmd({ cmd: 'SAVE_CONFIG' }); logMsg('Guardando config...'); }
function appCmd(cmdStr)  { sendCmd({ cmd: cmdStr }); logMsg(`CMD: ${cmdStr}`); }
function requestStatus() { sendCmd({ cmd: 'GET_STATUS' }); logMsg('GET_STATUS'); }

function rebootEsp() {
    if (!confirm('¿Reiniciar el ESP32?')) return;
    fetch('/reboot', { method: 'POST' })
        .then(() => logMsg('Reboot enviado'))
        .catch(e => logMsg('Reboot error: ' + e, true));
}

function startOTA() {
    const fileInput = document.getElementById('ota-file');
    if (!fileInput || !fileInput.files[0]) { alert('Selecciona un archivo .bin'); return; }
    const file = fileInput.files[0];
    if (!file.name.endsWith('.bin')) { alert('El archivo debe ser .bin'); return; }

    const progress = document.getElementById('ota-progress');
    const fill     = document.getElementById('ota-progress-fill');
    const label    = document.getElementById('ota-progress-label');
    if (progress) progress.style.display = 'block';

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);
    xhr.upload.onprogress = (e) => {
        if (e.lengthComputable) {
            const pct = Math.round((e.loaded / e.total) * 100);
            if (fill)  fill.style.width = `${pct}%`;
            if (label) label.textContent = `Subiendo... ${pct}%`;
        }
    };
    xhr.onload = () => {
        if (label) label.textContent = xhr.responseText || 'Completado';
        logMsg('OTA: ' + xhr.responseText);
    };
    xhr.onerror = () => {
        if (label) label.textContent = 'Error en la transferencia';
        logMsg('OTA error', true);
    };
    xhr.send(file);
}

// =============================================================================
//  OVERLAY RENDERERS
// =============================================================================
function renderZones(eY, xY, dL, dR) {
    uiCtx.save();
    // North zone fill
    uiCtx.fillStyle = 'rgba(0,255,136,0.07)';
    uiCtx.fillRect(dL*SCALE_X, 0, (dR-dL)*SCALE_X, eY*SCALE_Y);
    // South zone fill
    uiCtx.fillStyle = 'rgba(0,212,255,0.07)';
    uiCtx.fillRect(dL*SCALE_X, xY*SCALE_Y, (dR-dL)*SCALE_X, 240-xY*SCALE_Y);
    // Entry line
    uiCtx.setLineDash([8,6]); uiCtx.lineWidth = 1.5;
    uiCtx.strokeStyle = 'rgba(0,255,136,0.9)';
    uiCtx.beginPath(); uiCtx.moveTo(0, eY*SCALE_Y); uiCtx.lineTo(320, eY*SCALE_Y); uiCtx.stroke();
    uiCtx.fillStyle = '#00ff88'; uiCtx.font = '9px monospace';
    uiCtx.fillText('NORTE · ENTRY', 4, eY*SCALE_Y - 4);
    // Exit line
    uiCtx.strokeStyle = 'rgba(0,212,255,0.9)';
    uiCtx.beginPath(); uiCtx.moveTo(0, xY*SCALE_Y); uiCtx.lineTo(320, xY*SCALE_Y); uiCtx.stroke();
    uiCtx.fillStyle = '#00d4ff';
    uiCtx.fillText('SUR · EXIT', 4, xY*SCALE_Y + 11);
    uiCtx.restore();
}

// W2-10 FIX: renderDeadZones now draws actual visual markers (not empty)
function renderDeadZones(dL, dR) {
    if (dL <= 0 && dR >= 31) return; // nothing to show
    uiCtx.save();
    uiCtx.fillStyle = 'rgba(239,68,68,0.10)';
    if (dL > 0)  uiCtx.fillRect(0,              0, dL*SCALE_X, 240);
    if (dR < 31) uiCtx.fillRect(dR*SCALE_X+1,   0, 320 - dR*SCALE_X, 240);
    uiCtx.setLineDash([4,4]); uiCtx.lineWidth = 1; uiCtx.strokeStyle = 'rgba(239,68,68,0.4)';
    if (dL > 0)  { uiCtx.beginPath(); uiCtx.moveTo(dL*SCALE_X, 0); uiCtx.lineTo(dL*SCALE_X, 240); uiCtx.stroke(); }
    if (dR < 31) { uiCtx.beginPath(); uiCtx.moveTo(dR*SCALE_X, 0); uiCtx.lineTo(dR*SCALE_X, 240); uiCtx.stroke(); }
    uiCtx.restore();
}

function renderUserLines() {
    uiCtx.save();
    uiCtx.lineWidth = 2;
    userLines.forEach((l, idx) => {
        const sx1 = l.x1 * SCALE_X, sy1 = l.y1 * SCALE_Y;
        const sx2 = l.x2 * SCALE_X, sy2 = l.y2 * SCALE_Y;
        uiCtx.strokeStyle = 'rgba(0,255,136,0.9)';
        uiCtx.beginPath(); uiCtx.moveTo(sx1, sy1); uiCtx.lineTo(sx2, sy2); uiCtx.stroke();

        // Direction arrow
        const dx = sx2-sx1, dy = sy2-sy1, len = Math.hypot(dx, dy);
        if (len > 0) {
            const udx = dx/len, udy = dy/len;
            const cx = (sx1+sx2)/2, cy = (sy1+sy2)/2;
            const nx = -udy, ny = udx;
            uiCtx.strokeStyle = '#00ff88'; uiCtx.fillStyle = '#00ff88';
            uiCtx.beginPath(); uiCtx.moveTo(cx, cy); uiCtx.lineTo(cx+nx*12, cy+ny*12); uiCtx.stroke();
            uiCtx.beginPath();
            uiCtx.moveTo(cx+nx*12, cy+ny*12);
            uiCtx.lineTo(cx+nx*12-udx*4-udy*4, cy+ny*12-udy*4+udx*4);
            uiCtx.lineTo(cx+nx*12-udx*4+udy*4, cy+ny*12-udy*4-udx*4);
            uiCtx.fill();
            uiCtx.beginPath(); uiCtx.arc(sx1, sy1, 3, 0, Math.PI*2); uiCtx.fill();
        }
        uiCtx.fillStyle = 'white'; uiCtx.font = '10px Arial';
        uiCtx.fillText(`L${idx+1}`, sx1+5, sy1-5);
    });
    // Line being drawn
    if (isEditingLines && currentLine) {
        uiCtx.strokeStyle = 'rgba(255,255,0,0.8)';
        uiCtx.beginPath();
        uiCtx.moveTo(currentLine.x1*SCALE_X, currentLine.y1*SCALE_Y);
        uiCtx.lineTo(currentLine.x2*SCALE_X, currentLine.y2*SCALE_Y);
        uiCtx.stroke();
    }
    uiCtx.restore();
}

function renderTrails(tracks) {
    if (!showTrail) return;
    tracks.forEach(t => {
        if (!trackTrails[t.id]) trackTrails[t.id] = [];
        const trail = trackTrails[t.id];
        trail.push({ x: t.x, y: t.y });
        if (trail.length > 15) trail.shift();
        if (trail.length < 2) return;
        uiCtx.save();
        for (let i = 1; i < trail.length; i++) {
            uiCtx.globalAlpha = (i / trail.length) * 0.7;
            uiCtx.strokeStyle = '#ffb300'; uiCtx.lineWidth = 1.5;
            uiCtx.beginPath();
            uiCtx.moveTo(trail[i-1].x*SCALE_X, trail[i-1].y*SCALE_Y);
            uiCtx.lineTo(trail[i].x*SCALE_X,   trail[i].y*SCALE_Y);
            uiCtx.stroke();
        }
        uiCtx.restore();
    });
    // W2-11 FIX: Set<number> comparison — use numeric id consistently
    const activeSet = new Set(tracks.map(t => t.id));
    Object.keys(trackTrails).forEach(k => {
        if (!activeSet.has(Number(k))) delete trackTrails[k];
    });
}

function renderTracks(tracks) {
    if (!tracks.length) return;
    uiCtx.save();
    tracks.forEach(t => {
        const px = t.x * SCALE_X, py = t.y * SCALE_Y;
        const color = t.zone_state===1 ? '#00ff88' : (t.zone_state===3 ? '#00d4ff' : '#ffb300');
        uiCtx.shadowColor = 'rgba(0,0,0,0.8)'; uiCtx.shadowBlur = 4;
        uiCtx.strokeStyle = color; uiCtx.lineWidth = 1.5;
        uiCtx.strokeRect(px-10, py-10, 20, 20);
        uiCtx.shadowBlur = 0;
        const vlen = Math.hypot(t.vx, t.vy);
        if (vlen > 0.05) {
            uiCtx.beginPath(); uiCtx.moveTo(px, py);
            uiCtx.lineTo(px+t.vx*SCALE_X*5, py+t.vy*SCALE_Y*5); uiCtx.stroke();
        }
        uiCtx.font = 'bold 10px Arial'; uiCtx.lineWidth = 2;
        uiCtx.strokeStyle = 'black'; uiCtx.strokeText(`T${t.id}`, px-7, py-12);
        uiCtx.fillStyle = color;    uiCtx.fillText(`T${t.id}`,   px-7, py-12);
    });
    uiCtx.restore();
}

// =============================================================================
//  LINE DRAWING EDITOR
// =============================================================================
function getPointerPos(e) {
    const rect = uiCanvas.getBoundingClientRect();
    // W2-1 FIX: touchend uses changedTouches (touches is empty on touchend)
    const src = (e.changedTouches && e.changedTouches.length > 0)
        ? e.changedTouches[0]
        : (e.touches && e.touches.length > 0 ? e.touches[0] : e);
    return {
        x: (src.clientX - rect.left)  / (rect.width  / 320),
        y: (src.clientY - rect.top)   / (rect.height / 240)
    };
}

const onPointerDown = (e) => {
    if (!isEditingLines) return;
    if (e.type === 'touchstart') e.preventDefault();
    if (userLines.length >= 4) {
        alert('Máximo 4 líneas de conteo');
        toggleLineEditor(true);
        return;
    }
    const p = getPointerPos(e);
    currentLine = { x1: p.x/SCALE_X, y1: p.y/SCALE_Y, x2: p.x/SCALE_X, y2: p.y/SCALE_Y };
};

const onPointerMove = (e) => {
    if (!isEditingLines || !currentLine) return;
    if (e.type === 'touchmove') e.preventDefault();
    const p = getPointerPos(e);
    currentLine.x2 = p.x / SCALE_X;
    currentLine.y2 = p.y / SCALE_Y;
};

const onPointerUp = (e) => {
    if (!isEditingLines || !currentLine) return;
    // W2-1 FIX: changedTouches on touchend
    if (e.type === 'touchend') e.preventDefault();
    const dx = currentLine.x2 - currentLine.x1;
    const dy = currentLine.y2 - currentLine.y1;
    if (Math.hypot(dx, dy) > 1.5) {   // at least 1.5 sensor pixels long
        userLines.push({ ...currentLine });
        CONFIG.use_segments = true;
        updateLineList();
    }
    currentLine = null;
};

uiCanvas.addEventListener('mousedown',  onPointerDown);
uiCanvas.addEventListener('mousemove',  onPointerMove);
uiCanvas.addEventListener('mouseup',    onPointerUp);
uiCanvas.addEventListener('touchstart', onPointerDown, { passive: false });
uiCanvas.addEventListener('touchmove',  onPointerMove, { passive: false });
uiCanvas.addEventListener('touchend',   onPointerUp,   { passive: false });

function toggleLineEditor(forceOff = false) {
    isEditingLines = forceOff ? false : !isEditingLines;
    const btn = document.getElementById('btn-edit-lines');
    if (!btn) return;
    if (isEditingLines) {
        btn.textContent = '💾 GUARDAR LÍNEAS';
        btn.className = 'btn-warn';
        uiCanvas.style.pointerEvents = 'auto';
        uiCanvas.style.cursor = 'crosshair';
    } else {
        btn.textContent = '✏ EDITAR LÍNEAS EN EL CANVAS';
        btn.className = 'btn-primary';
        uiCanvas.style.pointerEvents = 'none';
        uiCanvas.style.cursor = 'default';
        if (!forceOff) {
            sendCmd({ cmd: 'SET_COUNTING_LINES', lines: userLines });
            logMsg(`Líneas guardadas: ${userLines.length}`);
        }
    }
}

function clearAllLines() {
    if (userLines.length === 0) return;
    if (!confirm('¿Borrar todas las líneas de conteo?')) return;
    userLines = []; CONFIG.use_segments = false;
    updateLineList();
    sendCmd({ cmd: 'SET_COUNTING_LINES', lines: [] });
    logMsg('Líneas borradas');
}

function updateLineList() {
    const list = document.getElementById('lines-list');
    if (!list) return;
    if (userLines.length === 0) {
        list.textContent = 'Sin líneas definidas — modo legacy activo.';
        return;
    }
    list.innerHTML = userLines.map((l, i) =>
        `<div style="margin-bottom:3px;">L${i+1}: (${l.x1.toFixed(1)},${l.y1.toFixed(1)}) → (${l.x2.toFixed(1)},${l.y2.toFixed(1)})</div>`
    ).join('');
}

function toggleTrail() {
    showTrail = !showTrail;
    const btn = document.getElementById('btn-trail');
    if (btn) btn.classList.toggle('active', showTrail);
    if (!showTrail) trackTrails = {};
}

// =============================================================================
//  DEBUG DRAWER
// =============================================================================
function toggleDebug() {
    const drw = document.getElementById('debug-drawer');
    const icn = document.getElementById('debug-toggle-icon');
    if (!drw) return;
    drw.classList.toggle('hidden');
    if (icn) icn.textContent = drw.classList.contains('hidden') ? '▲' : '▼';
}

function logMsg(txt, isErr = false) {
    const cons = document.getElementById('debug-console');
    if (!cons) return;
    const el = document.createElement('div');
    el.textContent = `[${new Date().toLocaleTimeString('es',{hour12:false})}] ${txt}`;
    if (isErr) el.style.color = 'var(--accent-red)';
    cons.appendChild(el);
    while (cons.childElementCount > 50) cons.removeChild(cons.firstChild);
    cons.scrollTop = cons.scrollHeight;
}
