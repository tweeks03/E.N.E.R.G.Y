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

// ---- Globals for Simulation 6 (Spinning Rainbow of Death) ----
uint32_t currentColors[7];
uint32_t targetColors[7];
bool case6_initialized = false;
unsigned long case6_lastUpdate = 0;
float case6_lerpAmount = 0.0;
const int case6_order[7] = {0, 2, 6, 1, 5, 4, 3};
const int case6_updateInterval = 200;
const float case6_lerpStep = 0.05;


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
              setStripColor(index, 0, 255, 0); // Green for Main
            }else{
              setStripColor(index, 255, 0, 0); // Red for Alternate
            }
            
          }else{
            setStripColor(index, r, g, b);
          }
        } else {
            if(sim==7){
              if(index<=2){
                setStripColor(index, 255, 0, 0); // Red for Alternate
              }else{
                setStripColor(index, 0, 255, 0); // Green for Main
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
      case 9: sim=8; break;

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
    lastsim = sim;
    

    // Reset colors or apply new sim
    switch (sim) {//finished
      case 0: // Default color test
        for (int i = 0; i < NUM_STRIPS; i++) {
        strips[i].begin();
        strips[i].clear();   // Optional: Clear to blank first
        strips[i].show();
      }
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
    0, 0, 0, 0, 0, 0.125,    // 0–5 sec
    0.25, 0.375,             // 6–7 sec
    0.5,                    // 8 sec
    0.625,                  // 9 sec
    0.75, 0.875, 1, 0.875,  // 10–13 sec
    0.75, 0.625,            // 14–15 sec
    0.5,                    // 16 sec
    0.375,                  // 17 sec
    0.25,                   // 18 sec
    0.125, 0, 0             // 19–21 sec
  };

  // Turn all lines cyan at 80% brightness
  for (int i = 0; i < 7; i++) {
    setStripColor(i, 0, 255, 255);  // Cyan
    strips[i].setBrightness(204);  // 255 * 0.8 = 204
    strips[i].show();
  }

  // Animate brightness drop and yellow load indicators
  for (int t = 0; t < 24; t++) {
    float brightnessFraction = 1-brightnessLevels[t];
    int brightnessValue = int(255 * brightnessFraction);


    // Highlight DuPont area loads in yellow
  strips[1].setPixelColor(3, strips[1].Color(brightnessValue, brightnessValue, 0));  // yellow
  strips[1].setPixelColor(12, strips[1].Color(brightnessValue, brightnessValue, 0));
  strips[1].setPixelColor(18, strips[1].Color(brightnessValue, brightnessValue, 0));
  strips[1].setPixelColor(21, strips[1].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(17, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(14, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(6, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(1, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(20, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(10, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(12, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[3].setPixelColor(21, strips[3].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(17, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(22, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(21, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(20, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(19, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(18, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(4, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(11, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(0, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(7, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(10, strips[4].Color(brightnessValue, brightnessValue, 0));
  strips[4].setPixelColor(3, strips[4].Color(brightnessValue, brightnessValue, 0));

    // Show updated colors
    for (int i = 0; i < 5; i++) {
      strips[i].show();
    }

    delay(1000); // 1 second per step
  }

  break;
}

      case 4: {
  const int flickerDuration = 10 * 1000; // 25 seconds
  const int flickerInterval = 10;       // Check every 100ms
  const int flickerChance = 10;          // % chance to flicker per pixel per interval

  unsigned long startTime = millis();

  // Set all houses to full brightness and white color
  for (int i = 0; i < 7; i++) {
    strips[i].setBrightness(255);
    setStripColor(i, 255, 255, 255); // full white
    strips[i].show();
  }

  while (millis() - startTime < flickerDuration) {
    for (int stripIndex = 0; stripIndex < 7; stripIndex++) {
      for (int pixelIndex = 0; pixelIndex < strips[stripIndex].numPixels(); pixelIndex++) {
        int randVal = random(100);
        if (randVal < flickerChance) {
          // Flicker: dim or off
          int flickerType = random(3); // 0 = off, 1 = dim, 2 = keep bright
          if (flickerType == 0) {
            strips[stripIndex].setPixelColor(pixelIndex, 0); // Off
          } else if (flickerType == 1) {
            strips[stripIndex].setPixelColor(pixelIndex, strips[stripIndex].Color(128, 128, 128)); // Dim gray
          } else {
            strips[stripIndex].setPixelColor(pixelIndex, strips[stripIndex].Color(255, 255, 255)); // Full
          }
        } else {
          // Keep full brightness
          strips[stripIndex].setPixelColor(pixelIndex, strips[stripIndex].Color(255, 255, 255));
        }
      }
      strips[stripIndex].show();
    }
    delay(flickerInterval);
  }

  break;
}


      case 5: {
         float brightnessLevels[14] = {
  1, 1, 1, .875,               // 10–13 sec
  0.75, 0.625,                           // 14–15 sec
  0.5,                                // 16 sec
  0.375,                                  // 17 sec
  0.25,                                 // 18 sec
  0.125, 0, 0,                        // 19–21 sec
  0                                  // 22 sec
};

for(int i = 0; i < 8; i++){ //set all lines
  setStripColor(i, 200, 200, 200);  // cyan
  strips[i].show();
 }

delay(3000);

for(int i = 0; i < 8; i++){ //set all lines
  setStripColor(i, 0, 0, 0); 
  strips[i].show();
 }

delay(3000);


for (int t = 0; t < 14; t++) {
  float brightnessFraction = brightnessLevels[t];
  int brightnessValue = int(255 * brightnessFraction); // Convert to 0–255 scale

for(int i = 0; i < 8; i++){ //set all lines
  setStripColor(i, 255, 255, 255);  // cyan
  strips[i].setBrightness(brightnessValue);
  strips[i].show();
 }




delay(500); // Wait 1 second
}
break;
}

case 6: { 
  case6_initialized = false;  // Reset if sim changes
  break;
}


      case 7://finished
        setStripColor(0, 0, 255, 0);     // Green
        setStripColor(1, 0, 255, 0);     // Blue
        setStripColor(2, 0, 255, 0);     // Cyan
        setStripColor(3, 255, 0, 0);   // Red
        setStripColor(4, 255, 0, 0);   // Yellow
        setStripColor(5, 255, 0,0);   // Magenta
        setStripColor(6, 255, 255, 255); // White
        break;

      case 8:
      r = 0; g = 0; b = 0;

      for (int i = 0; i < NUM_STRIPS; i++) {
        strips[i].setBrightness(255);
        setStripColor(i, r, g, b);
      }
      
      break;
    }


    
  }


 if (sim == 6) {
    // One-time setup
    if (!case6_initialized) {
      for (int i = 0; i < 7; i++) {
        int hue = (i * 36) % 256;
        currentColors[i] = strips[0].gamma32(strips[0].ColorHSV(hue * 256));
        targetColors[i] = currentColors[i];
      }
      case6_initialized = true;
    }

    if (millis() - case6_lastUpdate > case6_updateInterval) {
      case6_lastUpdate = millis();

      // Rotate targetColors clockwise
      uint32_t last = targetColors[6];
      for (int i = 6; i > 0; i--) {
        targetColors[i] = targetColors[i - 1];
      }
      targetColors[0] = last;

      case6_lerpAmount = 0.0;
    }

    if (case6_lerpAmount < 1.0) {
      case6_lerpAmount += case6_lerpStep;
      if (case6_lerpAmount > 1.0) case6_lerpAmount = 1.0;
    }

    for (int i = 0; i < 7; i++) {
      int stripIndex = case6_order[i];
      uint32_t c1 = currentColors[i];
      uint32_t c2 = targetColors[i];

      uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
      uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;

      uint8_t r = r1 + (r2 - r1) * case6_lerpAmount;
      uint8_t g = g1 + (g2 - g1) * case6_lerpAmount;
      uint8_t b = b1 + (b2 - b1) * case6_lerpAmount;

      uint32_t interpolatedColor = (r << 16) | (g << 8) | b;

      for (int j = 0; j < strips[stripIndex].numPixels(); j++) {
        strips[stripIndex].setPixelColor(j, r, g, b);
      }

      // Only update currentColors when we're done fading
      if (case6_lerpAmount >= 1.0) {
        currentColors[i] = targetColors[i];
      }

      strips[stripIndex].show();
    }
  }
}




