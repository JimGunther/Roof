#include "Meteo.h"
#include "Arduino.h"

// Interrupt Service Routine starts here______________

// Wind 
const int _margin = 5;  // minimum milliseconds between checks: insurance against contact bounce:   
volatile int _currRevs = 0;
volatile unsigned long _lastSTime = 0;
volatile unsigned long _thisSTime;

void IRAM_ATTR one_Rotation();
void one_Rotation() {
  _thisSTime = esp_timer_get_time() / 1000;
  if (_thisSTime - _lastSTime > _margin) {
    _currRevs++;
    _lastSTime = _thisSTime;
  }
}
// ------------------------ END OF ISR --------------------------------------------------------------

/***********************************************************************************************
* Meteo.cpp: code file for Meteo class                                                             *
*                                                                                              *
* Version: 0.3                                                                                 *
* Last updated: 29/06/2025 17:32                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

// ************************* START OF Class Meteo *******************************************
Meteo::Meteo() {}

void Meteo::setup(const char* nm, int num) {
  strcpy(_name, nm);
  _num = num;
  
  // Start relevant sensor(s) to Meteo type and attach ISR to interrupt
  if (strcmp(_name, "Bu") == 0) {  }
  if (strcmp(_name, "Rv") == 0) {  
    attachInterrupt(digitalPinToInterrupt(RevsPin), one_Rotation, RISING);  // anemometer
  }
  if (strcmp(_name, "Pr") == 0) { bmp.begin(); }
  if (strcmp(_name, "Tp") == 0) { aht.begin(); }
  if (strcmp(_name, "Lt") == 0){
    bh1750a.begin();
    bh1750b.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5c);
  }
}

int Meteo::getCurrRevs() {
  return _currRevs;
}

void Meteo::resetRevs() {
  _currRevs = 0;
}

bool Meteo::update() {
  int currVal;
  float p, ll;
  sensors_event_t humidity, temp;
  bool bUpdated = true;
  switch (_num) {
    case 0: //"Bu"
      _value = digitalRead(RainPin);
      break;
    case 1: //"Rv"
      _value = _currRevs;
      break;
    case 2: //"Gu"
    // _value already updated by catchMaxGust() method, so nothing to do...
      break;
    case 3: //"Tp:  // gets humidity too
      if (aht.getEvent(&humidity, &temp)) {
        _value = (int)(10.0 * temp.temperature);  // to allow 1 d.p. display
        _value2 = (int)humidity.relative_humidity;
      }
      break;
    case 4: //"Hm"
      // do nothing: updated as _value2 with temperature
      break;
    case 5: // "Pr"
      bmp.getPressure(&p);
      _value = (int)(p * .01);  
      break;
    case 6: //"Lt"
      ll = (int)((bh1750a.readLightLevel() + bh1750b.readLightLevel()) / 2);
      _value = ll;
      break;
    default:
      bUpdated = false;
      break;  // do nothing
  }
  return bUpdated;
}

int Meteo::makeCSV(char * buf) {
  return sprintf(buf, ",%s:%04d", _name, _value);
}

void Meteo::getName(char* buf) { strncpy(buf, _name, 2); }

int Meteo::getValue() { return _value; }

int Meteo::getValue2() { return _value2; }

void Meteo::setValue(int v) { _value = v; }
