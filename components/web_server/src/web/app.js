/* ESP32 Thermal Main App Logic */

// --- STATE GLOBALS ---
const CONFIG = {
    temp_bio: 25.0,
    delta_t: 1.5,
    alpha_ema: 0.05,
    line_entry: 8,
    line_exit: 16,
    nms_center: 16,
    nms_edge: 4,
    view_mode: 0 // 0=Normal, 1=Diff
};

let COLOR_THRESHOLDS = { low: 25.0, high: 38.0 };
let LUT = new Uint8Array(256 * 3); // Lookup table for colors
let ws = null;
let reconnectTimer = null;
const SENSOR_W = 32;
const SENSOR_H = 24;

// Auto-Gain termal
let autoMin = 20.0;
let autoMax = 35.0;

// Visual Canvas (Base Termal Escalada)
const UPSCALE = 2; // Factor de interpolación espacial pura (Opción B)
const UP_W = SENSOR_W * UPSCALE;
const UP_H = SENSOR_H * UPSCALE;

const canvas = document.getElementById('thermalCanvas');
const ctx = canvas.getContext('2d');
const offCanvas = document.createElement('canvas');
offCanvas.width = UP_W; offCanvas.height = UP_H;
const offCtx = offCanvas.getContext('2d', { willReadFrequently: true });
const targetImgData = offCtx.createImageData(UP_W, UP_H);

// --- OVERLAYS Y ESCALADO VECTORIAL ---
const uiCanvas = document.getElementById('uiCanvas');
const uiCtx = uiCanvas.getContext('2d');
// Nuestra constante de dibujo vectorial sobre matriz de baja resolución
const SCALE_X = 320 / SENSOR_W; // 10
const SCALE_Y = 240 / SENSOR_H; // 10

// Scale Canvas
const scaleCvs = document.getElementById('scaleCanvas');
const scaleCtx = scaleCvs.getContext('2d');

// --- INITIALIZATION ---
window.onload = () => {
    logMsg("App init");
    regenerateLUT(); 
    applyVisionModeClass('normal');
    connectWebSocket();
};

// --- MULTI VIEW TAB NAVIGATION ---
function switchMainView(viewId, btn) {
    document.querySelectorAll('.view-section').forEach(v => v.classList.remove('active'));
    document.getElementById(`view-${viewId}`).classList.add('active');
    
    document.querySelectorAll('.nav-item').forEach(b => b.classList.remove('active'));
    btn.classList.add('active');
}

function switchSettings(panelId) {
    document.querySelectorAll('.settings-panel').forEach(v => v.classList.remove('active'));
    document.getElementById(`set-${panelId}`).classList.add('active');
    
    document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
    event.target.classList.add('active');
}

function setVisionMode(mode) {
    document.querySelectorAll('.vision-controls .btn-pill').forEach(b => b.classList.remove('active'));
    event.target.classList.add('active');
    
    applyVisionModeClass(mode);
    
    // If differential, command ESP32 to switch view_mode = 1. Else 0.
    const espMode = (mode === 'diff') ? 1 : 0;
    if (espMode !== CONFIG.view_mode) {
        CONFIG.view_mode = espMode;
        if(ws && ws.readyState===1) {
            ws.send(JSON.stringify({cmd:"SET_PARAM", param:"view_mode", val:espMode}));
            logMsg(`ViewMode changed to ${mode} (${espMode})`);
        }
    }
}

function applyVisionModeClass(mode) {
    if (mode === 'raw') {
        canvas.style.imageRendering = 'pixelated';
        ctx.imageSmoothingEnabled = false;
        canvas.classList.remove('smooth-thermal');
    } else {
        canvas.style.imageRendering = 'auto';
        ctx.imageSmoothingEnabled = true;
        canvas.classList.add('smooth-thermal');
    }
}

// --- COLOR GRADIENT LUT ENGINE ---
function interpolateColor(color1, color2, factor) {
    return [
        Math.round(color1[0] + factor * (color2[0] - color1[0])),
        Math.round(color1[1] + factor * (color2[1] - color1[1])),
        Math.round(color1[2] + factor * (color2[2] - color1[2]))
    ];
}

function regenerateLUT() {
    // Pure Ironbow / Inferno Palette mapping points
    const c0 = [0, 0, 0];        // Black
    const c1 = [35, 0, 80];      // Dark Purple
    const c2 = [140, 0, 130];    // Magenta
    const c3 = [230, 75, 0];     // Orange
    const c4 = [255, 200, 20];   // Yellow
    const c5 = [255, 255, 255];  // White

    // Smooth Gradient construction
    for(let i=0; i<256; i++) {
        let f = i / 255.0;
        let rgb;
        if(f < 0.2)       rgb = interpolateColor(c0, c1, f / 0.2);
        else if(f < 0.5)  rgb = interpolateColor(c1, c2, (f - 0.2) / 0.3);
        else if(f < 0.75) rgb = interpolateColor(c2, c3, (f - 0.5) / 0.25);
        else if(f < 0.90) rgb = interpolateColor(c3, c4, (f - 0.75) / 0.15);
        else              rgb = interpolateColor(c4, c5, (f - 0.90) / 0.10);

        LUT[i*3]   = rgb[0];
        LUT[i*3+1] = rgb[1];
        LUT[i*3+2] = rgb[2];
    }
    
    // Draw Scale Bar Reference
    const sData = scaleCtx.createImageData(200, 10);
    for(let x=0; x<200; x++) {
        let i = Math.floor((x/199)*255) * 3;
        for(let y=0; y<10; y++) {
            let px = (y*200 + x)*4;
            sData.data[px] = LUT[i];
            sData.data[px+1] = LUT[i+1];
            sData.data[px+2] = LUT[i+2];
            sData.data[px+3] = 255;
        }
    }
    scaleCtx.putImageData(sData, 0, 0);
}

// --- WEBSOCKET NETWORK ---
function connectWebSocket() {
    let wsUrl = `ws://${window.location.host}/ws`;
    // For local dev debugging
    if (window.location.protocol === "file:") wsUrl = "ws://192.168.4.1/ws";
    
    ws = new WebSocket(wsUrl);
    ws.binaryType = "arraybuffer";

    ws.onopen = () => {
        logMsg("WS Connected");
        document.getElementById('lbl-ws').innerText = "Conectado";
        document.getElementById('dot-ws').className = "dot green";
        ws.send(JSON.stringify({cmd: "GET_CONFIG"}));
        if(reconnectTimer) clearInterval(reconnectTimer);
    };

    ws.onclose = () => {
        logMsg("WS Closed. Retrying in 2s...", true);
        document.getElementById('lbl-ws').innerText = "Desconectado";
        document.getElementById('dot-ws').className = "dot red";
        document.getElementById('dot-sensor').className = "dot gray";
        reconnectTimer = setTimeout(connectWebSocket, 2000);
    };

    ws.onerror = (e) => {
        logMsg("WS Error occurred", true);
    };

    ws.onmessage = (e) => {
        if(typeof e.data === "string") {
            try {
                let obj = JSON.parse(e.data);
                if(obj.type === "config") {
                    updateConfigUI(obj);
                }
            } catch(jsonErr) {}
        } else if(e.data instanceof ArrayBuffer) {
            // Decouple to prevent UI freeze under packet bursts
            latestFrameBuffer = e.data;
            if(!isRendering) {
                isRendering = true;
                requestAnimationFrame(renderLoop);
            }
        }
    };
}

// --- RENDERING PIPELINE DECOUPLE ---
let latestFrameBuffer = null;
let isRendering = false;

function processBinaryFrame(buffer) {
    try {
        const view = new DataView(buffer);
        if(view.getUint8(0) !== 0x11) return; // Header mismatch
        
        let ofs = 1;
    const sensorOk = view.getUint8(ofs++) === 1;
    const ambT = view.getFloat32(ofs, true); ofs += 4;
    const cntIn = view.getUint16(ofs, true); ofs += 2;
    const cntOut = view.getUint16(ofs, true); ofs += 2;
    const numTracks = view.getUint8(ofs++);
    
    // Memoria cliente para Track Smoothing Temporal (EMA de visualización)
    if(!window.clientTracks) window.clientTracks = {};
    const unseenIds = new Set(Object.keys(window.clientTracks));

    // Procesar Tracklets Array (Data: ID, X, Y, Vx, Vy)
    const tracks = [];
    for(let i=0; i<numTracks; i++) {
        let tid = view.getUint8(ofs++);
        let tx = view.getUint16(ofs, true) / 100.0; ofs += 2;
        let ty = view.getUint16(ofs, true) / 100.0; ofs += 2;
        let tvx = view.getInt16(ofs, true) / 100.0; ofs += 2;
        let tvy = view.getInt16(ofs, true) / 100.0; ofs += 2;
        
        let t;
        if(window.clientTracks[tid]) {
            t = window.clientTracks[tid];
            t.x = t.x * 0.7 + tx * 0.3;     // EMA espacial suaviza saltos enteros
            t.y = t.y * 0.7 + ty * 0.3;
            t.vx = t.vx * 0.8 + tvx * 0.2;  // EMA vector de velocidad suaviza brincos
            t.vy = t.vy * 0.8 + tvy * 0.2;
        } else {
            t = { id: tid, x: tx, y: ty, vx: tvx, vy: tvy };
            window.clientTracks[tid] = t;
        }
        unseenIds.delete(tid.toString());
        
        // Inferir zona visual desde posición Y para UI (basado en thresholds actuales)
        if (t.y < CONFIG.line_entry) t.zone_state = 1; // Norte
        else if (t.y >= CONFIG.line_exit) t.zone_state = 3; // Sur
        else t.zone_state = 2; // Neutra
        
        tracks.push(t);
    }
    // Purgar tracks muertos
    unseenIds.forEach(id => delete window.clientTracks[id]);
    
    // Thermal Matrix Decoding Below (Using DataView)
    document.getElementById('dot-sensor').className = sensorOk ? "dot green" : "dot red";
    document.getElementById('lbl-amb').innerText = `${ambT.toFixed(1)}°C`;
    document.getElementById('stat-in').innerText = cntIn;
    document.getElementById('stat-out').innerText = cntOut;
    
    let overIn = document.getElementById('overlay-in');
    let overOut = document.getElementById('overlay-out');
    if(overIn) overIn.innerText = `In: ${cntIn}`;
    if(overOut) overOut.innerText = `Out: ${cntOut}`;
    
    // Calulate rough FPS
    window.frameCount = (window.frameCount || 0) + 1;
    if(!window.lastFpsTime) window.lastFpsTime = performance.now();
    let now = performance.now();
    if(now - window.lastFpsTime > 1000) {
        document.getElementById('lbl-fps').innerText = window.frameCount + " Hz";
        window.frameCount = 0;
        window.lastFpsTime = now;
    }

    // Buscamos Mínimo y Máximo real para el Auto-Gain
    let frameMin = 999.0;
    let frameMax = -999.0;
    for(let i=0; i< SENSOR_W*SENSOR_H; i++) {
        let t = view.getInt16(ofs + i*2, true) / 100.0; // LEER BYTE ALIGNMENT SEGURO
        if(t < frameMin) frameMin = t;
        if(t > frameMax) frameMax = t;
    }
    
    // Suavizado del Auto-Gain por si hay saltos drásticos de ruido
    autoMin = (autoMin * 0.9) + (frameMin * 0.1);
    autoMax = (autoMax * 0.9) + (frameMax * 0.1);
    
    // Si estamos en diferencial, anclar simétricamente en 0 para que el ruido no salte
    if(CONFIG.view_mode === 1) {
        let maxAbs = Math.max(Math.abs(autoMin), Math.abs(autoMax));
        autoMax = maxAbs;
        autoMin = -maxAbs;
    }
    
    // Evitar que el contraste se comprima demasiado (min 8.0 grados de ventana)
    // Esto previene que el granulado de fondo estático explote abarcando todo el gradiente
    const MIN_RANGE = 8.0;
    if((autoMax - autoMin) < MIN_RANGE) {
        let mid = (autoMax + autoMin) / 2.0;
        autoMin = mid - (MIN_RANGE / 2.0);
        autoMax = mid + (MIN_RANGE / 2.0);
    }
    
    // Interpolación Espacial Bilineal (Opción B del Plan)
    // Expandimos orgánicamente de 32x24 a 64x48 calculando temperaturas vecinas reales.
    let upData = new Float32Array(UP_W * UP_H);
    for(let y=0; y<UP_H; y++) {
        let gy = (y / (UP_H - 1)) * (SENSOR_H - 1);
        let gyi = Math.floor(gy);
        let fy = gy - gyi;
        let gyi1 = Math.min(gyi+1, SENSOR_H-1);
        
        for(let x=0; x<UP_W; x++) {
            let gx = (x / (UP_W - 1)) * (SENSOR_W - 1);
            let gxi = Math.floor(gx);
            let fx = gx - gxi;
            let gxi1 = Math.min(gxi+1, SENSOR_W-1);
            
            let c00 = view.getInt16(ofs + (gyi*SENSOR_W + gxi)*2, true) / 100.0;
            let c10 = view.getInt16(ofs + (gyi*SENSOR_W + gxi1)*2, true) / 100.0;
            let c01 = view.getInt16(ofs + (gyi1*SENSOR_W + gxi)*2, true) / 100.0;
            let c11 = view.getInt16(ofs + (gyi1*SENSOR_W + gxi1)*2, true) / 100.0;
            
            let top = c00 * (1-fx) + c10 * fx;
            let bot = c01 * (1-fx) + c11 * fx;
            upData[y*UP_W + x] = top * (1-fy) + bot * fy;
        }
    }
    
    // Limits
    const MIN_T = autoMin;
    const MAX_T = autoMax;
    const RANGE = MAX_T - MIN_T;
    
    const pxData = targetImgData.data;
    for(let i=0; i< UP_W * UP_H; i++) {
        let t = upData[i];
        
        if(t < MIN_T) t = MIN_T;
        if(t > MAX_T) t = MAX_T;
        
        let norm = (t - MIN_T) / RANGE;
        let lutIdx = Math.floor(norm * 255) * 3;
        if(lutIdx > 255*3) lutIdx = 255*3;
        
        pxData[i*4] = LUT[lutIdx];
        pxData[i*4+1] = LUT[lutIdx+1];
        pxData[i*4+2] = LUT[lutIdx+2];
        pxData[i*4+3] = 255;
    }
    
    // Draw via Offscreen for proper HW scaling
    offCtx.putImageData(targetImgData, 0, 0);
    ctx.drawImage(offCanvas, 0, 0, 320, 240);
    
    // Actualizar etiquetas visuales
    document.getElementById('scale-min').innerText = `${MIN_T.toFixed(1)}°C`;
    document.getElementById('scale-max').innerText = `${MAX_T.toFixed(1)}°C`;
    
    // Dibujar Zonas y Tracks sobre el Overlay Vectorial
    renderCountingZones(CONFIG.line_entry, CONFIG.line_exit);
    renderTracks(tracks);
    
    } catch(e) {
        if(!window.lastErr || performance.now() - window.lastErr > 3000) {
            logMsg("BinFrame Err: " + e.message, true);
            window.lastErr = performance.now();
        }
    }
}

function renderLoop() {
    isRendering = false;
    if(!latestFrameBuffer) return;
    let buf = latestFrameBuffer;
    latestFrameBuffer = null;
    processBinaryFrame(buf);
}

// --- OVERLAY RENDER ENGINES ---
function renderCountingZones(eY, xY) {
    uiCtx.clearRect(0,0,320,240);
    uiCtx.save();
    
    // Zona Norte
    uiCtx.fillStyle = 'rgba(0, 255, 136, 0.08)';
    uiCtx.fillRect(0, 0, 320, eY * SCALE_Y);
    // Zona Sur
    uiCtx.fillStyle = 'rgba(0, 212, 255, 0.08)';
    uiCtx.fillRect(0, xY * SCALE_Y, 320, 240 - (xY * SCALE_Y));
    
    // Lineas Dasheadas
    uiCtx.setLineDash([8, 6]);
    uiCtx.lineWidth = 1.5;
    
    // Entry (Norte)
    let pE = eY * SCALE_Y;
    uiCtx.strokeStyle = 'rgba(0, 255, 136, 0.9)';
    uiCtx.beginPath(); uiCtx.moveTo(0, pE); uiCtx.lineTo(320, pE); uiCtx.stroke();
    uiCtx.fillStyle = '#00ff88'; uiCtx.font = '10px monospace';
    uiCtx.fillText('NORTE ( ENTRY )', 5, pE - 5);
    
    // Exit (Sur)
    let pX = xY * SCALE_Y;
    uiCtx.strokeStyle = 'rgba(0, 212, 255, 0.9)';
    uiCtx.beginPath(); uiCtx.moveTo(0, pX); uiCtx.lineTo(320, pX); uiCtx.stroke();
    uiCtx.fillStyle = '#00d4ff'; 
    uiCtx.fillText('SUR ( EXIT )', 5, pX + 12);
    
    uiCtx.restore();
}

function renderTracks(tracks) {
    if(!tracks.length) return;
    uiCtx.save();
    
    tracks.forEach(t => {
        let px = t.x * SCALE_X;
        let py = t.y * SCALE_Y;
        
        let color = '#fff';
        if(t.zone_state===1) color = '#00ff88';
        else if(t.zone_state===2) color = '#ffb300';
        else if(t.zone_state===3) color = '#00d4ff';
        
        // Caja de puntería contrastada
        uiCtx.shadowColor = 'rgba(0,0,0,0.8)';
        uiCtx.shadowBlur = 4;
        uiCtx.strokeStyle = color;
        uiCtx.lineWidth = 1.5;
        uiCtx.strokeRect(px - 10, py - 10, 20, 20);
        uiCtx.shadowBlur = 0; // Apagar para otras figuras
        
        // Vector vel
        let vlen = Math.sqrt(t.vx*t.vx + t.vy*t.vy);
        if(vlen > 0.05) {
            uiCtx.beginPath();
            uiCtx.moveTo(px, py);
            uiCtx.lineTo(px + (t.vx*SCALE_X*5), py + (t.vy*SCALE_Y*5));
            uiCtx.stroke();
        }
        
        // Etiqueta HD
        uiCtx.font = 'bold 11px Arial';
        uiCtx.lineWidth = 2;
        uiCtx.strokeStyle = 'black';
        uiCtx.strokeText(`Tº${t.id}`, px-8, py - 15);
        uiCtx.fillStyle = color;
        uiCtx.fillText(`Tº${t.id}`, px-8, py - 15);
    });
    
    uiCtx.restore();
}

// --- CONFIG ACTIONS ---
function updateConfigUI(obj) {
    if(document.getElementById('cfg-temp_bio')) document.getElementById('cfg-temp_bio').value = obj.temp_bio;
    if(document.getElementById('cfg-delta_t')) document.getElementById('cfg-delta_t').value = obj.delta_t;
    if(document.getElementById('cfg-alpha_ema')) document.getElementById('cfg-alpha_ema').value = obj.alpha_ema;
    if(document.getElementById('cfg-line_entry')) document.getElementById('cfg-line_entry').value = obj.line_entry;
    if(document.getElementById('cfg-line_exit')) document.getElementById('cfg-line_exit').value = obj.line_exit;
    if(document.getElementById('cfg-nms_center')) document.getElementById('cfg-nms_center').value = obj.nms_center;
    if(document.getElementById('cfg-nms_edge')) document.getElementById('cfg-nms_edge').value = obj.nms_edge;
    
    CONFIG.temp_bio = obj.temp_bio || CONFIG.temp_bio;
    CONFIG.line_entry = obj.line_entry || CONFIG.line_entry;
    CONFIG.line_exit = obj.line_exit || CONFIG.line_exit;
    CONFIG.view_mode = obj.view_mode;
    
    // Set UI buttons according to mode
    document.querySelectorAll('.vision-controls .btn-pill').forEach(b => b.classList.remove('active'));
    if(CONFIG.view_mode === 1) {
        applyVisionModeClass('diff');
        document.querySelector('.vision-controls [data-mode="diff"]').classList.add('active');
    } else {
        // default normal if 0
        applyVisionModeClass('normal');
        document.querySelector('.vision-controls [data-mode="normal"]').classList.add('active');
    }
}

document.querySelectorAll('input').forEach(inp => {
    inp.addEventListener('change', (e) => {
        let key = e.target.id.replace('cfg-', '');
        // Color limits don't go to ESP32, they are local
        if(key === 'color_low' || key === 'color_high') return;
        
        let val = parseFloat(e.target.value);
        if(ws && ws.readyState===1) {
            ws.send(JSON.stringify({cmd:"SET_PARAM", param:key, val:val}));
            logMsg(`Set ${key}=${val}`);
        }
    });
});

function saveConfig() {
    if(ws && ws.readyState===1) {
        ws.send(JSON.stringify({cmd:"SAVE_CONFIG"}));
        logMsg("Config save req sent.");
    }
}

function appCmd(cmdStr) {
    if(ws && ws.readyState===1) {
        ws.send(JSON.stringify({cmd:cmdStr}));
        logMsg(`CMD: ${cmdStr}`);
    }
}

function rebootEsp() {
    if(confirm('¿Reiniciar el ESP32 por completo?')) {
        fetch('/reboot', {method:'POST'}).then(()=>logMsg("Reboot triggered"));
    }
}

// --- DEBUGGER DRAWER ---
function toggleDebug() {
    const drw = document.getElementById('debug-drawer');
    const icn = document.getElementById('debug-toggle-icon');
    if(drw.classList.contains('hidden')) {
        drw.classList.remove('hidden');
        icn.innerText = '▼';
    } else {
        drw.classList.add('hidden');
        icn.innerText = '▲';
    }
}

function logMsg(txt, isErr=false) {
    let p = document.createElement('div');
    p.textContent = `[${new Date().toLocaleTimeString()}] ${txt}`;
    if(isErr) p.style.color = "var(--danger)";
    const cons = document.getElementById('debug-console');
    if(!cons) return;
    cons.appendChild(p);
    while(cons.childElementCount > 40) cons.removeChild(cons.firstChild);
    cons.scrollTop = cons.scrollHeight;
}
