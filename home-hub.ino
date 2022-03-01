#include <SPI.h>
#include <WiFiNINA.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "TouchScreen.h"
#include <Fonts/FreeSans12pt7b.h>
#include "icon_bitmaps.h"
//#include <NTPClient.h>
//#include <WiFiUdp.h>

// These are the four touchscreen analog pins
#define YP A2  // must be an analog pin, use "An" notation!
#define XM A3  // must be an analog pin, use "An" notation!
#define YM 7   // can be a digital pin
#define XP 8   // can be a digital pin

// This is calibration data for the raw touch data to the screen coordinates
#define TS_MINX 110
#define TS_MINY 80
#define TS_MAXX 900
#define TS_MAXY 940

#define MINPRESSURE 10
#define MAXPRESSURE 1000

// These are 'flexible' lines that can be changed
#define TFT_CS 10
#define TFT_DC 9
#define TFT_RST 8 // RST can be set to -1 if you tie it to Arduino's reset

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

// SoftSPI - note that on some processors this might be *faster* than hardware SPI!
//Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, MOSI, SCK, TFT_RST, MISO);

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Var used to control state switch
uint8_t screen = 1;
uint8_t oldscreen = 99;
uint8_t updateMenu = 1;
unsigned long lastTouched = 0;
uint8_t screenTouched = 0;
uint8_t beenAMin = 0;

// Define colors
uint16_t RUSH = 0xDD40;
uint16_t NIGHTSKY = 0x2124;
uint16_t MOONDUST = 0xF77D;

char ssid[] = "2.4 Aspire-UC126";        // your network SSID (name)
char pass[] = "CXNK00597407";    // your network password (use for WPA, or use as key for WEP)
//int keyIndex = 0;                 // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;
// NTP client to fetch real time
//WiFiUDP ntpUDP;
//NTPClient timeClient(ntpUDP);
unsigned long lastMillis = 0;
uint16_t intervalNTP = 60000; // Request NTP time every minute

const char hueHubIP[] = "192.168.10.111";  // Hue hub IP
const char hueUsername[] = "iFUcKxu7nYaPZcP7VdcEQzIZwfqxgF8rhercudysT";  // Hue username
const uint8_t hueHubPort = 80;
bool wifiCon = false;

//  Hue variables
boolean hueOn;  // on/off
uint8_t hueBri;  // brightness value
String command;  // Hue command
boolean success = 0;
uint16_t *colorBG;


void setup() {
  Serial.begin(9600);

  tft.begin();
  tft.setFont(&FreeSans12pt7b);
  tft.setRotation(1);

//  // read diagnostics (optional but can help debug problems)
//  uint8_t x = tft.readcommand8(HX8357_RDPOWMODE);
//  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
//  x = tft.readcommand8(HX8357_RDMADCTL);
//  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
//  x = tft.readcommand8(HX8357_RDCOLMOD);
//  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
//  x = tft.readcommand8(HX8357_RDDIM);
//  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
//  x = tft.readcommand8(HX8357_RDDSDR);
//  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 
  
  //Serial.println(F("Benchmark                Time (microseconds)"));

  //tft.setRotation(1);

  tft.fillScreen(NIGHTSKY);
  tft.drawBitmap(196, 96, bitmap_wifibad, 128, 128, RUSH);
  tft.setCursor(202, 280);
  tft.setTextColor(MOONDUST);
  tft.println("Version 0.9");
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    //Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    //Serial.println("Please upgrade the firmware");
  }

 // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    //Serial.print("Attempting to connect to Network named: ");
    //Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  printWifiStatus();                        // you're connected now, so print out the status
//  timeClient.begin();

  Serial.println(F("Done!"));

  // draw home screen
  drawHome();
  screen = 1;
}


void loop(void) {
  uint8_t buttonPress = 0;
  
  // Retrieve a point  
  TSPoint p = ts.getPoint();

  // we have some minimum pressure we consider 'valid'
  // pressure of 0 means no pressing!
  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) {
     return;
  }

  // Scale from ~0->1000 to tft.width using the calibration #'s
  p.x = map(p.x, TS_MINX, TS_MAXX, 0, tft.height());
  p.y = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());

  Serial.print("X = "); Serial.print(p.x);
  Serial.print("\tY = "); Serial.print(p.y);
  Serial.print("\tPressure = "); Serial.println(p.z);  
  delay(200);

  if (p.x > 270 && p.y < 50) {
        screen = 1;
      }

  if (screen == 2) {
    colorBG = &RUSH;
  } else {
    colorBG = &NIGHTSKY;
  }

  switch(screen) {
    case 10:
      if (oldscreen != screen) {
        oldscreen = screen;
        analogWrite(5, 0);
      }
      delay(2000);
    case 9:
      if (oldscreen != screen) {
        oldscreen = screen;
        analogWrite(5, 20);
      }
      if (p.x > 0 & p.z > 300) {
        screenTouched = 1;
      }
      break;
    case 3:
      if (oldscreen != screen) {
        screenTouched = 1;
        oldscreen = screen;
        screenChangeMenu();
        tft.fillScreen(NIGHTSKY);
        tft.drawBitmap(141, 71, bitmap_cactus, 64, 64, RUSH);
        tft.drawBitmap(141, 196, bitmap_flag, 64, 64, RUSH);
        tft.drawBitmap(275, 71, bitmap_lamp, 64, 64, RUSH);
        tft.drawBitmap(275, 196, bitmap_bed, 64, 64, RUSH);
      }

      if (inRange(141, 205, p.y)) {
        if (inRange(61, 124, p.x)) {
          buttonPress = 9; // flag
          }
        else if (inRange(184, 248, p.x)) {
          buttonPress = 7; // cactus
          }
      } else if (inRange(275, 339, p.y)) {
        if (inRange(61, 124, p.x)) {
          buttonPress = 10; // bed
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 8; // ceiling ('sky')
        }
      }
      
      break;
    case 2:
      if (oldscreen != screen) {
        screenTouched = 1;
        oldscreen = screen;
        screenChangeMenu();
        tft.fillScreen(RUSH);
        tft.drawBitmap(72, 71, bitmap_plane, 64, 64, NIGHTSKY);
        tft.drawBitmap(72, 196, bitmap_music, 64, 64, NIGHTSKY);
        tft.drawBitmap(208, 71, bitmap_moon, 64, 64, NIGHTSKY);
        tft.drawBitmap(208, 196, bitmap_mug, 64, 64, NIGHTSKY);
        tft.drawBitmap(344, 71, bitmap_shrimp, 64, 64, NIGHTSKY);
        tft.drawBitmap(344, 196, bitmap_confetti, 64, 64, NIGHTSKY);
      }

      if (inRange(72, 136, p.y)) {
        if (inRange(60, 124, p.x)) {
          buttonPress = 4; // music scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 1; // airplane scene
        }
      } else if (inRange(208, 272, p.y)) {
        if (inRange(60, 124, p.x)) {
          buttonPress = 5; // coffee (mug) scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 2; // moon (twillight) scene
        }
      } else if (inRange(344, 408, p.y)) {
        if (inRange(60, 124, p.x)) {
          buttonPress = 6; // stars scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 3; // shrimp scene
        }
      }

      break;
    case 1:
      // Draw Screen 1
      if (oldscreen != screen) {
        screenTouched = 1;
        oldscreen = screen;
        drawHome();
        screenChangeMenu();
      }
    
      if (p.x > 270 && p.y < 50) {
        screen = 1;
      } else if (p.y < 240 & p.x > 0) {
        screen = 2;
      } else if (p.y > 240 & p.x > 0) {
        screen = 3;
      }

      break;
  }

  screenTouched = iPressedAButton(buttonPress);
  //iPressedAButton(buttonPress);
  
//  // draw home button
//  tft.drawBitmap(10, 10, bitmap_home, 24, 24, MOONDUST);

  if ((WiFi.status() == WL_CONNECTED & wifiCon == false) || updateMenu == 1) {
    // draw wifi connected icon
    tft.drawBitmap(446, 10, bitmap_wifi, 24, 24, MOONDUST);
    // draw home button
    tft.drawBitmap(10, 10, bitmap_home, 24, 24, MOONDUST);
    wifiCon = true;
  } else if (WiFi.status() != WL_CONNECTED & wifiCon == true) {
    if (screen == 1 || screen == 2) {
      colorBG = &NIGHTSKY;
    } else {
      colorBG = &RUSH;
    }
    tft.fillRect(445, 470, 9, 35, colorBG);

    tft.fillScreen(NIGHTSKY);
    tft.drawBitmap(196, 96, bitmap_wifibad, 128, 128, RUSH);
    wifiCon = false;

    updateMenu = 0;
  }

  if (screenTouched == 1) {
    lastTouched = millis();
    if (screen >= 9) {
      digitalWrite(5, HIGH);
      if (screen == 10) {
        screen = 1;
      }
      delay(500);
    }
    screenTouched = 0;
    beenAMin = 0;
  }

  unsigned long currentMillis = millis();

  if (currentMillis - lastTouched > intervalNTP) { // if a minute has passed since last touch
     if (beenAMin >= 5) {
      screen = 10;
     } else {
      screen = 9;
      beenAMin += 1;
     }
     lastTouched = millis();
  }

//  if (currentMillis - lastMillis > intervalNTP || updateMenu == 1) { // If a minute has passed since last NTP request
//    lastMillis = currentMillis;
//    Serial.println("\r\nSending NTP request ...");
//    timeClient.update();
//    tft.fillRect(339, 10, 100, 26, *colorBG);
//    tft.setCursor(340, 30);
//    //tft.println(timeClient.getHours() + ":" + timeClient.getMinutes());
//    tft.println(timeClient.getFormattedTime());
//
//    updateMenu = 0;
//  }
  

}





  void screenChangeMenu(void){
    updateMenu = 1;
  }











//boolean setHue(int lightNum, String command, uint16_t *colorBG)
//{
//  uint8_t i = 0;
//  if (client.connect(hueHubIP, hueHubPort))
//  {
//    while (client.connected() & i < 50)
//    {
//      tft.drawBitmap(412, 10, bitmap_load, 24, 24, MOONDUST);
//      client.print("PUT /api/");
//      client.print(hueUsername);
//      client.print("/lights/");
//      client.print(lightNum);  // hueLight zero based, add 1
//      client.println("/state HTTP/1.1");
//      client.println("keep-alive");
//      client.print("Host: ");
//      client.println(hueHubIP);
//      client.print("Content-Length: ");
//      client.println(command.length());
//      client.println("Content-Type: text/plain;charset=UTF-8");
//      client.println();  // blank line before body
//      client.println(command);  // Hue command
//      i += 1;
//    }
//    client.stop();
//    tft.fillRect(411, 9, 26, 26, *colorBG);
//    return true;  // command executed
//  }
//  else
//    return false;  // command failed
//}
//
//boolean getHue(uint8_t lightNum, uint16_t *colorBG){
//  uint8_t i = 0;
//  String message = "";
//  if (client.connect(hueHubIP, hueHubPort))
//  {
//    while (client.connected() & i < 50) {
//      tft.drawBitmap(412, 10, bitmap_load, 24, 24, MOONDUST);
//      client.print("GET /api/");
//      client.print(hueUsername);
//      client.print("/lights/");
//      client.print(String(lightNum));
//      client.println();
//      i += 1;
//    }
//  
//    while(client.available()) {
//      char c = client.read();
//      message += c;
//    }
//    client.stop();
//    tft.fillRect(411, 9, 26, 26, *colorBG);
//    if (message[15] == 't') {
//      return true;
//    } else {
//      return false;
//    }
//  }
//  else {
//    return false;
//  }
//}



boolean controlHue(uint8_t lightNum, String command, uint16_t *colorBG, bool mode) {
  uint8_t i = 0;
  Serial.print("sucgma");
  if (client.connect(hueHubIP, hueHubPort))
  {
    Serial.print("FUCK BITCH");
    tft.drawBitmap(446, 45, bitmap_load, 24, 24, MOONDUST); // draw loading icon
    while (client.connected() & i < 10) {
      if (mode == true) { // set Hue properties
        client.print("PUT /api/");
        client.print(hueUsername);
        client.print("/lights/");
        client.print(lightNum);  // hueLight zero based, add 1
        client.println("/state HTTP/1.1");
        client.println("keep-alive");
        client.print("Host: ");
        client.println(hueHubIP);
        client.print("Content-Length: ");
        client.println(command.length());
        client.println("Content-Type: text/plain;charset=UTF-8");
        client.println();  // blank line before body
        client.println(command);  // Hue command
        
      } else { // get Hue properties
        client.print("GET /api/");
        client.print(hueUsername);
        client.print("/lights/");
        client.print(String(lightNum));
        client.println();
      }
      i += 1;
      }

    if (mode == false) {
      while(client.available()) {
        char c = client.read();
        command += c;
      }
      client.stop();
      tft.fillRect(445, 44, 26, 26, *colorBG);
    
      if (command[15] == 't') {
         return true;
      } else {
        return false;
      }
    } else {
      client.stop();
      tft.fillRect(445, 44, 26, 26, *colorBG);
      return true;
    }
    } else{
      return;
    }




  
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
  // print where to go in a browser:
  Serial.print("To see this page in action, open a browser to http://");
  Serial.println(ip);
}

bool inRange(uint16_t low, uint16_t high, int16_t x)        
{        
 return (low <= x && x <= high);         
}

const char* iPressedAButton(uint8_t buttonPress) {
  uint8_t lightNum = 0;

  switch (buttonPress) {
    case 1: // airplane scene
      command = "{\"on\": true,\"hue\": 63175,\"sat\":241,\"bri\":96,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 60332,\"sat\":193,\"bri\":64,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 59762,\"sat\":125,\"bri\":162,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
    case 10: // bed light
      lightNum = 6;
      break;
    case 7: // cactus light
      lightNum = 1;
      break;
    case 6: //stars scene
      command = "{\"on\": true,\"hue\": 46691,\"sat\":247,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 63834,\"sat\":82,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 45470,\"sat\":225,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
    case 9: //flag light
      lightNum = 4;
      break;
    case 3: // shrimp scene
      command = "{\"on\": true,\"hue\": 43433,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 47090,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 46329,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
    case 5: // coffee scene
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
    case 4: // music scene
      command = "{\"on\": true,\"hue\": 50994,\"sat\":254,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 43289,\"sat\":254,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 47779,\"sat\":253,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
    case 8: //ceiling light
      lightNum = 1;
      break;
    case 2: // moon scene
      command = "{\"on\": true,\"hue\": 49424,\"sat\":252,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(1,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 35575,\"sat\":220,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(4,command, &RUSH, true);
      command = "{\"on\": true,\"hue\": 61955,\"sat\":254,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      controlHue(6,command, &RUSH, true);
      break;
  }

  

  // if button is from devices pane, determine if light is on/off and change it
  if (lightNum != 0) {
    if (controlHue(lightNum, "", colorBG, false) == true) {
      command = "{\"on\": false}";
      controlHue(lightNum, command, colorBG, true);
    } else {
      command = "{\"on\": true}";
      controlHue(lightNum,command, colorBG, true);
    }
  }

  
//  command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
//  setHue(1,command);
  
  return 1;
}

void drawHome() {
   tft.fillRect(0, 0, 240, 320, RUSH);
   tft.fillRect(240, 0, 240, 320, NIGHTSKY);
   tft.drawBitmap(56, 96, bitmap_scenes, 128, 128, NIGHTSKY);
   tft.drawBitmap(296, 96, bitmap_devices, 128, 128, RUSH);
}
