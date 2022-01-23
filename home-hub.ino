#include <SPI.h>
#include <WiFiNINA.h>
#include "Adafruit_GFX.h"
#include "Adafruit_HX8357.h"
#include "TouchScreen.h"
#include <Fonts/FreeSans12pt7b.h>
#include "icon_bitmaps.h"

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

// Size of the color selection boxes and the paintbrush size
#define BOXSIZE 40

// Var used to control state switch
uint8_t screen = 1;
uint8_t oldscreen = 1;

// Setup canvas for later use
//GFXcanvas1 canvas(128, 32); // 128x32 pixel canvas

// Define colors
uint16_t RUSH = 0xDD40;
uint16_t NIGHTSKY = 0x2124;
#define MOONDUST 0xF77D

char ssid[] = "2.4 Aspire-UC126";        // your network SSID (name)
char pass[] = "CXNK00597407";    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key index number (needed only for WEP)

int status = WL_IDLE_STATUS;
WiFiServer server(80);
WiFiClient client;

const char hueHubIP[] = "192.168.10.111";  // Hue hub IP
const char hueUsername[] = "iFUcKu7nYaPZcP7VdcEQzIZwfqxgF8rhercudysT";  // Hue username
const int hueHubPort = 80;
bool wifiCon = false;

//  Hue variables
boolean hueOn;  // on/off
int hueBri;  // brightness value
long hueHue;  // hue value
String command;  // Hue command
boolean success = 0;
char buttonPress = 'Z';
uint16_t *colorBG;


void setup() {
  Serial.begin(9600);
  Serial.println("I'm alive and ready to contol your home (+ life)!"); 

  tft.begin();
  tft.setFont(&FreeSans12pt7b);
  tft.setRotation(1);

  // read diagnostics (optional but can help debug problems)
  uint8_t x = tft.readcommand8(HX8357_RDPOWMODE);
  Serial.print("Display Power Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(HX8357_RDMADCTL);
  Serial.print("MADCTL Mode: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(HX8357_RDCOLMOD);
  Serial.print("Pixel Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(HX8357_RDDIM);
  Serial.print("Image Format: 0x"); Serial.println(x, HEX);
  x = tft.readcommand8(HX8357_RDDSDR);
  Serial.print("Self Diagnostic: 0x"); Serial.println(x, HEX); 
  
  Serial.println(F("Benchmark                Time (microseconds)"));

  tft.setRotation(1);

  tft.fillScreen(NIGHTSKY);
  tft.drawBitmap(196, 96, bitmap_wifibad, 128, 128, RUSH);
  tft.setCursor(202, 280);
  tft.setTextColor(MOONDUST);
  tft.println("Version 0.9");
  
  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

 // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to Network named: ");
    Serial.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  printWifiStatus();                        // you're connected now, so print out the status

  Serial.println(F("Done!"));

  // make the color selection boxes
  tft.fillRect(0, 0, 240, 320, RUSH);
  tft.fillRect(240, 0, 240, 320, NIGHTSKY);
  tft.drawBitmap(56, 96, bitmap_scenes, 128, 128, NIGHTSKY);
  tft.drawBitmap(296, 96, bitmap_devices, 128, 128, RUSH);
  screen = 1;
}


void loop(void) {
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

//  Serial.print("X = "); Serial.print(p.x);
//  Serial.print("\tY = "); Serial.print(p.y);
//  Serial.print("\tPressure = "); Serial.println(p.z);  
//  delay(200);


  switch(screen) {
    case 3:
      if (oldscreen != screen) {
        oldscreen = screen;
        tft.fillScreen(NIGHTSKY);
        tft.drawBitmap(141, 71, bitmap_cactus, 64, 64, RUSH);
        tft.drawBitmap(141, 196, bitmap_flag, 64, 64, RUSH);
        tft.drawBitmap(275, 71, bitmap_lamp, 64, 64, RUSH);
        tft.drawBitmap(275, 196, bitmap_bed, 64, 64, RUSH);
      }

      if (inRange(141, 205, p.y)) {
        if (inRange(61, 124, p.x)) {
          buttonPress = 'F'; // flag
          }
        else if (inRange(184, 248, p.x)) {
          buttonPress = 'C'; // cactus
          }
      } else if (inRange(275, 339, p.y)) {
        if (inRange(61, 124, p.x)) {
          buttonPress = 'B'; // bed
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 'S'; // ceiling ('sky')
        }
      }
      
      break;
    case 2:
      if (oldscreen != screen) {
        oldscreen = screen;
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
          buttonPress = 'P'; // music scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 'A'; // airplane scene
        }
      } else if (inRange(208, 272, p.y)) {
        if (inRange(60, 124, p.x)) {
          buttonPress = 'M'; // coffee (mug) scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 'T'; // moon (twillight) scene
        }
      } else if (inRange(344, 408, p.y)) {
        if (inRange(60, 124, p.x)) {
          buttonPress = 'D'; // stars scene
        } else if (inRange(184, 248, p.x)) {
          buttonPress = 'K'; // shrimp scene
        }
      }

      break;
    case 1:
      // Draw Screen 1
      if (oldscreen != screen) {
        oldscreen = screen;
        tft.fillRect(0, 0, 240, 320, RUSH);
        tft.fillRect(240, 0, 240, 320, NIGHTSKY);
        tft.drawBitmap(56, 96, bitmap_scenes, 128, 128, NIGHTSKY);
        tft.drawBitmap(296, 96, bitmap_devices, 128, 128, RUSH);
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

  if (p.x > 270 && p.y < 50) {
        screen = 1;
      }

  // draw home button
  tft.drawBitmap(10, 10, bitmap_home, 24, 24, MOONDUST);
  buttonPress = iPressedAButton(buttonPress);

  if (WiFi.status() == WL_CONNECTED & wifiCon == false) {
    tft.drawBitmap(446, 10, bitmap_wifi, 24, 24, MOONDUST);
    wifiCon = true;
  } else if (wifiCon = true) {
//    if (screen == 1 || screen == 2) {
//      colorBG = &NIGHTSKY;
//    } else {
//      colorBG = &RUSH;
//    }
//    tft.fillRect(445, 470, 9, 35, colorBG);

    tft.fillScreen(NIGHTSKY);
    tft.drawBitmap(196, 96, bitmap_wifibad, 128, 128, RUSH);
    wifiCon = false;
  }

}

















boolean setHue(int lightNum, String command, uint16_t *colorBG)
{
  uint8_t i = 0;
  if (client.connect(hueHubIP, hueHubPort))
  {
    while (client.connected() & i < 50)
    {
      tft.drawBitmap(412, 10, bitmap_load, 24, 24, MOONDUST);
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
      i += 1;
    }
    client.stop();
    tft.fillRect(411, 9, 26, 26, *colorBG);
    return true;  // command executed
  }
  else
    return false;  // command failed
}

boolean getHue(uint8_t lightNum, uint16_t *colorBG){
  Serial.print("I made it this far...\n");
  uint8_t i = 0;
  String message = "";
  if (client.connect(hueHubIP, hueHubPort))
  {
    while (client.connected() & i < 50) {
      tft.drawBitmap(412, 10, bitmap_load, 24, 24, MOONDUST);
      client.print("GET /api/");
      client.print(hueUsername);
      client.print("/lights/");
      client.print(String(lightNum));
      client.println();
      i += 1;
    }
  
    while(client.available()) {
      char c = client.read();
      message += c;
    }
    client.stop();
    tft.fillRect(411, 9, 26, 26, *colorBG);
    if (message[15] == 't') {
      return true;
    } else {
      return false;
    }
  }
  else {
    return false;
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

const char* iPressedAButton(char buttonPress) {
  uint8_t lightNum = 0;

  switch (buttonPress) {
    case 'A': // airplane scene
      command = "{\"on\": true,\"hue\": 63175,\"sat\":241,\"bri\":96,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 60332,\"sat\":193,\"bri\":64,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 59762,\"sat\":125,\"bri\":162,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
    case 'B': // bed light
      lightNum = 6;
      colorBG = &NIGHTSKY;
      break;
    case 'C': // cactus light
      lightNum = 1;
      colorBG = &NIGHTSKY;
      break;
    case 'D': //stars scene
      command = "{\"on\": true,\"hue\": 46691,\"sat\":247,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 63834,\"sat\":82,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 45470,\"sat\":225,\"bri\":51,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
    case 'F': //flag light
      lightNum = 4;
      colorBG = &NIGHTSKY;
      break;
    case 'K': // shrimp scene
      command = "{\"on\": true,\"hue\": 43433,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 47090,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 46329,\"sat\":254,\"bri\":135,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
    case 'M': // coffee scene
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
    case 'P': // music scene
      command = "{\"on\": true,\"hue\": 50994,\"sat\":254,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 43289,\"sat\":254,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 47779,\"sat\":253,\"bri\":119,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
    case 'S': //ceiling light
      lightNum = 1;
      colorBG = &NIGHTSKY;
      break;
    case 'T':
      command = "{\"on\": true,\"hue\": 49424,\"sat\":252,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(1,command, &RUSH);
      command = "{\"on\": true,\"hue\": 35575,\"sat\":220,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(4,command, &RUSH);
      command = "{\"on\": true,\"hue\": 61955,\"sat\":254,\"bri\":157,\"transitiontime\":"+String(random(15,25))+"}";
      setHue(6,command, &RUSH);
      break;
  }

  // if button is from devices pane, determine if light is on/off and change it
  if (lightNum != 0) {
    if (getHue(lightNum, colorBG) == true) {
      command = "{\"on\": false}";
      setHue(lightNum, command, colorBG);
    } else {
      command = "{\"on\": true}";
      setHue(lightNum,command, colorBG);
    }
  }

  
//  command = "{\"on\": true,\"hue\": 8410,\"sat\":140,\"bri\":254,\"transitiontime\":"+String(random(15,25))+"}";
//  setHue(1,command);
  
  return 'Z';
}
