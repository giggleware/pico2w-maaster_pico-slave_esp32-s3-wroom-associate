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

    if ($_GET["action"] === "text" && isset($_POST["text"])) {

        // Sanitize and limit length (16x2 OLED = 32 chars)
        $text = substr($_POST["text"], 0, 32);

        $ch = curl_init("$PICO_URL/api/text");
        curl_setopt_array($ch, [
            CURLOPT_POST            => true,
            CURLOPT_HTTPHEADER      => ["Content-Type: application/json"],
            CURLOPT_POSTFIELDS      => json_encode([
                "text" => $text
            ]),
            CURLOPT_RETURNTRANSFER  => true,
            CURLOPT_TIMEOUT         => 5
        ]);

        $resp = curl_exec($ch);
        $err  = curl_error($ch);
        curl_close($ch);

        if ($resp === false) {
            http_response_code(500);
            echo json_encode([
                "ok"    => false,
                "error" => $err
            ]);
            exit;
        }

        echo json_encode([
            "ok"   => true,
            "text" => $text
        ]);
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
