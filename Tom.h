#ifndef TOM
#define TOM
#include "config.h"
#include "Meteo.h"


/***********************************************************************************************
* Tom.h: header file for Helper class                                                       *
*                                                                                              *
* Version: 0.1                                                                                 *
* Last updated: 23/06/2025 15:42                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

class Tom {
  public:
    Tom();
  
  private:
    int ixRevs;
    int pollRevs[REVPOLL_COUNT];
    int ixWD;
    int wd6[WD_DIVISOR];
    Meteo items[NumItems];
  public:
    void startup();
    void iToName(int n, char* buf); // buf length == 3
    int nameToI(const char* nm);
    void catchMaxGust();
    void catchWD();
    bool updateMeteo(int loopCount, int maxLoop);
    int makeWDCSV(char* wdBuf);
    int makeValsCSV(char* valsBuf);
    Meteo* getItems() { return items; }
    int getRevsIx() { return ixRevs; }
    int* getPollRevs() { return pollRevs; }
    Meteo getMeteo(const char* nm);
    void resetAll();
};
#endif
