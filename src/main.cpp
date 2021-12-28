/*************************************************************
Oliver  - 20211008

ESP8266 - Temperature Controller

takes temperature with NTC sensor and switches a relay.
Shows temperature in C and F and relay state

Ver.2110131800
************************************************************/

// - REQUIRED LIBRARIES--------------------------------------------------------
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <webpage.h> // contains the HTML code for our webpage
#include "thermistor.h"
#include "ADS1X15.h"
ADS1115 ADS(0x48); // ADS1115 physiquement défini à l'adresse 0x48

// - DEBUG --------------------------------------------------------------------
#define DEBUG // uncomment this for DEBUG output

// - CONSTANTS ----------------------------------------------------------------
#define HighTemp 19 // 18
#define LowTemp 17  // 17

const char *optTemp = "C"; // C = *Celsius (°C), F = Fahrenheit (°F)

#define compMinRun 120     // 120sec/2 min
#define compMaxRun 3600    // 3600sec/60 min
#define compMinOffTime 600 // 600sec/10 min

// Replace with your wifi credentials
const char *ssid = "xxxxxxxxxxxx";
const char *password = "xxxxxxxxxx";

// - PINS ---------------------------------------------------------------------
#ifndef DEBUG
#define BUILTIN_LED 5
#else
#define BUILTIN_LED 2
#endif
#define RELAYPIN_1 14 // D5 //GPIO14
#define RELAYPIN_2 16 // D0 //GPIO16
#define RELAYPIN_3 12 // D6 //GPIO12
#define RELAYPIN_4 13 // D7 //GPIO13

// - Variables ----------------------------------------------------------------
Thermistor *thermistor;
float currentTemp;

int16_t tension_A0; // ADS1X15 read A0

// compessor & temperature values
int restTime = 0;
int coolTime = 0;
boolean compessorRun = false;

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 1000; // update frequency in ms

extern const char index_html[];

String RelayStateW = "OFF";

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

void (*resetFunc)(void) = 0; // declare reset fuction at address 0 ------------

void setRelayState(int relayName = RELAYPIN_3, uint8_t relayState = LOW) // --
{
  digitalWrite(relayName, relayState);

  if (relayState == HIGH)
  {
    digitalWrite(BUILTIN_LED, LOW);
    RelayStateW = "ON";
#ifdef DEBUG
    Serial.println("Relay ON");
#endif
  }
  else
  {
    digitalWrite(BUILTIN_LED, HIGH);
    RelayStateW = "OFF";
#ifdef DEBUG
    Serial.println("Relay OFF");
#endif
  }
  delay(1000);
}

void readTemperature() // -----------------------------------------------------
{
  if (optTemp == "C")
  {
    double tempC = thermistor->readTempC();
    currentTemp = tempC;
  }
  if (optTemp == "F")
  {
    double tempF = thermistor->readTempF();
    currentTemp = tempF;
  }

#ifdef DEBUG
  Serial.print("Temperature: ");
  Serial.println(String(currentTemp) + " " + String(optTemp));
#endif
}

// Replaces placeholder with DS18B20 values
String processor(const String &var) // ----------------------------------------
{
  if (var == "TEMPERATUREW")
  {
    return String(currentTemp);
  }
  if (var == "OPTTEMP")
  {
    return optTemp;
  }
  if (var == "DEVICENAME")
  {
    return "CoolBOX";
  }
  if (var == "RELAYSTATE")
  {
    return RelayStateW;
  }
  return String();
}

// Connect to WiFi network
void setup_wifi() // ----------------------------------------------------------
{
#ifdef DEBUG
  Serial.print("\nConnecting to ");
  Serial.println(ssid);
#endif

  WiFi.begin(ssid, password); // Connect to network

  while (WiFi.status() != WL_CONNECTED)
  { // Wait for connection
    delay(500);
#ifdef DEBUG
    Serial.print(".");
#endif
  }
#ifdef DEBUG
  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
#endif
}

void setup() // ---------------------------------------------------------------
{
#ifdef DEBUG
  Serial.begin(115200); // Serial port for debugging purposes
  delay(1000);          // wait a bit to get Serial Port ready
  Serial.println("Serial active");
#endif
  // pinMode(RELAYPIN_1, OUTPUT);
  // pinMode(RELAYPIN_2, OUTPUT);
  pinMode(RELAYPIN_3, OUTPUT);
  // pinMode(RELAYPIN_4, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT); // Initialize the BUILTIN_LED pin as an output, it is acive low
  // digitalWrite(BUILTIN_LED, HIGH); // Turn the LED off
  setRelayState(RELAYPIN_3, LOW);

  // compessorRun = false;

  // thermistor = new Thermistor(A0, 3.3, 3.3, 1023, 10000, 10000, 25, 3950, 5, 20); // Sensor settings
  // thermistor = new Thermistor(A0, 1.65, 1.0, 1023, 10000, 10000, 25, 3950, 5, 20);
  // thermistor = new Thermistor(A0, 1.0, 1.0, 1023, 10000, 10000, 25, 3950, 5, 20);
  thermistor = new Thermistor(A0, 1.6, 1.0, 1023, 10000, 10000, 25, 3950, 20, 5);

  setup_wifi(); // Connect to network

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "text/html", index_html, processor); });
  server.on("/json", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send_P(200, "application/json; charset=UTF-8", json_response, processor); });
  server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
            { resetFunc(); });

  AsyncElegantOTA.begin(&server); // Start ElegantOTA
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  server.begin(); // Start server

  restTime = (millis() - (compMinOffTime * 1000));
  coolTime = 0;
  delay(1000);

  // ADS1X15 setup
  ADS.begin();    // create ADS instanz
  ADS.setGain(0); // ± 6.144 volt (par défaut). À noter que le gain réel ici sera de « 2/3 », et non zéro, comme le laisse croire ce paramètre !
  // ADS.setGain(1);	// ± 4.096 volt
  // ADS.setGain(2);	// ± 2.048 volt
  // ADS.setGain(4);	// ± 1.024 volt
  // ADS.setGain(8);	// ± 0.512 volt
  // ADS.setGain(16);	// ± 0.256 volt
  ADS.setMode(1);     // 0 = CONTINUOUS, 1 = SINGLE (default)
  ADS.setDataRate(7); // avec vitesse de mesure, allant de 0 à 7 (7 étant le max, soit 860 échantillons par seconde)
  ADS.readADC(0);     // Et on fait une lecture à vide, pour envoyer tous ces paramètres

} // end setup

// Run checks -----------------------------------------------------------------
boolean tooWarm() { return (currentTemp >= HighTemp); }
boolean tooCold() { return (currentTemp <= LowTemp); }
boolean restLongEnough() { return ((millis() - restTime) >= (compMinOffTime * 1000)); }
boolean onLongEnough() { return ((millis() - coolTime) >= (compMinRun * 1000)); }
boolean onTooLong() { return ((millis() - coolTime) >= (compMaxRun * 1000)); }

void loop() // ----------------------------------------------------------------
{
  while (1)
  {
    if ((millis() - lastTime) > timerDelay)
    {

      lastTime = millis();
      readTemperature();

      // ADS1X15
      // Demande de mesure de tensions à l'ADC (résultats entre -32768 et +32767)
      tension_A0 = ADS.readADC(0); // Mesure at ADS channel A0, store the reading
#ifdef DEBUG
      Serial.print("ADS A0 = "); // print the reading
      Serial.println(tension_A0);
#endif

      if (compessorRun == false)
      {
        if (tooWarm() && restLongEnough()) // switch compressor on
        {
          compessorRun = true;
          setRelayState(RELAYPIN_3, HIGH);
          coolTime = millis();
        }
      }
      else
      {
        if (onTooLong() || (onLongEnough() && tooCold())) // switch compressor off
        {
          compessorRun = false;
          setRelayState(RELAYPIN_3, LOW);
          restTime = millis();
        }
      }
    }
    // AsyncElegantOTA.loop();
    yield(); // to reset WDT of ESP8266
  }          // end while
} // end loop