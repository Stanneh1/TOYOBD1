// ToyotaOBD1_Reader
// In order to read the data from the OBD connector, short E1 + TE2. then to read the data connect to VF1.
// Note the data line output is 12V - connecting it directly to one of the arduino pins might damage (proabably) the board
// This is made for diaply with an OLED display using the U8glib - which allow wide range of display types with minor adjusments.
// Many thanks to GadgetFreak for the greate base code for the reasding of the data.
// If you want to use invert line - note the comments on the MY_HIGH and the INPUT_PULLUP in the SETUP void.

#include "FS.h"
#include "SD.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
//#include <MD_KeySwitch.h>
#include "DEFINES.h"
#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "logo.h"

int i = 0;

static const BaseType_t pro_cpu = 0;
static const BaseType_t app_cpu = 1;
TaskHandle_t readAndSendDataTask;

//Espdevkit1 Receiver mac addy 58:BF:25:9A:17:E8
// Structure example to send data
// Must match the receiver structure
// REPLACE WITH YOUR RECEIVER MAC Address
uint8_t broadcastAddress[] = { 0x58, 0xBF, 0x25, 0x9A, 0x17, 0xE8 };

// Create a struct_message called obdData
struct_message obdData;
// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

#define Ls 0.003965888 //Injector nozzle capacity liters per second // base 0.004 or 240cc

//comment out if you do not need the functionality of logging to the SD card.
//also be sure to comment out if the SD module is not connected. Otherwise, the program will not start
//#define SDCARD 

#if defined(SDCARD) //DEFINE SD card writer
#define SD_CS 5
#define FILE_BASE_NAME "/Data"   //filename pattern

#endif

//reading the signal from the injectors. As practice has shown,
//the consumption according to the HBS is calculated quite accurately.
//There is no sense in using the signal from the injectors.
//#define INJECTOR

#define LOGGING_FULL //Writing all data to SD
#define DEBUG_OUTPUT true // for debug option - swith output to Serial

#define SHOW_FALSE_RPMS

//DEFINE pins for inputs-outputs
#define LED_PIN          33
#define ENGINE_DATA_PIN 26 //VF1 PIN
//#define TOGGLE_BTN_PIN 14 //screen change button interupt PIN
#if defined(INJECTOR)
  #define INJECTOR_PIN 25 //engine injector PIN
  volatile unsigned long Injector_Open_Duration = 0;
  volatile unsigned long INJ_TIME = 0;
  volatile unsigned long InjectorTime1 = 0;
  volatile unsigned long InjectorTime2 = 0;
  volatile uint32_t num_injection = 0;
  volatile uint16_t rpm_inj = 0;
  float total_avg_consumption;
  float avg_consumption_inj;
  volatile float total_duration_inj, current_duration_inj;
  volatile float total_consumption_inj, current_consumption_inj;
  volatile uint32_t current_time_inj, total_time_inj;
#endif

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//MD_KeySwitch S(TOGGLE_BTN_PIN, HIGH);
byte CurrentDisplayIDX = 4;

float current_run = 0;
float total_run = 0;
float total_avg_speed;
float avg_speed;
unsigned long current_time = 0;
unsigned long total_time = 0;
unsigned long t = 0;
float  cycle_obd_inj_dur, trip_obd_inj_dur, total_obd_inj_dur, trip_obd_fuel_consumption,
       total_obd_fuel_consumption, trip_obd_avg_fuel_consumption, total_obd_avg_fuel_consumption; //by obd protocol
float LPK, LPH, LPH_INJ;
bool flagNulSpeed = true;
volatile uint8_t ToyotaNumBytes, ToyotaID, ToyotaData[TOYOTA_MAX_BYTES];
volatile uint16_t ToyotaFailBit = 0;
boolean LoggingOn = false;

float IRAM_ATTR getOBDdata(byte OBDdataIDX) {
  float returnValue;
  switch (OBDdataIDX) {
    case 0:// UNKNOWN
      returnValue = ToyotaData[0];
      break;
    case OBD_INJ: //Injector injection time = X * 0.125 (ms)
      returnValue = ToyotaData[OBD_INJ] * 0.125; //Injection time of injectors
      break;
    case OBD_IGN: //Ignition advancing angle X * 0.47-30 (deg)
      returnValue = ToyotaData[OBD_IGN] * 0.47 - 30;
      break;
    case OBD_IAC: //Valve state XX For different types of KXX different formulas: X / 255 * 100 (%) X (step)
      returnValue = ToyotaData[OBD_IAC] * 0.39215; ///optimize divide
      break;
    case OBD_RPM: //Crankshaft speed X * 25 (rpm)
      returnValue = ToyotaData[OBD_RPM] * 25;
      break;
    case OBD_MAP: //Air mass meter (MAP / MAF)
      //  X*0.6515 (kPa)
      //  X*4.886 (mm mercury column)
      //  X*0.97 (kPa) (for turbo engines)
      //  X*7.732 (mmHg) (for turbo engines)
      //  X*2(гр/сек) MAF
      //  X/255*5 (Volt) (voltage at flow meter)
      returnValue = ToyotaData[OBD_MAP] * 2; //Raw data
      break;
    case OBD_ECT: // Engine temperature (ECT)
      // Depending on the value of X, different formulas are:
      // 0..14:          =(Х-5)*2-60
      // 15..38:        =(Х-15)*0.83-40
      // 39..81:        =(Х-39)*0.47-20
      // 82..134:      =(Х-82)*0.38
      // 135..179:    =(Х-135)*0.44+20
      // 180..209:    =(Х-180)*0.67+40
      // 210..227:    =(Х-210)*1.11+60
      // 228..236:    =(Х-228)*2.11+80
      // 237..242:    =(Х-237)*3.83+99
      // 243..255:    =(Х-243)*9.8+122
      // Temperature in degrees Celsius.
      if (ToyotaData[OBD_ECT] >= 243)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 243) * 9.8) + 122;
      else if (ToyotaData[OBD_ECT] >= 237)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 237) * 3.83) + 99;
      else if (ToyotaData[OBD_ECT] >= 228)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 228) * 2.11) + 80.0;
      else if (ToyotaData[OBD_ECT] >= 210)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 210) * 1.11) + 60.0;
      else if (ToyotaData[OBD_ECT] >= 180)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 180) * 0.67) + 40.0;
      else if (ToyotaData[OBD_ECT] >= 135)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 135) * 0.44) + 20.0;
      else if (ToyotaData[OBD_ECT] >= 82)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 82) * 0.38);
      else if (ToyotaData[OBD_ECT] >= 39)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 39) * 0.47) - 20.0;
      else if (ToyotaData[OBD_ECT] >= 15)
        returnValue = ((float)(ToyotaData[OBD_ECT] - 15) * 0.83) - 40.0;
      else
        returnValue = ((float)(ToyotaData[OBD_ECT] - 15) * 2.0) - 60.0;
      break;
    case OBD_TPS: // Throttle position
      // X/2 (degrees)
      // X/1.8(%)
      returnValue = ToyotaData[OBD_TPS] / 1.8;
      break;
    case OBD_SPD: //Vehicle speed (km / h)
      returnValue = ToyotaData[OBD_SPD];
      break;
    //Correction for in-line / correction of the first half
    case OBD_OXSENS:
      returnValue = (float)ToyotaData[OBD_OXSENS] * 0.01953125;
      break;
    //Correction of the second half
    /*
      case OBD_OXSENS2:// Lambda2 tst
      returnValue = (float)ToyotaData[OBD_OXSENS2] * 0.01953125;
      break;*/
    //  read bytes of flags bit by bit
    case 11:
      returnValue = bitRead(ToyotaData[11], 0);  //Re-enrichment after starting 1-On
      break;
    case 12:
      returnValue = bitRead(ToyotaData[11], 1); //Cold engine 1 - Yes
      break;
    case 13:
      returnValue = bitRead(ToyotaData[11], 4); //Detonation 1-Yes
      break;
    case 14:
      returnValue = bitRead(ToyotaData[11], 5); //Lambda probe feedback 1-Yes
      break;
    case 15:
      returnValue = bitRead(ToyotaData[11], 6); //Additional enrichment 1-Yes
      break;
    case 16:
      returnValue = bitRead(ToyotaData[12], 0); //Starter 1-Yes
      break;
    case 17:
      returnValue = bitRead(ToyotaData[12], 1); //Sign XX (Throttle valve) 1-Yes (Closed)
      break;
    case 18:
      returnValue = bitRead(ToyotaData[12], 2); //Air conditioner 1-Yes
      break;
    case 19:
      returnValue = bitRead(ToyotaData[13], 3); //Neutral 1-Yes
      break;
    case 20:
      returnValue = bitRead(ToyotaData[14], 4); //First Half Blend 1-Rich, 0-Poor
      break;
    case 21:
      returnValue = bitRead(ToyotaData[14], 5); //Mixture of the second half 1-Rich, 0-Poor
      break;
    default: // DEFAULT CASE (in no match to number)
      // send "error" value
      returnValue =  9999.99;
  } // end switch
  // send value back
  return returnValue;
} // end void getOBDdata

void ReadEEPROM(){
    EEPROM.get(104, total_run);
    EEPROM.get(108, total_time);
    EEPROM.get(200, total_obd_inj_dur);
    EEPROM.get(204, trip_obd_inj_dur);
    //EEPROM.get(208, total_closed_duration);
 }

void SaveEEPROM(){
    EEPROM.put(104, total_run);
    EEPROM.put(108, total_time);
    EEPROM.put(200, total_obd_inj_dur);
    EEPROM.put(204, trip_obd_inj_dur);
    //EEPROM.put(208, total_closed_duration);
}

void cleardata() {
  int i;
  for (i = 104; i <= 112; i++) {
    EEPROM.put(i, 0);
  }
  for (i = 200; i <= 212; i++) {
    EEPROM.put(i, 0);
  }
  ReadEEPROM();
}

void IRAM_ATTR drawAllData(void *) {
  // graphic commands to redraw the complete screen.
  for(;;){
    //Serial.println("drawing data");
      //  unsigned long new_t;
      //  unsigned int diff_t;
      
    if (ToyotaNumBytes > 0)  { 
        // This is where we will end up when the data has been captured and this is where we will really display data
        //For debugging purposes I have also added it outside of this if statement so I can obtain readings.

          //    new_t = millis();
          //    if (new_t > t) {
          //      diff_t = new_t - t;
          //      cycle_obd_inj_dur = getOBDdata(OBD_RPM) / 60000.0  * Ncyl * Ninjection * (float)diff_t  * getOBDdata(OBD_INJ) / 2.0; //Opening time of injectors for 1 data cycle.
          //             // In the MC the nozzle is triggered every 2 revolutions KV
          //             //cycle time ms in s. We get the number of operations during the cycle. Multiply by the injector opening time,
          //             //we get the opening time of 6 injectors IN MILLISECONDS
          //      current_run += (float)diff_t / 3600000 * getOBDdata(OBD_SPD);  //Distance traveled since switching on. In KM
          //      total_run += (float)diff_t / 3600000 * getOBDdata(OBD_SPD);    //Total distance traveled. EEPROM. In KM
          //      trip_obd_inj_dur += cycle_obd_inj_dur; //Time of open injectors for a trip to the MC
          //      total_obd_inj_dur += cycle_obd_inj_dur; //Opening times of injectors for all time. EEPROM IN MS
          //      total_time += diff_t; //total elapsed time in milliseconds limit ~ 49 days. EEPROM
          //      current_time += diff_t; //Travel time in milliseconds since power on
          //      total_avg_speed = total_run / (float)total_time * 3600000; //average speed for all time. km \ h
          //      avg_speed = current_run / (float)current_time * 3600000 ; //average speed
          //      trip_obd_fuel_consumption = trip_obd_inj_dur  * Ls / 1000.0; //fuel consumption per trip in liters
          //      total_obd_fuel_consumption = total_obd_inj_dur  * Ls / 1000.0;  //all-time fuel consumption. From EEPROM in liters
          //      trip_obd_avg_fuel_consumption = 100.0 * trip_obd_fuel_consumption / current_run; //average travel expense
          //      total_obd_avg_fuel_consumption = 100.0 * total_obd_fuel_consumption / total_run;
          //      LPK = 100 / getOBDdata(OBD_SPD) * (getOBDdata(OBD_INJ) * getOBDdata(OBD_RPM) * Ls * 0.18);
          //      LPH = getOBDdata(OBD_INJ) * getOBDdata(OBD_RPM) * Ls * 0.18;
          //      t = millis();
          //    }
          //#if defined(INJECTOR)
          //    //by signal from injectors
          //     //total_avg_consumption = 100 * total_consumption_inj / total_run;
          //     //avg_consumption_inj = 100 * current_consumption_inj / current_run; //average l/100km for unleaded fuel //kind of ok
          //#endif

          
          display.clearDisplay();
          display.setRotation(2);
          
          display.setTextSize(1);      // Normal 1:1 pixel scale
          display.setTextColor(WHITE); // Draw white text
          display.cp437(true);         // Use full 256 char 'Code Page 437' font
        
          display.setCursor(0, 10);     // Start at top-left corner
          display.println(F("INJ"));
          display.setCursor(25, 10);
          display.print(getOBDdata(OBD_INJ));
        
          display.setCursor(0, 25);
          display.println(F("IGN"));
          display.setCursor(25, 25);
          display.print(getOBDdata(OBD_IGN));
        
          display.setCursor(0, 40);
          display.println(F("IAC"));
          display.setCursor(25, 40);
          display.print(getOBDdata(OBD_IAC));
        
          display.setCursor(0, 55);
          display.println(F("RPM"));
          display.setCursor(25, 55);
          display.print(getOBDdata(OBD_RPM));
        
          display.setCursor(65, 10);
          display.println(F("MAP"));
          display.setCursor(92, 10);
          display.print(getOBDdata(OBD_MAP));
        
          display.setCursor(65, 25);
          display.println(F("ECT"));
          display.setCursor(92, 25);
          display.print(getOBDdata(OBD_ECT));
        
          display.setCursor(65, 40);
          display.println(F("TPS"));
          display.setCursor(92, 40);
          display.print(getOBDdata(OBD_TPS));
        
          display.setCursor(65, 55);
          display.println(F("SPD"));
          display.setCursor(92, 55);
          display.print(getOBDdata(OBD_SPD));
                
          display.display(); //draw all of the above on the display as text

      
          ToyotaNumBytes = 0;     // reset the counter.
    }
      //The following display code should be removed for live use this is just so that I can get
      //test data displaying.
      #if defined(SHOW_FALSE_RPMS)
      display.clearDisplay();
      display.setRotation(2);
      
      display.setTextSize(1);      // Normal 1:1 pixel scale
      display.setTextColor(WHITE); // Draw white text
      display.cp437(true);         // Use full 256 char 'Code Page 437' font
    
      display.setCursor(0, 10);     // Start at top-left corner
      display.println(F("INJ"));
      display.setCursor(25, 10);
      display.print(getOBDdata(OBD_INJ));
    
      display.setCursor(0, 25);
      display.println(F("IGN"));
      display.setCursor(25, 25);
      display.print(getOBDdata(OBD_IGN));
    
      display.setCursor(0, 40);
      display.println(F("IAC"));
      display.setCursor(25, 40);
      display.print(getOBDdata(OBD_IAC));
    
//      display.setCursor(0, 55);
//      display.println(F("RPM"));
//      display.setCursor(25, 55);
//      display.print(getOBDdata(OBD_RPM));

      display.setCursor(0, 55);
      display.println(F("RPM"));
      display.setCursor(25, 55);
      display.print(i);
    
      display.setCursor(65, 10);
      display.println(F("MAP"));
      display.setCursor(92, 10);
      display.print(getOBDdata(OBD_MAP));
    
      display.setCursor(65, 25);
      display.println(F("ECT"));
      display.setCursor(92, 25);
      display.print(getOBDdata(OBD_ECT));
    
      display.setCursor(65, 40);
      display.println(F("TPS"));
      display.setCursor(92, 40);
      display.print(getOBDdata(OBD_TPS));
    
      display.setCursor(65, 55);
      display.println(F("SPD"));
      display.setCursor(92, 55);
      display.print(getOBDdata(OBD_SPD));
      
      display.display(); //draw all of the above on the display as text

      #endif

      if (getOBDdata(OBD_SPD) == 0 && flagNulSpeed == false)  
      { 
        SaveEEPROM();
        flagNulSpeed = true; //re-recording prohibition
      }
      
      //started to move - enable recording  
      if (getOBDdata(OBD_SPD) != 0)
      flagNulSpeed = false;
          
    #if defined(SDCARD)
        //every 0.5s record in the data log by double pressing the button
        if (millis() % 500 < 50 && LoggingOn == true) 
        {         
          logData();
        }
    #endif

     //Serial.println("finished drawing data");
     
     //increment i to display as the RPM value for testing.
     i=i+1;
  }
   
} // end void drawalldata

void IRAM_ATTR SendAllData(void *) {
  for(;;){
    //Serial.println("Sending Data");
    obdData.inj_float = getOBDdata(OBD_INJ);
    obdData.rpm_int = int(getOBDdata(OBD_RPM));
    #if defined(SHOW_FALSE_RPMS)
    obdData.rpm_int = i;
    #endif
    obdData.ign_int = int(getOBDdata(OBD_IGN));
    obdData.iac_int = int(getOBDdata(OBD_IAC));
    obdData.map_int = int(getOBDdata(OBD_MAP));
    obdData.ect_int = int(getOBDdata(OBD_ECT));
    obdData.tps_int = int(getOBDdata(OBD_TPS));
    obdData.spd_int = int(getOBDdata(OBD_SPD));
      // Send message via ESP-NOW
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&obdData, sizeof(obdData));
    if (result == ESP_OK) {
      //Serial.println("Sent with success");
    } else {
      //Serial.println("Error sending the data");
    }

    //Serial.println("Finished Sending Data");
  }
} // end void SendAllData

void IRAM_ATTR  vf1Changed() {
  static uint8_t ID, EData[TOYOTA_MAX_BYTES];
  static boolean InPacket = false;
  static unsigned long StartMS;
  static uint16_t BitCount;
  int state = digitalRead(ENGINE_DATA_PIN);
    //Serial.print(F("Vf1 State Changed to: "));
  //Serial.println(state);
    //digitalWrite(LED_PIN, !state);
  if (InPacket == false)  {
    if (state == MY_HIGH)   {
      StartMS = millis();
    }   else   { // else  if (state == MY_HIGH)
      if ((millis() - StartMS) > (15 * 8))   {
        StartMS = millis();
        InPacket = true;
        BitCount = 0;
      } // end if  ((millis() - StartMS) > (15 * 8))
    } // end if  (state == MY_HIGH)
  }  else   { // else  if (InPacket == false)
    uint16_t bits = ((millis() - StartMS) + 1 ) / 8; // The +1 is to cope with slight time errors
    StartMS = millis();
    // process bits
    while (bits > 0)  {
      if (BitCount < 4)  {
        if (BitCount == 0)
          ID = 0;
        ID >>= 1;
        if (state == MY_LOW)  // inverse state as we are detecting the change!
          ID |= 0x08;
      }   else    { // else    if (BitCount < 4)
        uint16_t bitpos = (BitCount - 4) % 11;
        uint16_t bytepos = (BitCount - 4) / 11;
        if (bitpos == 0)      {
          // Start bit, should be LOW
          if ((BitCount > 4) && (state != MY_HIGH))  { // inverse state as we are detecting the change!
            ToyotaFailBit = BitCount;
            InPacket = false;
            break;
          } // end if ((BitCount > 4) && (state != MY_HIGH))
        }  else if (bitpos < 9)  { //else TO  if (bitpos == 0)
          EData[bytepos] >>= 1;
          if (state == MY_LOW)  // inverse state as we are detecting the change!
            EData[bytepos] |= 0x80;
        } else { // else if (bitpos == 0)
          // Stop bits, should be HIGH
          if (state != MY_LOW)  { // inverse state as we are detecting the change!
            ToyotaFailBit = BitCount;
            InPacket = false;
            break;
          } // end if (state != MY_LOW)
          if ( (bitpos == 10) && ((bits > 1) || (bytepos == (TOYOTA_MAX_BYTES - 1))) ) {
            ToyotaNumBytes = 0;
            ToyotaID = ID;
            for (uint16_t i = 0; i <= bytepos; i++)
              ToyotaData[i] = EData[i];
            ToyotaNumBytes = bytepos + 1;
            if (bits >= 16)  // Stop bits of last byte were 1's so detect preamble for next packet
              BitCount = 0;
            else  {
              ToyotaFailBit = BitCount;
              InPacket = false;
            }
            break;
          }
        }
      }
      ++BitCount;
      --bits;
    } // end while
  } // end (InPacket == false)

   //Serial.println(F("Finished Processing Vf1 State Change"));
}

void runVF1ChangedTask(void * pvParameters){attachInterrupt(ENGINE_DATA_PIN, vf1Changed, CHANGE);vTaskDelete(NULL);}

void setup() {
    // SSD1306_SWITCHCAPVCC = generate   voltage from 3.3V internally
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
      //Serial.println(F("SSD1306 allocation failed"));
      for(;;); // Don't proceed, loop forever
    }
      // Show TRD Boot Logo.
      display.setRotation(2);
      noInterrupts()
      drawTRDLogo();
      interrupts();


  // Init Serial Monitor
  //Serial.begin(115200);
  //Serial.println("serial started");
  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    //Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  // Add peer
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    //Serial.println("Failed to add peer");
    return;
  }
    
  pinMode(ENGINE_DATA_PIN, INPUT_PULLDOWN); // VF1 PIN

  //Setup our tasks and which core they run on and what priority.

  //VF1ChangeTask this task runs on core 0 with a priority of 2 and it is triggered whenever a change in the signal from VF1 on pin 26 changes from high to low and vice versa.
    xTaskCreatePinnedToCore(
                      runVF1ChangedTask,   /* Task function. */
                      "vf1 Changed",     /* name of task. */
                      4000,       /* Stack size of task */
                      NULL,        /* parameter of the task */
                      2,           /* priority of the task */
                      NULL,      /* Task handle to keep track of created task */
                      pro_cpu);          /* pin task to core 1 */  

  //drawAllData this task runs on a constant loop on core 1 with a priority of 2 constantly drawing the contents of the obdData object
    xTaskCreatePinnedToCore(
                      drawAllData,   /* Task function. */
                      "Draw All Data",     /* name of task. */
                      4000,       /* Stack size of task */
                      NULL,        /* parameter of the task */
                      2,           /* priority of the task */
                      NULL,      /* Task handle to keep track of created task */
                      app_cpu);          /* pin task to core 1 */

  //SendAllData this task runs on a constant loop on core 1 with a priority of 1 constantly sending the contents of the obdData object to the reveiver
    xTaskCreatePinnedToCore(
                      SendAllData,   /* Task function. */
                      "Send All Data",     /* name of task. */
                      4000,       /* Stack size of task */
                      NULL,        /* parameter of the task */
                      1,           /* priority of the task */
                      NULL,      /* Task handle to keep track of created task */
                      app_cpu);          /* pin task to core 1 */
                      
    ReadEEPROM();
  
    if (DEBUG_OUTPUT) {
    // Serial.println("system Started");
    // Serial.print("Read float from EEPROM: ");
    // Serial.println(total_run, 3);
    // Serial.println(total_time, 3);
    // Serial.println(total_obd_avg_fuel_consumption, 3);
    // Serial.println(total_obd_inj_dur_ee, 3);
  }


#if defined(SDCARD)
  char fileName[13] = FILE_BASE_NAME "00.csv";
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
  SDinit();
  writeHeader();
#endif
#if defined(INJECTOR)
  InjectorInit();
  pinMode(INJECTOR_PIN, INPUT); // Injector PIN
  attachInterrupt(digitalPinToInterrupt(INJECTOR_PIN), InjectorTime, CHANGE); //setup Interrupt for data line
#endif
  t = millis();
} // END VOID SETUP


void loop(void) {
  
   // read hall effect sensor value
//int  hallVal = hallRead();
//  // print the results to the serial monitor
//  if(hallVal > 1){
//     Serial.println(hallVal);  
//  }


//int touchVal = touchRead(T0);
//if(touchVal < 40)
//{
//  Serial.println(touchVal);
//}

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
