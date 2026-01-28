<?php
/* =======================
   CONFIG / SETUP
======================= */
$PICO_URL = "http://your_local_ip";
$DB_HOST  = "localhost";
$DB_USER  = "pico";
$DB_PASS  = "pico";
$DB_NAME  = "pico";

$conn = new mysqli($DB_HOST, $DB_USER, $DB_PASS, $DB_NAME);
if ($conn->connect_error) {
    http_response_code(500);
    exit("Database connection failed");
}
?>