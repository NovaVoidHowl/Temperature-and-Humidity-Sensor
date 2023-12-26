// NVH's Temperature and Humidity Sensor
// Author: NVH
// 
// Notes:
// - This is a simple temperature and humidity sensor that uses a DHT22 sensor and a Heltec Wifi Kit 32
// - to update boot logo use GNU Image Manipulation Program with xbm type export, then grab the raw hex from that (open in vscode)

#include "heltec.h"
#include "WiFi.h"
#include "bootlogo.h"
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include "ESPAsyncWebServer.h" // https://github.com/me-no-dev/ESPAsyncWebServer
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include "time.h"

// App constants
const char* VERSION = "0.5.1"; 
const char* APPNAME = "NVH_TEMP/HUM";

// NTP settings
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

#define DHTPIN 40     // Digital pin connected to the DHT sensor -- GPIO40 on diagram

// Uncomment the type of sensor in use:
//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

// Initialise DHT sensor.
DHT dht(DHTPIN, DHTTYPE);

// Initialise AsyncWebServer object on port 80
AsyncWebServer server(80);

const char index_json[] PROGMEM = R"rawliteral(
{"App":"%APPNAME%","Version":"%VERSION%","Description":"NVH Temperature and Humidity Sensor","SystemTime":"%TIME%","Author":"NVH","Website":"https://novavoidhowl.uk/", "Endpoints": ["/temperature", "/humidity"]}
)rawliteral";

const char temp_json[] PROGMEM = R"rawliteral(
{"App":"%APPNAME%","Version":"%VERSION%","Description":"Temperature Endpoint","Value":"%TEMPERATURE%"}
)rawliteral";

const char hum_json[] PROGMEM = R"rawliteral(
{"App":"%APPNAME%","Version":"%VERSION%","Description":"Humidity Endpoint","Value":"%HUMIDITY%"}
)rawliteral";

String generateRandomString(int length) {
	String characters = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	String randomString;

	for (int i = 0; i < length; i++) {
		int randomIndex = random(0, characters.length()); // generate a random index
		randomString += characters[randomIndex]; // append the random character to the string
	}

	return randomString;
}

String readDHTTemperature()
{
	float t = dht.readTemperature();
	if (isnan(t)) {
		Serial.println("Failed to read from DHT sensor!");
		return "Failed to read from DHT sensor!";
	}
	else
	{
		return String(t);
	}
}

String readDHTHumidity()
{
	float h = dht.readHumidity();
	if (isnan(h)) {
		Serial.println("Failed to read from DHT sensor!");
		return "Failed to read from DHT sensor!";
	}
	else
	{
		return String(h);
	}
}

String getDateTime()
{
	struct tm timeInfo;
	if(!getLocalTime(&timeInfo)){
		Serial.println("Failed to obtain time");
		return String("Failed to obtain time");
	}
	else
	{
		String timeStr = String(asctime(&timeInfo));
		timeStr.trim();
		return timeStr;
	}
}

// Replaces placeholders in HTML with actual values
String processor(const String& var){
  //Serial.println(var);
	if(var == "VERSION"){
		return VERSION;
	}
	if (var == "APPNAME"){
		return APPNAME;
	}
	if (var == "TIME"){
		return getDateTime();
	}

  if(var == "TEMPERATURE"){
    return readDHTTemperature();
  }
  if(var == "HUMIDITY"){
    return readDHTHumidity();
  }
  return String();
}

void logo(){
	Heltec.display -> clear();
	Heltec.display -> drawXbm(0,5,bootlogo_width,bootlogo_height,(const unsigned char *)bootlogo_data);
	Heltec.display -> display();
}

void wifiStatusToDisplay()
{
	if(WiFi.status() == WL_CONNECTED)
	{

		Heltec.display->drawString(0, 0, "WiFi - IP: ");
		Heltec.display->drawString(60, 0, WiFi.localIP().toString());
		Heltec.display -> display();
		//delay(500);
	}
	else
	{
		Heltec.display -> drawString(0, 0, "WiFi Not Connected");
		Heltec.display -> display();
		//while(1);
	}
}

void bootBar(int startupTask, int startupTaskCount)
{
  Heltec.display->drawProgressBar(0, 50, 120, 10, 100 - (100/startupTaskCount * (startupTaskCount - startupTask)));
	Heltec.display->display();
}

void setup()
{

	int startupTaskCount = 3;
	int startupTask = 0;
  
	pinMode(LED,OUTPUT);
	digitalWrite(LED,HIGH);

	Heltec.begin(true /*DisplayEnable Enable*/, false /*LoRa Enable*/, true /*Serial Enable*/);

	logo();
	delay(600);
	Heltec.display->clear();
	Heltec.display->drawString(0, 0, String(APPNAME) + " Booting");
	Heltec.display->drawString(0, 10, "Version: ");
	Heltec.display->drawString(60, 10, String(VERSION));
	Heltec.display->display();
	bootBar(startupTask, startupTaskCount);
  delay(2000);
  

	// setup DHT
	dht.begin();
	startupTask = 1;
  bootBar(startupTask, startupTaskCount);
	delay(500);

	WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
	// it is a good practice to make sure your code sets wifi mode how you want it.
  startupTask = 2;
  bootBar(startupTask, startupTaskCount);
	delay(500);

	//WiFiManager, Local initialisation. Once its business is done, there is no need to keep it around
	WiFiManager wm;
	startupTask = 3;
  bootBar(startupTask, startupTaskCount);
	delay(500);
  
	// reset settings - wipe stored credentials
	// these are stored by the esp library
	// if GPIO0 is held low during boot, clear the stored settings
	int resetCountdown = 10;
	if (digitalRead(0) == LOW)	
	{
		// while GPIO0 stays low carry on with countdown
		while (digitalRead(0) == LOW && resetCountdown > 0)
		{
			Heltec.display->clear();
			Heltec.display->drawString(0, 0, "Resetting WiFi");
			Heltec.display->drawString(0, 10, "Configuration");
			Heltec.display->drawString(0, 20, "in " + String(resetCountdown));
			Heltec.display->drawString(0, 30, "hold button to continue");
			// draw progress bar
			Heltec.display->drawProgressBar(0, 50, 120, 10, 100 - (resetCountdown * 10));
			Heltec.display->display();
			delay(1000);
			resetCountdown--;
		}

		// if countdown has finished, reset settings
		if (resetCountdown == 0)
		{
			Heltec.display->clear();
			Heltec.display->drawString(0, 0, "Resetting WiFi");
			Heltec.display->drawString(0, 10, "Configuration");
			Heltec.display->drawString(0, 20, "Resetting...");
			Heltec.display->display();
			delay(1000);
			wm.resetSettings();
		}
	}


	// create random string for setup password, and print to screen
	String randomCode = generateRandomString(8);
	String randomAP = "NVH_T/H_Setup_" + generateRandomString(4);

	Heltec.display->clear();
	Heltec.display->drawString(0, 0, "WiFi Setup");
	Heltec.display->drawString(0, 10, "Password: ");
	Heltec.display->drawString(60, 10, randomCode);
	Heltec.display->drawString(0, 20, "SSID: ");
	Heltec.display->drawString(0, 30, randomAP);
	Heltec.display->display();



	// Automatically connect using saved credentials,
  // if connection fails, it starts an access point with random name suffix and random password
	// then goes into a blocking loop awaiting configuration and will return success result
  
	bool res;
	res = wm.autoConnect(randomAP.c_str(), randomCode.c_str()); // password protected ap

	if(!res) {
			Serial.println("Failed to connect");
			Heltec.display->drawString(0, 40, "WiFi Not Connected");
			Heltec.display->display();
			// ESP.restart();
	} 
	else {
			//if you get here you have connected to the WiFi    
			Serial.println("connected...yeey :)");
			Heltec.display->drawString(0, 40, "WiFi - IP: ");
			Heltec.display->drawString(60, 40, WiFi.localIP().toString());
			Heltec.display->display();
	}

	// Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/json", index_json, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/json", temp_json, processor);
  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "application/json", hum_json, processor);
  });
  // Start server
  server.begin();

	// Connect to NTP server
	configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

	// turn the led off, its very bright
	digitalWrite(LED,LOW);
  
}

void loop()
{
  // screen stuff
	Heltec.display -> clear();

	wifiStatusToDisplay();
	// if WiFi is not connected, try to reconnect
	if(WiFi.status() != WL_CONNECTED)
	{
		Heltec.display -> drawString(0, 20, "Trying to reconnect..");
		WiFi.reconnect();
		delay(1000);
	}

	//Show temperature and humidity
	Heltec.display -> drawString(0, 25, "Temp: ");
	Heltec.display -> drawString(60, 25, readDHTTemperature());
	Heltec.display -> drawString(0, 35, "Hum: ");
	Heltec.display -> drawString(60, 35, readDHTHumidity());

	//Show time in bottom left
	Heltec.display -> drawString(0, 50, getDateTime());

	// write to display
	Heltec.display -> display();
	delay(2000);
}

