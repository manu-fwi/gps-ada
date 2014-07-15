// Licence and copyright, see gps_1.ino file

#ifndef __GPS_LIB_H
#define __GPS_LIB_H

#include <Arduino.h>

// 10s timeout
#define GPS_TOUT 10000
#define MAX_LINE_LEN 120

#define GPS_RMC 1
#define GPS_GGA 2

typedef struct _gps_data_t gps_data_t;
struct _gps_data_t{
  byte hours,minutes,sec,month,day, nb_sats, fix;  // 0 = no fix, 1 = fix, 2 = 3D fix;
  uint16_t msec,year,
           altitude; // Altitude in decimeters

// LOCUS parameters
  byte LOCUS_percent;
  uint16_t LOCUS_serial,LOCUS_log_nb;
  boolean LOCUS_logging;
};

boolean gps_wait_for_sentence(const char *,byte max_tries);
void gps_nmea_enable(byte sentences);
boolean gps_get_nmea(byte sentences);
#endif
