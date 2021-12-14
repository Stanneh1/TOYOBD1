#define Ncyl 6.0      //6 nozzles
#define Ninjection 1  //injection 1 time per cycle
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

//DEFINE OBD READER
#define  MY_HIGH  HIGH //LOW    // I have inverted the Eng line using an Opto-Coupler, if yours isn't then reverse these low & high defines.
#define  MY_LOW   LOW //HIGHLOW
#define  TOYOTA_MAX_BYTES  24
#define OBD_INJ 1 //Injector pulse width (INJ)
#define OBD_IGN 2 //Ignition timing angle (IGN)
#define OBD_IAC 3 //Idle Air Control (IAC)
#define OBD_RPM 4 //Engine speed (RPM)
#define OBD_MAP 5 //Manifold Absolute Pressure (MAP)
#define OBD_ECT 6 //Engine Coolant Temperature (ECT)
#define OBD_TPS 7 // Throttle Position Sensor (TPS)
#define OBD_SPD 8 //Speed (SPD)
#define OBD_OXSENS 9 //Lambda 1
//#define OBD_OXSENS2 10 //Lambda 2 on V-shaped engine. I do not have it.
//by signal from injectors
#if defined(INJECTOR)

#endif

//Define screen stuffs
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
