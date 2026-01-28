<?php
/* =======================
   CONFIG / SETUP
======================= */
$PICO_URL = "http://192.168.1.183";
$DB_HOST  = "192.168.1.113";
$DB_USER  = "pico";
$DB_PASS  = "pico";
$DB_NAME  = "pico";

$conn = new mysqli($DB_HOST, $DB_USER, $DB_PASS, $DB_NAME);
if ($conn->connect_error) {
    http_response_code(500);
    exit("Database connection failed");
}
?>
<?php
/* =======================
   AJAX HANDLERS
======================= */
if (isset($_GET["action"])) {

    if ($_GET["action"] === "led") {
        $resp = @file_get_contents("$PICO_URL/api/status");
        $obj  = json_decode($resp, true);
        echo json_encode(["led" => $obj["led"] ?? 0]);
        exit;
    }

    if ($_GET["action"] === "read") {
        $ch = curl_init("$PICO_URL/api/status");
        curl_setopt_array($ch, [
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT => 5
        ]);
        $resp = curl_exec($ch);
        curl_close($ch);

        if (!$resp) {
            http_response_code(504);
            exit(json_encode(["error" => "Pico unreachable"]));
        }

        $data = json_decode($resp, true);
        if (!$data) {
            http_response_code(500);
            exit(json_encode(["error" => "Bad JSON"]));
        }

        $LOG_INTERVAL = 60;
        $now = time();

        $last = 0;
        $res = $conn->query("SELECT MAX(UNIX_TIMESTAMP(timestamp)) AS t FROM log");
        if ($row = $res->fetch_assoc()) {
            $last = (int)$row["t"];
        }

        if (($now - $last) >= $LOG_INTERVAL) {
            $stmt = $conn->prepare(
                "INSERT INTO log (raw, temperature, led) VALUES (?, ?, ?)"
            );
            $stmt->bind_param("idi",
                $data["raw"],
                $data["temperature"],
                $data["led"]
            );
            $stmt->execute();
            $stmt->close();
        }

        echo json_encode($data);
        exit;
    }

    if ($_GET["action"] === "send" && isset($_POST["cmd"])) {
        $cmd = intval($_POST["cmd"]) & 0xFF;

        $ch = curl_init("$PICO_URL/api/control");
        curl_setopt_array($ch, [
            CURLOPT_POST => true,
            CURLOPT_HTTPHEADER => ["Content-Type: application/json"],
            CURLOPT_POSTFIELDS => json_encode(["led" => $cmd]),
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_TIMEOUT => 5
        ]);
        curl_exec($ch);
        curl_close($ch);

        echo json_encode(["ok" => true, "cmd" => $cmd]);
        exit;
    }

    if ($_GET["action"] === "history") {
        $res = $conn->query(
            "SELECT temperature, timestamp
             FROM log
             ORDER BY timestamp DESC
             LIMIT 30"
        );

        $rows = [];
        while ($r = $res->fetch_assoc()) {
            $rows[] = $r;
        }

        echo json_encode(array_reverse($rows));
        exit;
    }
}
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Pico 2W Monitor</title>

<style>
/* =======================
   PAGE LAYOUT
======================= */
body {
    background:#111;
    color:#eee;
    font-family:system-ui,sans-serif;
    margin:0;
}

.page {
    display:flex;
    justify-content:center;
}

.content {
    width:50%;
    min-width:720px;
    padding:20px;
}

/* =======================
   TYPOGRAPHY
======================= */
h1 { margin-bottom:5px; }
.tiny-text { font-size:12px; color:#aaa; }

/* =======================
   PANELS
======================= */
.panel {
    display:flex;
    align-items:center;
    gap:40px;
    margin-bottom:30px;
}

#current {
    font-size:48px;
    font-weight:bold;
}

/* =======================
   LED BUTTONS
======================= */
.cmd-btn {
    padding:12px 20px;
    margin-right:8px;
    border:none;
    border-radius:6px;
    color:white;
    cursor:pointer;
    transition:box-shadow .3s, transform .2s;
}

.cmd-btn.blue   { background:#0077ff; }
.cmd-btn.red    { background:#cc0000; }
.cmd-btn.yellow { background:#ffc800; color:#000; }
.cmd-btn.green  { background:#109a06; }

.cmd-btn.glow { transform:scale(1.05); }
.cmd-btn.blue.glow   { box-shadow:0 0 20px rgba(0,120,255,.8); }
.cmd-btn.red.glow    { box-shadow:0 0 20px rgba(255,0,0,.8); }
.cmd-btn.yellow.glow { box-shadow:0 0 20px rgba(255,220,0,.8); }
.cmd-btn.green.glow  { box-shadow:0 0 20px rgba(0,200,80,.8); }

/* =======================
   CHART
======================= */
.chart-wrap {
    display:flex;
    align-items:flex-end;
    gap:12px;
    margin-top:20px;
}

.y-axis {
    display:flex;
    flex-direction:column;
    justify-content:space-between;
    height:200px;
    font-size:11px;
    color:#888;
    text-align:right;
}

#chart {
    position:relative;
    display:flex;
    align-items:flex-end;
    gap:6px;
    height:200px;
    padding:10px;
    background:#222;
    border-radius:8px;
}

.bar {
    width:16px;
    border-radius:4px 4px 0 0;
    background:linear-gradient(to top,
        #0066ff 30%,
        #00cc66 50%,
        #ffeb3b 75%,
        #ff9800 90%,
        #f44336 100%);
    transition:height .4s;
}

/* Tooltip */
#tooltip {
    position:absolute;
    background:#000;
    color:#fff;
    padding:6px 8px;
    font-size:12px;
    border-radius:4px;
    pointer-events:none;
    opacity:0;
    transform:translate(-50%,-10px);
    transition:opacity .15s;
}
</style>
</head>

<body>
<div class="page">
<div class="content">

<h1>Pico 2W Monitor</h1>
<div id="last-updated" class="tiny-text">Last update —</div>

<div class="panel">
    <div>
        <div class="tiny-text">Current Temperature</div>
        <div id="current">-- °F</div>
    </div>

    <div>
        <h3>LED Control</h3>
        <button class="cmd-btn blue" data-cmd="8">Blue</button>
        <button class="cmd-btn red" data-cmd="4">Red</button>
        <button class="cmd-btn yellow" data-cmd="2">Yellow</button>
        <button class="cmd-btn green" data-cmd="1">Green</button>
        <div id="cmd-status" class="tiny-text"></div>
    </div>
</div>

<div class="chart-wrap">
    <div class="y-axis">
        <div>100</div><div>90</div><div>80</div><div>70</div><div>60</div>
        <div>50</div><div>40</div><div>30</div><div>20</div><div>10</div><div>0</div>
    </div>
    <div id="chart"><div id="tooltip"></div></div>
</div>

</div>
</div>

<script>
document.addEventListener("DOMContentLoaded", () => {

const $ = id => document.getElementById(id);

/* LED glow */
function updateLedGlow(val){
    document.querySelectorAll(".cmd-btn").forEach(btn=>{
        const bit=parseInt(btn.dataset.cmd,10);
        btn.classList.toggle("glow",(val & bit)!==0);
    });
}

async function pollLed(){
    try{
        const r=await fetch("?action=led");
        const j=await r.json();
        updateLedGlow(j.led);
    }catch{}
}

/* Status */
async function fetchStatus(){
    const r=await fetch("?action=read");
    const d=await r.json();

    $("current").textContent = Number(d.temperature).toFixed(1)+" °F";
    $("last-updated").textContent =
        "Last update: "+new Date().toLocaleTimeString();

    updateLedGlow(d.led);
    loadHistory();
}

/* History + tooltip */
async function loadHistory(){
    const chart=$("chart");
    const tooltip=$("tooltip");
    chart.querySelectorAll(".bar").forEach(b=>b.remove());

    const r=await fetch("?action=history");
    const data=await r.json();

    data.forEach(d=>{
        const temp=Number(d.temperature);
        const bar=document.createElement("div");
        bar.className="bar";
        bar.style.height=Math.min(200,temp*2)+"px";

        bar.addEventListener("mouseenter",e=>{
            tooltip.textContent = `${temp.toFixed(1)} °F`;
            tooltip.style.left = e.target.offsetLeft + "px";
            tooltip.style.bottom = bar.style.height;
            tooltip.style.opacity = 1;
        });

        bar.addEventListener("mouseleave",()=>tooltip.style.opacity=0);
        chart.appendChild(bar);
    });
}

/* Commands */
document.querySelectorAll(".cmd-btn").forEach(btn=>{
    btn.addEventListener("click",async()=>{
        await fetch("?action=send",{
            method:"POST",
            headers:{"Content-Type":"application/x-www-form-urlencoded"},
            body:"cmd="+encodeURIComponent(btn.dataset.cmd)
        });
    });
});

/* Timers */
fetchStatus();
setInterval(fetchStatus,300000);
setInterval(pollLed,500);

});
</script>
</body>
</html>
