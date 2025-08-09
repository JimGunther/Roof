#ifndef TOM
#define TOM
#include "config.h"
#include <Wire.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>


/***********************************************************************************************
* Tom.h: header file for Helper class                                                       *
*                                                                                              *
* Version: 2.5                                                                                 *
* Last updated: 07/08/2025 21:15                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

struct meteo {
    char _name[INAME_LEN];
    int _num;
    int _value;
};

class Tom {
  public:
    Tom();
  
  private:
    int ixRevs;
    int pollRevs[REVPOLL_COUNT];
    int ixWD;
    int wd6[WD_DIVISOR];
    meteo items[NumItems];
    
    Adafruit_AHTX0 aht;
    Adafruit_BMP085_Unified bmp;
    BH1750 bh1750a;
    BH1750 bh1750b;  

  public:
    void startup();
    void iToName(int n, char* buf); // buf length == 3
    int nameToI(const char* nm);
    void catchMaxGust();
    void catchWD();
    bool updateMeteo(int loopCount, int maxLoop);
    int makeWDCSV(char* wdBuf);
    int makeValsCSV(char* valsBuf);
    int getRevsIx() { return ixRevs; }
    int getLight4Blink();
    int* getPollRevs() { return pollRevs; }
    void resetAll();
};
#endif
