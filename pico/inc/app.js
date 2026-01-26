document.addEventListener("DOMContentLoaded", () => {

    const CHART_HEIGHT = 200;
    const MAX_TEMP = 100;
    const PX_PER_DEG = CHART_HEIGHT / MAX_TEMP;
    const $ = id => document.getElementById(id);

    document.getElementById("sendBtn").addEventListener("click", send);

    function send() {
        const text = document.getElementById("msg").value;
        fetch("?action=text", {
            method: "POST",
            headers: { "Content-Type": "application/x-www-form-urlencoded" },
            body: "text=" + encodeURIComponent(text)
            //body: JSON.stringify({ text })
        });
    }

    /* LED glow */
    function updateLedGlow(val) {
        document.querySelectorAll(".cmd-btn").forEach(btn => {
            const ledId = parseInt(btn.dataset.cmd, 10); // 1–4
            const bitmask = 1 << (ledId - 1);

            btn.classList.toggle("glow", (val & bitmask) !== 0);
        });
    }

    async function pollLed() {
        try {
            const r = await fetch("?action=led");
            const j = await r.json();
            updateLedGlow(j.led);
        } catch { }
    }

    /* Status */
    async function fetchStatus() {
        const r = await fetch("?action=read");
        const d = await r.json();
        $("current").textContent = Number(d.temperature).toFixed(1) + " °F";
        $("last-updated").textContent = "Last update: " + new Date().toLocaleTimeString();
        updateLedGlow(d.led);
        loadHistory();
    }

    /* History */
    async function loadHistory() {
        const chart = $("chart");
        const tooltip = $("tooltip");
        chart.querySelectorAll(".bar").forEach(b => b.remove());

        const r = await fetch("?action=history");
        const data = await r.json();

        data.forEach(d => {
            const temp = Number(d.temperature);
            const bar = document.createElement("div");
            bar.className = "bar";

            const h = Math.min(CHART_HEIGHT, temp * PX_PER_DEG);
            bar.style.height = h + "px";

            bar.addEventListener("mouseenter", () => {
                tooltip.textContent = `${temp.toFixed(1)} °F @ ${d.timestamp}`;
                tooltip.style.left = bar.offsetLeft + bar.offsetWidth / 2 + "px";
                tooltip.style.bottom = h + "px";
                tooltip.style.opacity = 1;
            });

            bar.addEventListener("mouseleave", () => tooltip.style.opacity = 0);
            chart.appendChild(bar);
        });
    }

    /* Commands */
    document.querySelectorAll(".cmd-btn").forEach(btn => {
        btn.addEventListener("click", () => {
            fetch("?action=send", {
                method: "POST",
                headers: { "Content-Type": "application/x-www-form-urlencoded" },
                body: "cmd=" + encodeURIComponent(btn.dataset.cmd)
            });
        });
    });

    /* Timers */
    fetchStatus();
    setInterval(fetchStatus, 300000);
    setInterval(pollLed, 500);

});
