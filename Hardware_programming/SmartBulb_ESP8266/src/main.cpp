#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <PubSubClient.h>

const char* mqtt_server = "20.198.48.206"; 
WiFiClient espClient;
PubSubClient client(espClient);

ESP8266WebServer server(80);
String deviceID = "LIGHT_" + String(ESP.getChipId());
String topic = "lights/" + deviceID;

// HTML template for displaying available networks
String htmlPageStart = "<html>\
  <body>\
  <h2>Select Wi-Fi Network</h2>\
  <form action='/setup'>\
    SSID: <select name='ssid'>";

// HTML template ending (used after the Wi-Fi network list)
String htmlPageEnd = "</select><br>\
  Password: <input type='password' name='pass'><br>\
  <input type='submit' value='Connect'>\
  </form>\
  </body>\
</html>";

// Save Wi-Fi credentials to EEPROM
void saveCredentials(String ssid, String pass) {
  for (int i = 0; i < ssid.length(); i++) {
    EEPROM.write(i, ssid[i]);
  }
  EEPROM.write(ssid.length(), '\0');  // Null-terminate SSID

  for (int i = 0; i < pass.length(); i++) {
    EEPROM.write(100 + i, pass[i]);
  }
  EEPROM.write(100 + pass.length(), '\0');  // Null-terminate password
  EEPROM.commit();
}

// Connect to the Wi-Fi network using the stored credentials
void connectToWiFi(String ssid, String pass) {
  WiFi.begin(ssid.c_str(), pass.c_str());

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected to Wi-Fi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
  }
}

void getCredentials(){
  String ssid, pass;

  for (int i = 0; i < 32; i++)
  {
    if(EEPROM.read(i) == '\0') break;
    ssid += char(EEPROM.read(i));
    // Serial.println(ssid);
  }
  
  for (int i = 100; i < 132; i++)
  {
    if(EEPROM.read(i) == '\0') break;
    pass += char(EEPROM.read(i));
    // Serial.println(pass);
  }

  connectToWiFi(ssid, pass);
}

void deviceConfig(){
  // Start the ESP in AP mode
  WiFi.softAP("Smart Bulb");

  // Print the IP address where the user can connect
  Serial.println("Access Point created. Connect to 'ESP_AP' and go to 192.168.4.1");

  // Define the route to serve the available networks page
  server.on("/", []() {
    String wifiListHtml = htmlPageStart;

    // Scan for available Wi-Fi networks
    int n = WiFi.scanNetworks();
    Serial.println("Scan completed");
    if (n == 0) {
      wifiListHtml += "<option>No networks found</option>";
    } else {
      for (int i = 0; i < n; i++) {
        wifiListHtml += "<option value='" + WiFi.SSID(i) + "'>" + WiFi.SSID(i) + "</option>";
      }
    }
    wifiListHtml += htmlPageEnd;

    // Serve the page with the Wi-Fi list
    server.send(200, "text/html", wifiListHtml);
  });

  // Handle the form submission to set up the Wi-Fi
  server.on("/setup", []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");

    if (ssid != "") {
      Serial.println("Received SSID and password:");
      Serial.println("SSID: " + ssid);
      Serial.println("Password: " + pass);

      // Save credentials in EEPROM
      saveCredentials(ssid, pass);

      // Send a response back to the client
      server.send(200, "text/html", "Credentials received! Connecting to Wi-Fi...");

      WiFi.softAPdisconnect();  // Disable AP mode

      // Connect to the Wi-Fi network with the provided SSID and password
      connectToWiFi(ssid, pass);
    }
  });
  server.begin();
}

// MQTT Callback function
void callback(char* topics, byte* payload, unsigned int length) {
  if (String(topics) == topic) {
    if (payload[0] == '1') {
      digitalWrite(LED_BUILTIN, LOW);
    } else if(payload[0] == '0') {
      digitalWrite(LED_BUILTIN, HIGH);
    } else if(payload[0] == 64){
      deviceConfig();
    }
  }
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  pinMode(LED_BUILTIN, OUTPUT);

  getCredentials();  //connect device with WiFi
  Serial.println(deviceID);
  Serial.println(topic);

  // Configure MQTT Server
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);   
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      client.subscribe(topic.c_str());  // Subscribe to the control topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

void loop() {
  String serialData;
  server.handleClient();

  if(Serial.available()){
    serialData = Serial.readStringUntil('\n');
    serialData.trim();
  }

  if(serialData == "setup"){
    deviceConfig();
  }
  
  client.loop();
  if (!client.connected()) {
    reconnect();
  }
}

