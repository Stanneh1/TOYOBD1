#if defined(SDCARD)
#define SD_CS 5
#define FILE_BASE_NAME "/Data"   //filename pattern

  char fileName[13] = FILE_BASE_NAME "00.csv";
  const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
void writeHeader() {

  File file = SD.open(fileName);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
#ifdef LOGGING_FULL
  file.print(F("INJ_OBD;INJ_HW;IGN;IAC;RPM;MAP;ECT;TPS;SPD;OXSENS;ASE;COLD;DET;OXf;Re-ENRICHMENT;STARTER;IDLE;A/C;NEUTRAL;Ex1;Ex2;AVG SPD;LPK_OBD;LPH_OBD;LPH_INJ;TOTAL_OBD;TOTAL_INJ;AVG_OBD;AVG_INJ"));
#else
 file.print(F("INJ_OBD;INJ_HW;IGN;IAC;RPM;MAP;ECT;TPS;SPD;O2SENS;EXHAUST;AVG SPD;LPH_INJ;TOTAL_INJ;AVG_INJ;CURR_INJ;CURR_RUN;TOTAL_RUN;"));
#endif
  file.println();
  //if (!file.sync() || file.getWriteError())  error("write error");

  file.close();
}
void logData() {

File file = SD.open(fileName);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.print(getOBDdata(OBD_INJ)); file.write(';'); file.print(float(INJ_TIME) / 1000); file.write(';'); file.print(getOBDdata(OBD_IGN));  file.write(';');  file.print(getOBDdata(OBD_IAC));  file.write(';');
  file.print(getOBDdata(OBD_RPM)); file.write(';'); file.print(getOBDdata(OBD_MAP));  file.write(';'); file.print(getOBDdata(OBD_ECT));  file.write(';');
  file.print(getOBDdata(OBD_TPS)); file.write(';');  file.print(getOBDdata(OBD_SPD));  file.write(';'); file.print(getOBDdata(OBD_OXSENS)); file.write(';'); file.print(getOBDdata(20)); file.write(';');
#ifdef LOGGING_FULL
  file.print(getOBDdata(11)); file.write(';'); file.print(getOBDdata(12)); file.write(';'); file.print(getOBDdata(13)); file.write(';'); file.print(getOBDdata(14)); file.write(';');
  file.print(getOBDdata(15)); file.write(';'); file.print(getOBDdata(16)); file.write(';'); file.print(getOBDdata(17)); file.write(';'); file.print(getOBDdata(18)); file.write(';');
  file.print(getOBDdata(19)); file.write(';'); file.print(getOBDdata(20)); file.write(';'); file.print(getOBDdata(21)); file.write(';');
#endif
  file.print(total_avg_speed); file.write(';');//AVG_SPD ok
  file.print(float(INJ_TIME) / 1000 * getOBDdata(OBD_RPM)*Ls * 0.18); file.write(';'); //LPH_INJ ok
  file.print(total_consumption_inj); file.write(';'); //TOTAL_INJ   ok
  file.print(avg_consumption_inj);  file.write(';'); //!AVG_INJ
  file.print(current_consumption_inj);  file.write(';'); //!CURR_INJ

  file.print(current_run);   file.write(';'); //CURR_RUN ok
  file.print(total_run); file.write(';'); //RUN_TOTAL ok

  file.println();

  file.close();
  //if (!file.sync() || file.getWriteError())  error("write error");
}

void SDinit(){
  // Initialize SD card
  SD.begin(SD_CS);  
  if(!SD.begin(SD_CS)) {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE) {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ERROR - SD card initialization failed!");
    return;    // init failed
  }


  
  File file = SD.open(fileName);
  if(!file) {
    Serial.println("File doens't exist");
    Serial.println("Creating file...");
    writeFile(SD,fileName, "INJ_OBD;INJ_HW;IGN;IAC;RPM;MAP;ECT;TPS;SPD;OXSENS;ASE;COLD;DET;OXf;Re-ENRICHMENT;STARTER;IDLE;A/C;NEUTRAL;Ex1;Ex2;AVG SPD;LPK_OBD;LPH_OBD;LPH_INJ;TOTAL_OBD;TOTAL_INJ;AVG_OBD;AVG_INJ");
  }
  else {
    Serial.println("File already exists");
        if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
        writeFile(SD,fileName, "ESP32 and SD Card \r\n");
  }
  file.close();
  
    while (SD.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
       Serial.println("Can't create file name");  
//      error("Can't create file name");
    }
  }

}
}

// Write to the SD card (DON'T MODIFY THIS FUNCTION)
void writeFile(fs::FS &fs, const char * path, const char * message) {
  Serial.printf("Writing file: %s\n", path);
  File file = fs.open(path, FILE_WRITE);
  if(!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message)) {
    Serial.println("File written");
  } else {
    Serial.println("Write failed");
  }
  file.close();
}


#endif
