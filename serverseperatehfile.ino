#include <Wire.h>
#include <Adafruit_INA219.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include "index_html.h"


const char* ssid = "UD Devices";

#define NUM_STRIPS 7
const int pins[NUM_STRIPS] = {17, 23, 18, 25, 15, 3, 19};
const int numPixels[NUM_STRIPS] = {23, 23, 23, 23, 23, 18, 16};

int sim=0;
int lastsim=0;
uint8_t r = 0, g = 0, b = 0;

Adafruit_NeoPixel strips[NUM_STRIPS] = {
  Adafruit_NeoPixel(numPixels[0], pins[0], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[1], pins[1], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[2], pins[2], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[3], pins[3], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[4], pins[4], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[5], pins[5], NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(numPixels[6], pins[6], NEO_GRB + NEO_KHZ800)
};

void setStripColor(int stripIndex, uint8_t r, uint8_t g, uint8_t b) {
  if (stripIndex >= 0 && stripIndex < NUM_STRIPS) {
    for (int i = 0; i < numPixels[stripIndex]; i++) {
      strips[stripIndex].setPixelColor(i, strips[stripIndex].Color(r, g, b));
    }
    strips[stripIndex].show();
  }
}

AsyncWebServer server(80);

Adafruit_INA219 sensors[7] = {
  Adafruit_INA219(0x43),
  Adafruit_INA219(0x44),
  Adafruit_INA219(0x4D),
  Adafruit_INA219(0x40),
  Adafruit_INA219(0x45),
  Adafruit_INA219(0x41),
  Adafruit_INA219(0x4A)
};

const int controlPins[7] = {13, 16, 12, 14, 2, 4, 5};

float busVoltage[7];
float shuntVoltage[7];
float current_mA[7];
float loadVoltage[7];
String powerSource[7] = {"Main", "Main", "Main", "Main", "Main", "Main", "Main"};



void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected: " + WiFi.localIP().toString());



  for (int i = 0; i < 7; i++) {
    while (!sensors[i].begin()) {
      Serial.print("Failed to find INA219 #");
      Serial.print(i + 1);
      Serial.println(" - retrying...");
      delay(1000);
    }
    sensors[i].setCalibration_32V_2A();
    pinMode(controlPins[i], OUTPUT);
    digitalWrite(controlPins[i], HIGH); // Start with HIGH = Main
  }

  // Initialize each strip
  for (int i = 0; i < NUM_STRIPS; i++) {
    strips[i].begin();
    strips[i].show(); // Turn off all pixels
  }

  Serial.println("Setting each NeoPixel strip to a different color...");

  // Example: Set each strip to a different color
  setStripColor(0, 0, 255, 0);     // Green
  setStripColor(1, 0, 0, 255);     // Blue
  setStripColor(2, 0, 255, 255);     // Cyan
  setStripColor(3, 255, 0, 0);   // Red
  setStripColor(4, 255, 255, 0);   // Yellow
  setStripColor(5, 255, 0,255);   // Magenta
  setStripColor(6, 255, 255, 255); // White


  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    String csv = "Sensor,Voltage,Current,PowerSource\n";
    for (int i = 0; i < 7; i++) {
      busVoltage[i] = sensors[i].getBusVoltage_V();
      shuntVoltage[i] = sensors[i].getShuntVoltage_mV();
      current_mA[i] = sensors[i].getCurrent_mA();
      loadVoltage[i] = busVoltage[i] + (shuntVoltage[i] / 1000.0);
      csv += String(i+1) + "," + String(loadVoltage[i], 2) + "," + String(current_mA[i], 2) + "," + powerSource[i] + "\n";
    }
    request->send(200, "text/csv", csv);
});


  server.on("/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("index", true) && request->hasParam("state", true)) {
      int index = request->getParam("index", true)->value().toInt();
      String state = request->getParam("state", true)->value();
      if (index >= 0 && index < 7 && (state == "Main" || state == "Alternate")) {
        powerSource[index] = state;
        bool alternate = (state == "Alternate");
        digitalWrite(controlPins[index], alternate ? LOW : HIGH);

        delay(50);  // Give a longer time (~50ms) for power stabilization

        // Force re-initialize the strip
        strips[index].begin();
        strips[index].clear();   // Optional: Clear to blank first
        strips[index].show();
        delay(10);               // Small pause after re-init

        // Now recolor
        if (!alternate) {
          if(sim==7){
            if(index<=2){
              setStripColor(index, 0, 55, 0); // Green for Main
            }else{
              setStripColor(index, 55, 0, 0); // Red for Alternate
            }
            
          }else{
            setStripColor(index, r, g, b);
          }
        } else {
            if(sim==7){
              if(index<=2){
                setStripColor(index, 55, 0, 0); // Red for Alternate
              }else{
                setStripColor(index, 0, 55, 0); // Green for Main
              }
            }else{
              setStripColor(index, r, g, b);
            }
        }


        request->send(200, "text/plain", "Sensor updated");
        return;
      }
    }
    request->send(400, "text/plain", "Invalid parameters");
  });

  server.on("/sim", HTTP_GET, [](AsyncWebServerRequest *request){
  if (request->hasParam("num")) {
    int simNum = request->getParam("num")->value().toInt();
    Serial.print("Running simulation #");
    Serial.println(simNum);

    

    switch (simNum) {
      case 1: sim=0; break; // Yellow
      case 2: sim=1; break; // Pink
      case 3: sim=2; break; // Cyan
      case 4: sim=3; break; // White
      case 5: sim=4; break; //  dim White
      case 6: sim=5; break; // red
      case 7: sim=6; break; // off
      case 8: sim=7; break;
      default:
        request->send(400, "text/plain", "Invalid simulation number");
        return;
    }

    

    request->send(200, "text/plain", "Simulation " + String(simNum) + " applied");
  } else {
    request->send(400, "text/plain", "Missing sim number");
  }
});



  server.begin();
}

void loop() {
  if (sim != lastsim) {

    // Reset colors or apply new sim
    switch (sim) {//finished
      case 0: // Default color test
        setStripColor(0, 0, 255, 0);     // Green
        setStripColor(1, 0, 0, 255);     // Blue
        setStripColor(2, 0, 255, 255);     // Cyan
        setStripColor(3, 255, 0, 0);   // Red
        setStripColor(4, 255, 255, 0);   // Yellow
        setStripColor(5, 255, 0,255);   // Magenta
        setStripColor(6, 255, 255, 255); // White

        break;


      case 1: { // Fault Simulation: Random Blink, Neighbor Recovery//finished
        int stripIndex = 3; // Strip to blink on (DuPont 1?)
        int totalBlinks = 5;
        
        // Pick a random pixel (make sure it's not first or last to have neighbors)
        int pixelIndex = random(1, strips[stripIndex].numPixels() - 1);

        setStripColor(stripIndex, 0, 0, 0); // Clear strip first

        // Blink random pixel 5 times
        for (int i = 0; i < totalBlinks; i++) {
          bool on = (i % 2 == 0);
          strips[stripIndex].setPixelColor(pixelIndex, on ? strips[stripIndex].Color(255, 0, 0) : 0);
          strips[stripIndex].show();
          delay(500);
        }

        // After blinking, turn neighbors green and all others off
        for (int i = 0; i < strips[stripIndex].numPixels(); i++) {
          if (i == pixelIndex - 1 || i == pixelIndex + 1) {
            strips[stripIndex].setPixelColor(i, strips[stripIndex].Color(0, 255, 0)); // Green neighbors
          } else {
            strips[stripIndex].setPixelColor(i, 0); // Off everything else
          }
        }
        strips[stripIndex].show();

        delay(10000); // Hold for 10 seconds

        // Recover: Turn everything back on EXCEPT the bad pixel
        for (int i = 0; i < strips[stripIndex].numPixels(); i++) {
          if (i == pixelIndex) {
            strips[stripIndex].setPixelColor(i, 0); // Bad pixel stays OFF
          } else {
            strips[stripIndex].setPixelColor(i, strips[stripIndex].Color(255, 0, 0)); // Normal green
          }
        }
        strips[stripIndex].show();

        break;
      }


      case 2: { // Daily Load Sim (Evans area)//finished
        float brightnessLevels[24] = {
          0, 0, 0.25, 0.50, 0.75, 0.75, 0.625, 0.625,
          0.75, 0.5, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25,
          0.375, 0.5, 0.75, 1.0, 1.0, 1.0, 0.75, 0.75
        };

        for (int t = 0; t < 24; t++) {
          int brightness = int(255 * brightnessLevels[t]);
          for (int i = 0; i < NUM_STRIPS; i++) {
            strips[i].setBrightness(brightness);
            setStripColor(i, 255, 255, 0);  // Yellow
          }
          delay(1000);
        }
        break;
      }

      case 3: { // Load Drop (DuPont area)//finished
        float brightnessLevels[24] = {
          0, 0, 0, 0, 0, 0.125, 0.25, 0.375,
          0.5, 0.625, 0.75, 0.875, 1, 0.875,
          0.75, 0.625, 0.5, 0.375, 0.25, 0.125,
          0, 0, 0, 0
        };

        for (int t = 0; t < 24; t++) {
          int brightness = int(255 * brightnessLevels[t]);
          for (int i = 2; i <= 3; i++) { // DuPont 1 and 2
            strips[i].setBrightness(brightness);
            setStripColor(i, 255, 255, 0); // Yellow
          }
          delay(1000);
        }
        break;
      }

      case 4: // Load Ramp - Dim White
        r = 64; g = 64; b = 64;
        break;

      case 5: // Solar Effect - Orangey
        r = 128; g = 70; b = 0;
        break;

      case 6: // All off
        r = 0; g = 0; b = 0;
        break;

      case 7://finished
        setStripColor(0, 0, 255, 0);     // Green
        setStripColor(1, 0, 255, 0);     // Blue
        setStripColor(2, 0, 255, 0);     // Cyan
        setStripColor(3, 255, 0, 0);   // Red
        setStripColor(4, 255, 0, 0);   // Yellow
        setStripColor(5, 255, 0,0);   // Magenta
        setStripColor(6, 255, 255, 255); // White
        break;
    }

    // Apply color change (cases 4–6)
    if (sim >= 4 && sim <= 6) {
      for (int i = 0; i < NUM_STRIPS; i++) {
        strips[i].setBrightness(255);
        setStripColor(i, r, g, b);
      }
    }

    lastsim = sim;
  }
}
