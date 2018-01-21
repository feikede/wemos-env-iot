/*
 * wemos d1 mini read sensors and store it in countmatic
 * by Rainer Feike, 2017, MIT Lic
 * 
 * This little program for arduino ide reads sensor values from a DHT and a photoresistor (at A0) and
 * stores them via a wifi ap in a countmatic counter.
 * With a wemos d1 mini and a samsung 18650-26H power supply, it runs for about 10 days.
 * 
*/
#include <DHT.h>
#include <Wire.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>

#define DHTPIN D2       // Pin for DHT sensor input     
#define DHTTYPE DHT22   // DHT sensor type
#define POWER_PIN D6    // Pin for power suply
#define LIGHT_PIN A0    // analog in for light level

DHT dht(DHTPIN, DHTTYPE);   // initialize DHT lib

// countmatic setup for that project:
//
// create your token with https://api.countmatic.io/v2/counter/new?name=light
// then with the returned new rw-token add two counters for humidity and actualTemperature like
// https://api.countmatic.io/v2/counter/add?name=actualTemperature&token=xxxxxx-xxxx-xxxx-xxxx-xxxxxxxx-rw
// https://api.countmatic.io/v2/counter/add?name=humidity&token=xxxxxx-xxxx-xxxx-xxxx-xxxxxxxx-rw
//
// and you may create a read-only token to share with your friends like
// https://api.countmatic.io/v2/counter/readonly?token=xxxxxx-xxxx-xxxx-xxxx-xxxxxxxx-rw
// 
// you can invite your friends to visit your measured data with this link:
// https://countmatic.io/online-reader?token=xxxxxx-xxxx-xxxx-xxxx-xxxxxxxx-ro

const char* host = "api.countmatic.io";   // API host
const int httpsPort = 443;                // API port (tls)
const char* rwCountmaticToken = "xxxxxxxx-df98-473a-820d-1c492a5f4d69-rw";   // get your own token here
// rem: my readonly-token: "6e4ed640-036d-4714-b24a-ead2b8f89e27-ro"
const char* certFP = "A3 AD DD 55 0C F0 F2 1A 44 94 D0 1E 7D F3 A6 DA 00 F7 38 BD";   // Cert signature
const int sleepTimeS = 300;   // Deep sleep time in seconds


// Connect to wifi ap
int connect() {
  char ssid[] = "xxx"; //SSID of your Wi-Fi router
  char pass[] = "xxx"; //Password of your Wi-Fi router  
  // Connect to Wi-Fi network
  Serial.println();
  Serial.println();
  Serial.print("Connecting to...");
  Serial.println(ssid);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  int retry = 60;  // number of retries
  while ((--retry > 0) && (WiFi.status() != WL_CONNECTED)) {
    delay(500);  // wait before retrying
    Serial.print(".");
  }
  Serial.println("");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wi-Fi connected successfully");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return 1;
  } else {
    Serial.println("Wi-Fi connection failed");
  }
  return 0;
}

// disconnect wifi to save energy
void disconnect() {
  Serial.println("disconnecting...");
  WiFi.disconnect(true);
}

// send measured data to countmatic
void sendValues(float t, float h, float light) {
  char url[512];
  sprintf(url, "/v2/counter/reset?token=%s&name=humidity&initialvalue=%d", rwCountmaticToken, (int)(h*100));
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  // verify cert
  if (client.verify(certFP, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");  // but don't care for now
  }

  Serial.print("requesting URL: ");
  Serial.println(url);

  // send as simple GET request to countmatic
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    Serial.println(client.readStringUntil('\n')); // empty buffers - may not be necessary
  }

  if (!client.connect(host, httpsPort)) {
    Serial.println("re-connection failed");
    return;
  }

  sprintf(url, "/v2/counter/reset?token=%s&name=actualTemperature&initialvalue=%d", rwCountmaticToken, (int)(t*100));
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  
  while (client.connected()) {
    Serial.println(client.readStringUntil('\n')); // empty buffers
  }

  if (!client.connect(host, httpsPort)) {
    Serial.println("re-connection failed");
    return;
  }

  sprintf(url, "/v2/counter/reset?token=%s&name=light&initialvalue=%d", rwCountmaticToken, (int)(light*100));
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
}

void setup() {
  Serial.begin(9600);             // init serial monitor
  pinMode(POWER_PIN, OUTPUT);     // set output mode on power pin 
  digitalWrite(POWER_PIN, HIGH);  // power on all engines
  dht.begin();                    // start DHT sensor
  delay(1000);                    // delay a second for slow DHT measures
}

//
void loop() {
  
  float actualHumidity = dht.readHumidity();        // humidity
  float actualTemperatur = dht.readTemperature();   // temperature
  float lightLevel = analogRead(LIGHT_PIN);         // and light
  char str[80];
  
  sprintf(str, "T: %d.%02d H: %d.%02d\n", (int)actualTemperatur, (int)(actualTemperatur*100)%100, (int)actualHumidity, (int)(actualHumidity*100)%100);
  Serial.print(str);
  Serial.print(lightLevel);
  
  if ((connect() != 0)&&(!isnan(actualHumidity))&&(!isnan(actualTemperatur))) {
    sendValues(actualTemperatur, actualHumidity, lightLevel);
    disconnect();
  }
  // power off before deep sleep of wemos d1 mini
  digitalWrite(POWER_PIN, LOW);
  
  ESP.deepSleep(sleepTimeS*1000000, WAKE_RF_DEFAULT); // deep sleep finally
  delay(100); // that's very cabalistic

}
