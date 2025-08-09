#include "Tom.h"
#include "config.h"
#include <string.h>
#include "Arduino.h"
#define RANDOMISER

/***********************************************************************************************
* Tom.cpp: Tom class: helper class to manage Meteo objects                                     *
*                                                                                              *
* Version: 2.5                                                                                *
* Last updated: 07/08/2025 21:15                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
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

Tom::Tom() {};

void Tom::startup() {
  attachInterrupt(digitalPinToInterrupt(RevsPin), one_Rotation, RISING);  // anemometer

  //set pin modes
  pinMode(RainPin, INPUT_PULLUP); 
  pinMode(RevsPin, INPUT_PULLUP);
  pinMode(WDPin, INPUT_PULLUP);
  pinMode(LEDPin, OUTPUT);
  
  char nm[INAME_LEN];
  int i;
  // Set initial values for meteo objects
  for (i = 0; i < NumItems; i++ ) {
    iToName(i, nm);
    strcpy(items[i]._name, nm);
    items[i]._num = i;
    items[i]._value = 0;
  }

  for (i = 0; i < REVPOLL_COUNT; i++) {
    pollRevs[i] = 0;
  }
   
  for (i = 0; i < WD_DIVISOR; i++) {
    wd6[i] = 0;
  }
  // now initalize variables needed for particular cases
  ixRevs = 0;
  ixWD = 0;

  // Initialize sensors
  bmp.begin();
  aht.begin();
  bh1750a.begin();
  bh1750b.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5c);

  #ifdef RANDOMISER
  randomSeed(millis());
  #endif
   
}

void Tom::iToName(int n, char* buf) {
  int x, len = strlen(Indexer) >> 1; //halve it
  if ((n < 0) || (n >= len)) strcpy(buf, "Er");
  else {
    x = n << 1; // multiply by 2
    strncpy(buf, Indexer + x, 2);
  }
}

int Tom::nameToI(const char *nm) {
  int x = strstr(Indexer, nm) - Indexer;
  x = x >> 1;
  return x;
}

void Tom::catchMaxGust() {
  //int n = nameToI("Rv");
  int revsNow = _currRevs;
  int revsInc = revsNow - pollRevs[ixRevs];
  revsInc = max(0, revsInc);  // no -ve values!
  pollRevs[ixRevs] = revsNow;

  // Update "Gu" Meteo value if prev max exceeded
  int n = nameToI("Gu");
  if (revsInc > items[n]._value) {
    items[n]._value = revsInc;
  }
  ixRevs = (ixRevs + 1) % REVPOLL_COUNT;
}

void Tom::catchWD() {
  int wd64 = analogRead(WDPin) >> 6;  // divide by 64
  wd6[ixWD] = wd64;
  ixWD = (ixWD + 1) % WD_DIVISOR;
}

bool Tom::updateMeteo(int loopCount, int maxLoop) {
  int startNo = maxLoop - 2 * NumItems;
  if (loopCount < startNo) return true; // no action
  if (loopCount & 1) return true; //no action for odd-numbered loops
  int ix = (loopCount - startNo) >> 1;  // divide by 2
  int vlu;
  float p, ll;
  sensors_event_t humidity, temp;
  bool bUpdated = true;
  switch (ix) {
    case 0: //"Bu"
      items[ix]._value = digitalRead(RainPin);
      break;
    case 1: //"Rv"
      items[ix]._value = _currRevs;
      break;
    case 2: //"Gu"
    // _value already updated by catchMaxGust() method, so nothing to do...
      break;
    case 3: //"Tp:  // gets humidity too
      if (aht.getEvent(&humidity, &temp)) {
        items[ix]._value = (int)(10.0 * temp.temperature);  // to allow 1 d.p. display
        items[ix + 1]._value = (int)humidity.relative_humidity;
      }
      break;
    case 4: //"Hm"
      // do nothing: updated with temperature
      break;
    case 5: // "Pr"
      bmp.getPressure(&p);
      items[ix]._value = (int)(p * .01);  
      break;
    case 6: //"Lt"
      ll = (int)((bh1750a.readLightLevel() + bh1750b.readLightLevel()) / 2);
      items[ix]._value = ll;
      break;
    default:
      bUpdated = false;
      break;  // do nothing
  }
  
  #ifdef RANDOMISER
  long r = random(100);
  if ((ix == 0) && (r > 90)) { items[0]._value = 1 - items[0]._value; }  // simulate tip
  r = random(100);
  long s = random(3);
  if ((ix == 1) && (r > 70)) { items[1]._value = items[1]._value + (int)s; }
  #endif

  return bUpdated;
}

int Tom::makeWDCSV(char* wdBuf) {
  int i, count = 0, x = 1;
  wdBuf[0] = '$';
  for (i = 0; i < WD_DIVISOR; i++) {
    sprintf(wdBuf + x, ",%02d", wd6[i]);
    x += 3;
    count += 1;
  }
  return count;
}

int Tom::makeValsCSV(char *valsBuf) {
  int i, x = 1;
  char itemBuf[10];
  valsBuf[0] = '$';
  bool bAllZero = true;
  for (i = 0; i < NumItems; i++) {
    if (items[i]._value > 0) bAllZero = false;
    sprintf(itemBuf, ",%s:%04d", items[i]._name, items[i]._value);
    //Serial.print(items[i]._name);
    //Serial.println(items[i]._value);
    //Serial.println(itemBuf);
    strcpy(valsBuf + i * 8 + 1, itemBuf);
  }
  if (bAllZero) return 0;
  return 8 * NumItems;
}

int Tom::getLight4Blink() {
  int n = nameToI("Lt");
  return items[n]._value;
}

void Tom::resetAll() {
int i, n;
  for (i = 0; i < WD_DIVISOR; i++) wd6[i] = 0;
  n = nameToI("Rv"); 
  items[n]._value = 0;  // anemometer: NOT SURE ABOUT THIS ONE
  ixRevs = 0;
  ixWD = 0;
}
