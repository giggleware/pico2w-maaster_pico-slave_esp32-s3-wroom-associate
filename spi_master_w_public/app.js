// app.js - web UI logic for reading /value and sending /control

const valueDisplay = document.getElementById('valueDisplay');
const tempDisplay = document.getElementById('tempDisplay');
const ledHex = document.getElementById('ledHex');
const ledBin = document.getElementById('ledBin');

const btnRed = document.getElementById('btnRed');
const btnYellow = document.getElementById('btnYellow');
const btnGreen = document.getElementById('btnGreen');
const btnRelay = document.getElementById('btnRelay');

const ledRedDot = document.getElementById('ledRedDot');
const ledYellowDot = document.getElementById('ledYellowDot');
const ledGreenDot = document.getElementById('ledGreenDot');
const ledRelayDot = document.getElementById('ledRelayDot');

const loader = document.getElementById('loader');
const mainEl = document.querySelector('main');

// ----- Graph Setup -----
const graphCanvas = document.getElementById("graphCanvas");
const gctx = graphCanvas.getContext("2d");
let graphData = [];   // store recent temperatures
const MAX_POINTS = 200;  // length of history shown

let latestData = {
    raw: 0,
    temperature: 0,
    led: 0
};

// local tracked desired LED state (3 bits)
let desiredLed = 0;

// helper: convert number to 8-bit binary string
function toBin8(v) {
    let s = (v & 0xFF).toString(2);
    while (s.length < 8) s = '0' + s;
    return s;
}

function drawGraph() {
    if (!gctx) return;

    const w = graphCanvas.width;
    const h = graphCanvas.height;

    // Clear background
    gctx.clearRect(0, 0, w, h);

    if (graphData.length < 2) return;

    // ---- Auto-scale ----
    const maxVal = Math.max(...graphData);
    const minVal = Math.min(...graphData);
    const range = maxVal - minVal || 1;

    // ---- Smoothing (optional) ----
    const smoothed = [];
    const SMOOTH_WINDOW = 5;   // adjust 1–20 for strength

    for (let i = 0; i < graphData.length; i++) {
        let start = Math.max(0, i - SMOOTH_WINDOW);
        let end = Math.min(graphData.length, i + SMOOTH_WINDOW);
        let subset = graphData.slice(start, end);
        let avg = subset.reduce((a, b) => a + b, 0) / subset.length;
        smoothed.push(avg);
    }

    // ---- Grid lines ----
    gctx.strokeStyle = "#cccccc40";
    gctx.lineWidth = 1;

    // vertical lines every 50px
    for (let x = 50; x < w; x += 50) {
        gctx.beginPath();
        gctx.moveTo(x, 0);
        gctx.lineTo(x, h);
        gctx.stroke();
    }

    // horizontal lines (5 of them)
    for (let i = 0; i <= 5; i++) {
        let y = (i / 5) * h;
        gctx.beginPath();
        gctx.moveTo(0, y);
        gctx.lineTo(w, y);
        gctx.stroke();
    }

    // ---- Axis labels ----
    gctx.fillStyle = "#888";
    gctx.font = "12px Arial";

    gctx.fillText(maxVal.toFixed(1), 5, 12);
    gctx.fillText(((maxVal + minVal) / 2).toFixed(1), 5, h / 2);
    gctx.fillText(minVal.toFixed(1), 5, h - 6);

    // ---- Draw line ----
    gctx.beginPath();
    let y0 = h - ((smoothed[0] - minVal) / range) * h;
    gctx.moveTo(0, y0);

    for (let i = 1; i < smoothed.length; i++) {
        const x = (i / (smoothed.length - 1)) * w;
        const y = h - ((smoothed[i] - minVal) / range) * h;
        gctx.lineTo(x, y);
    }

    gctx.strokeStyle = "#00aaff";
    gctx.lineWidth = 2;
    gctx.stroke();
}

function updateLedUIFromByte(b) {
    // LED dots
    (b & 0x08) ? ledRelayDot.classList.add('on', 'blue') : ledRelayDot.classList.remove('on', 'blue');
    (b & 0x04) ? ledRedDot.classList.add('on', 'red') : ledRedDot.classList.remove('on', 'red');
    (b & 0x02) ? ledYellowDot.classList.add('on', 'yellow') : ledYellowDot.classList.remove('on', 'yellow');
    (b & 0x01) ? ledGreenDot.classList.add('on', 'green') : ledGreenDot.classList.remove('on', 'green');

    // Buttons lit/unlit
    (b & 0x08) ? btnRelay.classList.add('on', 'blue') : btnRelay.classList.remove('on', 'blue');
    (b & 0x04) ? btnRed.classList.add('on', 'red') : btnRed.classList.remove('on', 'red');
    (b & 0x02) ? btnYellow.classList.add('on', 'yellow') : btnYellow.classList.remove('on', 'yellow');
    (b & 0x01) ? btnGreen.classList.add('on', 'green') : btnGreen.classList.remove('on', 'green');

    // Debug displays
    ledHex.textContent = '0x' + (b & 0xFF).toString(16).padStart(2, '0').toUpperCase();
    ledBin.textContent = (b & 0xFF).toString(2).padStart(8, '0');
}

// send control byte to server
async function sendLedByte(b) {
    try {
        const res = await fetch('/control', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ led: b })
        });
        if (!res.ok) console.warn('control post failed', res.status);
    } catch (e) {
        console.warn('control post error', e);
    }
}

// ----- LED Button Handlers -----
btnRelay.addEventListener('click', () => {
    sendLedByte(0x08);
    document.getElementById("to-slave").innerHTML = "TO: 8";
});

btnRed.addEventListener('click', () => {
    sendLedByte(0x04);
    document.getElementById("to-slave").innerHTML = "TO: 4";
});

btnYellow.addEventListener('click', () => {
    sendLedByte(0x02);
    document.getElementById("to-slave").innerHTML = "TO: 2";
});

btnGreen.addEventListener('click', () => {
    sendLedByte(0x01);
    document.getElementById("to-slave").innerHTML = "TO: 1";
});

// ----- Poll Slave -----
async function pollValue() {
    try {
        const r = await fetch('/value');
        if (!r.ok) { console.warn('value fetch failed', r.status); return; }

        const j = await r.json();
        let tempRaw = null;

        // NEW response format
        if (j.led_state !== undefined) {
            desiredLed = j.led_state;
            document.getElementById("from-slave").innerHTML = "FROM: " + desiredLed;
            updateLedUIFromByte(desiredLed);
        }

        if (j.temp_raw !== undefined) {
            tempRaw = j.temp_raw;
            tempDisplay.textContent = tempRaw.toString();

            latestData.raw = j.value;
            latestData.temperature = j.temp_raw;
            latestData.led = j.led_state;
        }

        // OLD combined 24-bit packed format
        if (j.value !== undefined) {
            let packed = j.value;
            const ledByte = (packed >> 16) & 0xFF;
            tempRaw = packed & 0xFFFF;

            valueDisplay.textContent = packed;
            tempDisplay.textContent = tempRaw;

            desiredLed = ledByte;
            updateLedUIFromByte(ledByte);
        }

        // ----- Graph update -----
        if (typeof tempRaw === "number") {
            graphData.push(tempRaw);
            if (graphData.length > MAX_POINTS) graphData.shift();
            drawGraph();
        }

    } catch (e) {
        console.warn('poll error', e);
    } finally {
        setTimeout(pollValue, 500);
    }
}

// Load saved theme
if (localStorage.getItem("theme") === "dark") {
    document.body.classList.add("dark");
    themeToggle.textContent = "☀️";
}

// Save theme when changed
themeToggle.addEventListener("click", () => {
    document.body.classList.toggle("dark");

    if (document.body.classList.contains("dark")) {
        localStorage.setItem("theme", "dark");
        themeToggle.textContent = "☀️";
    } else {
        localStorage.setItem("theme", "light");
        themeToggle.textContent = "🌙";
    }
});

// ----- Init -----
window.addEventListener('load', () => {
    loader.style.display = 'none';
    mainEl.style.opacity = 1;
    pollValue();
});
