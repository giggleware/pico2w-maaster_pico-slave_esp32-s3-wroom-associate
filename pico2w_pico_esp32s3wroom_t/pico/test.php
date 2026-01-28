<?php

define("ESP_URL","192.168.1.248");

// Initialize a cURL session
$ch = curl_init();

// Set the URL
curl_setopt($ch, CURLOPT_URL, "http://".ESP_URL."/api/status");

// *** The crucial step ***
// Return the transfer as a string instead of outputting it directly
curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);

// Execute the cURL session and get the response
$response = curl_exec($ch);

// Check for errors
if (curl_errno($ch)) {
    echo 'cURL error: ' . curl_error($ch);
} else {
    // Process the response (e.g., decode JSON)
    $data = json_decode($response, true);
    echo "<pre>";
    print_r($data);
    echo "</pre>";
}

// Close the cURL session
curl_close($ch);

if(isset($_POST) && isset($_POST['text']) && strlen($_POST['text'])){
    $text = $_POST['text'];
    $ch = curl_init(ESP_URL."/api/text");
    curl_setopt_array($ch, [
        CURLOPT_POST => true,
        CURLOPT_HTTPHEADER => ["Content-Type: application/json"],
        CURLOPT_POSTFIELDS => json_encode(["text" => $text]),
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_TIMEOUT => 5
    ]);
    curl_exec($ch);
    curl_close($ch);
}
?>

<form name="text" method="post" action="">
<input type="text" name="text" value="">
<input type="submit" name="Submit">
</form>