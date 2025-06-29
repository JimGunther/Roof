#include "Tom.h"
#include "Meteo.h"
#include "config.h"
#include <string.h>
#include "Arduino.h"


/***********************************************************************************************
* Tom.cpp: Tom class: helper class to manage Meteo objects                                     *
*                                                                                              *
* Version: 0.3                                                                                 *
* Last updated: 29/06/2025 17:36                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/
Tom::Tom() {};

void Tom::startup() {
  //set pin modes
  pinMode(RainPin, INPUT_PULLUP); 
  pinMode(RevsPin, INPUT_PULLUP);
  pinMode(WDPin, INPUT_PULLUP);
  pinMode(LEDPin, OUTPUT);
  
  char nm[INAME_LEN];
  int i;
  for (i = 0; i < NumItems; i++ ) {
    iToName(i, nm);
    items[i].setup(nm, i);
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
  int n = nameToI("Rv");
  int revsNow = items[n].getCurrRevs();
  int revsInc = revsNow - Tom::pollRevs[Tom::ixRevs];
  revsInc = max(0, revsInc);
  pollRevs[Tom::ixRevs] = revsNow;

  // Update "Gu" Meteo value if prev max exceeded
  Meteo m = getMeteo("Gu");
  if (revsInc > m.getValue()) {
    m.setValue(revsInc);
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
  return items[ix].update();
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
  valsBuf[0] = '$';
  int ixt = nameToI("Tp"); // temperature
  int ixh = nameToI("Hm"); // humidity
  items[ixh].setValue(items[ixt].getValue2()); // get humidity from temperature Meteo object
  for (i = 0; i < NumItems; i++) {
    x += items[i].makeCSV(valsBuf + x);
  }
  return x;
}

Meteo Tom::getMeteo(const char *nm) {
  int i = nameToI(nm);
  return items[i];
}

void Tom::resetAll() {
int i, n;
  for (i = 0; i < WD_DIVISOR; i++) wd6[i] = 0;
  n = nameToI("Rv"); 
  items[n].resetRevs();  // anemometer
  ixRevs = 0;
  ixWD = 0;
}
