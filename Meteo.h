#ifndef METEO
#define METEO
#include "config.h"

#include <Wire.h>
#include <Adafruit_BMP085_U.h>
#include <Adafruit_AHTX0.h>
#include <BH1750.h>

/***********************************************************************************************
* Meteo.h: header file for Meteo class (replaces WInputs.h)                                    *
*                                                                                              *
* Version: 0.1                                                                                 *
* Last updated: 23/06/2025 09:05                                                               *
* Author: Jim Gunther                                                                          *
*                                                                                              *
***********************************************************************************************/

class Meteo {
  public:
    Meteo();
    void setup(const char* nm, int num);
    int getCurrRevs();
    void resetRevs();
    void getName(char* buf);
    int getValue();
    void setValue(int v);
    int getValue2();
    bool update();
    int makeCSV(char* buf);
  private:
    char _name[INAME_LEN];
    int _num;
    int _value;
    int _value2;
    
    Adafruit_AHTX0 aht;
    Adafruit_BMP085_Unified bmp;
    BH1750 bh1750a;
    BH1750 bh1750b;  
};
#endif