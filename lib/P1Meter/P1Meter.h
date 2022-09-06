#ifndef P1METER_H
#define P1METER_H
#include <ArduinoJson.h>
#include <vector>

class P1Meter
{
private:
    unsigned int CRC16(unsigned int crc, unsigned char *buf, int len);
    bool isNumber(char *res, int len);
    int FindCharInArrayRev(char array[], char c, int len);
    long getValue(char *buffer, int maxlen, char startchar, char endchar);
    bool decode_telegram(int len);

    // * Max telegram length
    #define P1_MAXLINELENGTH 1050
    #define BAUD_RATE 115200

    // * Set to store received telegram
    char telegram[P1_MAXLINELENGTH];
    // * Set during CRC checking
    unsigned int currentCRC = 0;

    StaticJsonDocument<1024> actual_values;
    StaticJsonDocument<1024> counters;

public:
    P1Meter();
    bool read_p1_hardwareserial();
    StaticJsonDocument<1024> get_actual_values();
    StaticJsonDocument<1024> get_counters();
};

#endif