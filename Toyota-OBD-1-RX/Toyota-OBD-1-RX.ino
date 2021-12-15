#include <esp_now.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "logo.h"
//Define screen stuffs
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Structure of the data we are receiving from the transmitter
// Must match the transmitter structure
typedef struct struct_message {
   float inj_float;
   int rpm_int;
   int ign_int;
   int iac_int;
   int map_int;
   int ect_int;
   int tps_int;
   int spd_int;
};

// Create a struct_message called obdData
struct_message obdData;

unsigned long lastRXTime;

// callback function that will be executed when data is received
void IRAM_ATTR OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  //Copy the incomingData from the transmitter into obdData. 
  memcpy(&obdData, incomingData, sizeof(obdData));

  //update last received time.
  lastRXTime = millis();
  
    //Serial.print("Bytes received: ");
    //Serial.println(len);
}

void setup() {
  // Initialize Serial Monitor
  //Serial.begin(115200);
  //Serial.println("Serial Started");
  //Serial.println();
  //Serial.println(WiFi.macAddress());
 
  lastRXTime = 0;
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      //Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);

      if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        //Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
    noInterrupts()
     drawTRDLogo();
     interrupts();
  drawNotConnected();
}

void loop() {
    if(lastRXTime != 0){
      if((lastRXTime + 2000) < millis() ){
        drawNotConnected();
        lastRXTime = 0;
      }
      else{
          drawAllData();
      }
    }
}

void drawTRDLogo(void) {
  display.clearDisplay();
  display.setRotation(2);
  display.drawBitmap(
    0,
    0,
    trd_logo, 128, 64, WHITE);
  display.display();
  delay(1000);
}
