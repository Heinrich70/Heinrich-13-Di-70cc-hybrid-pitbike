#include <WiFiS3.h>
#include <EEPROM.h>
#include "arduino_secrets.h"

// =====================================================
// HYBRID PITBIKE - WIFI + THROTTLE + CLUTCH + EEPROM
// =====================================================

// ================== Pins ==================

const int throttleHallPin = A2;
const int clutchHallPin   = A1;

const int throttleOutPin  = 5;
const int clutchMosfetPin = 6;


// ================== EEPROM ==================

const int EEPROM_CLUTCH_THRESHOLD = 0;


// ================== Throttle ==================

const int throttleLow  = 510;
const int throttleHigh = 850;


// ================== Clutch ==================

int clutchThreshold = 510;

const int thresholdStep = 5;


// ================== WiFi ==================

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;

WiFiServer server(80);


// ================== Global Variables ==================

int throttleRaw = 0;
int clutchRaw = 0;

int throttlePWM = 0;

bool clutchState = false;


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("   HYBRID PITBIKE STARTING");
  Serial.println("================================");


  // ================== Pins ==================

  pinMode(throttleHallPin, INPUT);
  pinMode(clutchHallPin, INPUT);

  pinMode(throttleOutPin, OUTPUT);
  pinMode(clutchMosfetPin, OUTPUT);

  analogWrite(throttleOutPin, 0);
  digitalWrite(clutchMosfetPin, LOW);


  // ================== Load EEPROM ==================

  EEPROM.get(
    EEPROM_CLUTCH_THRESHOLD,
    clutchThreshold
  );

  // Check saved value is valid

  if (
    clutchThreshold < 0 ||
    clutchThreshold > 1023
  ) {

    clutchThreshold = 510;
  }


  Serial.print("Clutch threshold loaded: ");
  Serial.println(clutchThreshold);


  // ================== WiFi Module ==================

  if (WiFi.status() == WL_NO_MODULE) {

    Serial.println(
      "ERROR: WiFi module not detected!"
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("WiFi module detected.");

  Serial.print("SSID: ");
  Serial.println(ssid);


  // ================== Connect ==================

  Serial.println("Connecting to WiFi...");

  status = WiFi.begin(ssid, pass);

  int attempts = 0;

  while (
    status != WL_CONNECTED &&
    attempts < 30
  ) {

    delay(1000);

    Serial.print(".");

    status = WiFi.status();

    attempts++;
  }

  Serial.println();


  // ================== Connected ==================

  if (status == WL_CONNECTED) {

    Serial.println(
      "=============================="
    );

    Serial.println("WIFI CONNECTED!");

    Serial.println(
      "=============================="
    );

    Serial.println(
      "Waiting for IP address..."
    );


    unsigned long startTime = millis();


    while (
      WiFi.localIP() ==
        IPAddress(0, 0, 0, 0) &&
      millis() - startTime < 15000
    ) {

      delay(200);

      Serial.print(".");
    }

    Serial.println();


    IPAddress ip = WiFi.localIP();


    if (
      ip != IPAddress(0, 0, 0, 0)
    ) {

      Serial.print("IP Address: ");
      Serial.println(ip);

      Serial.print("Signal strength: ");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");

      server.begin();

      Serial.println(
        "Web server started."
      );

      Serial.println();

    } else {

      Serial.println(
        "ERROR: No IP address assigned."
      );

      Serial.println(
        "DHCP failed."
      );
    }


  } else {

    Serial.println(
      "=============================="
    );

    Serial.println(
      "WIFI CONNECTION FAILED"
    );

    Serial.println(
      "=============================="
    );

    Serial.print(
      "WiFi status code: "
    );

    Serial.println(status);
  }
}


// =====================================================
// LOOP
// =====================================================

void loop() {

  // ================== Read Sensors ==================

  throttleRaw =
    analogRead(throttleHallPin);

  clutchRaw =
    analogRead(clutchHallPin);


  // ================== Throttle ==================

  throttlePWM = map(
    throttleRaw,
    throttleLow,
    throttleHigh,
    0,
    255
  );

  throttlePWM = constrain(
    throttlePWM,
    0,
    255
  );

  analogWrite(
    throttleOutPin,
    throttlePWM
  );


  // ================== Clutch ==================

  if (
    clutchRaw >= clutchThreshold
  ) {

    clutchState = true;

    digitalWrite(
      clutchMosfetPin,
      HIGH
    );

  } else {

    clutchState = false;

    digitalWrite(
      clutchMosfetPin,
      LOW
    );
  }


  // ================== Serial ==================

  static unsigned long lastPrint = 0;

  if (
    millis() - lastPrint >= 500
  ) {

    Serial.print(
      "Throttle A2: "
    );

    Serial.print(throttleRaw);

    Serial.print(
      " | PWM D5: "
    );

    Serial.print(throttlePWM);

    Serial.print(
      " | Clutch A1: "
    );

    Serial.print(clutchRaw);

    Serial.print(
      " | Threshold: "
    );

    Serial.print(clutchThreshold);

    Serial.print(
      " | MOSFET D6: "
    );

    if (clutchState) {
      Serial.println("HIGH");
    } else {
      Serial.println("LOW");
    }

    lastPrint = millis();
  }


  // ================== Web Server ==================

  WiFiClient client =
    server.available();

  if (client) {

    handleClient(client);

  }
}


// =====================================================
// HANDLE CLIENT
// =====================================================

void handleClient(
  WiFiClient &client
) {

  String request = "";

  unsigned long timeout =
    millis();


  while (
    client.connected() &&
    millis() - timeout < 500
  ) {

    if (client.available()) {

      char c = client.read();

      request += c;

      if (c == '\n') {
        break;
      }

      timeout = millis();
    }
  }


  // ================== Dashboard ==================

  if (
    request.indexOf(
      "GET / "
    ) >= 0
  ) {

    sendDashboard(client);
  }


  // ================== Status ==================

  else if (
    request.indexOf(
      "GET /status"
    ) >= 0
  ) {

    sendStatus(client);
  }


  // ================== Threshold Down ==================

  else if (
    request.indexOf(
      "GET /threshold/down"
    ) >= 0
  ) {

    clutchThreshold -=
      thresholdStep;


    clutchThreshold =
      constrain(
        clutchThreshold,
        0,
        1023
      );


    EEPROM.put(
      EEPROM_CLUTCH_THRESHOLD,
      clutchThreshold
    );


    sendOK(client);
  }


  // ================== Threshold Up ==================

  else if (
    request.indexOf(
      "GET /threshold/up"
    ) >= 0
  ) {

    clutchThreshold +=
      thresholdStep;


    clutchThreshold =
      constrain(
        clutchThreshold,
        0,
        1023
      );


    EEPROM.put(
      EEPROM_CLUTCH_THRESHOLD,
      clutchThreshold
    );


    sendOK(client);
  }


  client.stop();
}


// =====================================================
// DASHBOARD
// =====================================================

void sendDashboard(
  WiFiClient &client
) {

  client.println(
    "HTTP/1.1 200 OK"
  );

  client.println(
    "Content-Type: text/html"
  );

  client.println(
    "Connection: close"
  );

  client.println();


  client.println(R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1.0">

<title>Hybrid Pitbike</title>

<style>

body {

  margin: 0;

  padding: 20px;

  background: #111;

  color: #00ff00;

  font-family: Arial, sans-serif;

  text-align: center;

}

h1 {

  margin-bottom: 30px;

}

.card {

  background: #1c1c1c;

  border-radius: 15px;

  padding: 20px;

  margin: 15px auto;

  max-width: 500px;

}

.value {

  font-size: 32px;

  font-weight: bold;

  margin: 10px;

}

.bar {

  width: 90%;

  height: 25px;

  background: #333;

  border-radius: 15px;

  margin: auto;

  overflow: hidden;

}

.barFill {

  height: 100%;

  width: 0%;

  background: #00ff00;

}

.state {

  font-size: 25px;

  font-weight: bold;

}

.thresholdValue {

  font-size: 30px;

  font-weight: bold;

  margin: 15px;

}

.thresholdButton {

  width: 80px;

  height: 60px;

  font-size: 32px;

  font-weight: bold;

  margin: 5px;

  border-radius: 12px;

  border: none;

  cursor: pointer;

}

</style>

</head>


<body>

<h1>70cc Hybrid Pitbike</h1>


<!-- ================= THROTTLE ================= -->

<div class="card">

<h2>Throttle</h2>

<div class="value">

<span id="throttleRaw">
---
</span>

</div>

<p>Hall A2 Raw</p>


<div class="bar">

<div class="barFill"
     id="throttleBar">
</div>

</div>


<p>

Output PWM:

<span id="throttlePWM">
---
</span>%

</p>

</div>


<!-- ================= CLUTCH ================= -->

<div class="card">

<h2>Clutch</h2>

<div class="value">

<span id="clutchRaw">
---
</span>

</div>

<p>Hall A1 Raw</p>


<p>

MOSFET D6:

<span class="state"
      id="clutchState">
OFF
</span>

</p>


<hr>


<h3>
Clutch Disengage Threshold
</h3>


<div class="thresholdValue">

<span id="threshold">
510
</span>

</div>


<button
  class="thresholdButton"
  onclick="changeThreshold('down')">
−
</button>


<button
  class="thresholdButton"
  onclick="changeThreshold('up')">
+
</button>


</div>


<script>


// =====================================================
// UPDATE STATUS
// =====================================================

function updateStatus() {

  fetch(
    '/status',
    {
      cache: 'no-store'
    }
  )

  .then(
    response => response.json()
  )

  .then(data => {

    document.getElementById(
      'throttleRaw'
    ).innerText =
      data.throttleRaw;


    document.getElementById(
      'clutchRaw'
    ).innerText =
      data.clutchRaw;


    document.getElementById(
      'throttlePWM'
    ).innerText =
      data.throttlePWM;


    document.getElementById(
      'throttleBar'
    ).style.width =
      data.throttlePercent + '%';


    document.getElementById(
      'clutchState'
    ).innerText =
      data.clutchState
        ? 'ON'
        : 'OFF';


    document.getElementById(
      'threshold'
    ).innerText =
      data.clutchThreshold;

  })

  .catch(
    error => console.log(error)
  );

}


// =====================================================
// CHANGE THRESHOLD
// =====================================================

function changeThreshold(
  direction
) {

  fetch(
    '/threshold/' + direction,
    {
      cache: 'no-store'
    }
  )

  .then(() => {

    updateStatus();

  });

}


// =====================================================
// START
// =====================================================

setInterval(
  updateStatus,
  100
);

updateStatus();


</script>


</body>

</html>

)rawliteral");
}


// =====================================================
// STATUS
// =====================================================

void sendStatus(
  WiFiClient &client
) {

  int throttlePercent =
    map(
      throttlePWM,
      0,
      255,
      0,
      100
    );


  client.println(
    "HTTP/1.1 200 OK"
  );

  client.println(
    "Content-Type: application/json"
  );

  client.println(
    "Cache-Control: no-store"
  );

  client.println(
    "Connection: close"
  );

  client.println();


  client.print("{");


  client.print(
    "\"throttleRaw\":"
  );

  client.print(
    throttleRaw
  );


  client.print(",");


  client.print(
    "\"throttlePWM\":"
  );

  client.print(
    throttlePercent
  );


  client.print(",");


  client.print(
    "\"throttlePercent\":"
  );

  client.print(
    throttlePercent
  );


  client.print(",");


  client.print(
    "\"clutchRaw\":"
  );

  client.print(
    clutchRaw
  );


  client.print(",");


  client.print(
    "\"clutchState\":"
  );

  client.print(
    clutchState
      ? "true"
      : "false"
  );


  client.print(",");


  client.print(
    "\"clutchThreshold\":"
  );

  client.print(
    clutchThreshold
  );


  client.print("}");
}


// =====================================================
// OK RESPONSE
// =====================================================

void sendOK(
  WiFiClient &client
) {

  client.println(
    "HTTP/1.1 200 OK"
  );

  client.println(
    "Content-Type: text/plain"
  );

  client.println(
    "Cache-Control: no-store"
  );

  client.println(
    "Connection: close"
  );

  client.println();

  client.println("OK");
}