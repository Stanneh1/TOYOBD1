#if defined(INJECTOR)
int TCNT1 = 3036; // counter initial value so as to overflow every 1sec: 65536 - 3036 = 62500 * 16us = 1sec (65536 maximum value of the timer)
void InjectorInit() {
 #define TCCR1A = 0; // set entire TCCR1A register to 0
 #define  TCCR1B = 0; // set entire TCCR1B register to 0
 #define  TCCR1B |= (1 << CS12);//prescaler 256 --> Increment time = 256/16.000.000= 16us
  #define TIMSK1 |= (1 << TOIE1); // enable Timer1 overflow interrupt

  // set and initialize the TIMER1
}

// this is called every time a change occurs at the gasoline injector signal and calculates gasoline injector opening time, during the 1sec interval
void InjectorTime() 
{ 
  if (digitalRead(INJECTOR_PIN) == LOW) 
  {
    InjectorTime1 = micros();
  }
  if (digitalRead(INJECTOR_PIN) == HIGH) {
    InjectorTime2 = micros();
  }
  if (InjectorTime2 > InjectorTime1) 
  {
    //It is noticed that with a sharp drop in speed, bursts> 15ms = 15000μs. Warning? that this is engine braking and the injectors are turned off. (braking detected)
    if ((InjectorTime2 - InjectorTime1) > 500 && (InjectorTime2 - InjectorTime1) < 15000) 
    { 
      Injector_Open_Duration += (InjectorTime2 - InjectorTime1) * Ncyl; //Accumulation of the opening time Ncyl of the nozzles to calculate the total flow rate. in microseconds.
      INJ_TIME = InjectorTime2 - InjectorTime1; //injection duration
      num_injection++;
    }
  }
}

//TIMER1 overflow interrupt -- occurs every 1sec -- it holds the time (in seconds) and also prevents the overflowing of some variables
void IRAM_ATTR onTimer() {
  total_duration_inj += (float)Injector_Open_Duration / 1000;  //Total opening time of injectors along the wire in milliseconds. Stored in EEPROM.
  current_duration_inj += (float)Injector_Open_Duration / 1000; //Opening time of injectors along the wire in milliseconds per trip.
  total_consumption_inj = total_duration_inj / 1000 * Ls * Ncyl; //consumed in liters of benzene total
  current_consumption_inj = current_duration_inj / 1000 * Ls * Ncyl; //consumed in liters of benzyl per trip
  current_time_inj += 1000; //current running time of the machine
  total_time_inj += 1000; //the whole running time of the machine
  rpm_inj = num_injection * 2;
  LPH_INJ = (float)Injector_Open_Duration / 1000  * Ls * Ncyl / 2 * 1.2;
  // | injected fuel per second every second | in liters | for 6 pots | xs why such coefficients came out
  Injector_Open_Duration = 0; //nozzles open in a second
  TCNT1 = 3036;
}

#endif
