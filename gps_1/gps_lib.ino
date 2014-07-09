#include "gps_lib.h"

#include <avr/pgmspace.h>

prog_char PMTK_SET_NMEA_OUTPUT_OFF [] PROGMEM = "$PMTK314,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0*28";

// Index into the buffer that receives gps sentences
byte buf_idx=0;

/* Read the next line coming from the gps and parse it. Erase what was in the buf before, make sure
   you have a copy if you need it.
   The last line is stored in gps_buf (null-terminated without the final \r\n)
   returns true if it got a new valid line and false otherwise
 */

byte parseHex(char hex_digit)
{
  if (isdigit(hex_digit))
    return hex_digit-'0';
  else return (hex_digit-'A')+10;
}

boolean gps_get_new_line()
{
  unsigned long begin_t = millis();
  
  buf_idx = 0;
  sentence_type = 0;
  while ((millis()< begin_t + GPS_TOUT) && (buf_idx<MAX_LINE_LEN)) {
    if (Serial1.available()) {
      char c = Serial1.read();
      if (c=='$')
        // Reset the buffer
        buf_idx = 0;      
      gps_buf[buf_idx++] = c;
      if (gps_buf[buf_idx-1]=='\n') break;
    }
  }
  
  gps_buf[buf_idx]='\0';
  // Sanity checks: checksum etc
  if ((buf_idx<5) || (gps_buf[buf_idx-5]!='*')) return false;
  byte sum = parseHex(gps_buf[buf_idx-4]) << 4;
  sum += parseHex(gps_buf[buf_idx-3]);
    
  // check checksum
  for (byte i=1; i < buf_idx-5; i++) sum ^= gps_buf[i];
  return (sum == 0);
}

/* Returns false if more than max_tries sentences have been downloaded
   or if there was a timeout
 */
boolean gps_wait_for_sentence(const char * wait4, byte max_tries)
{
  unsigned long begin_t = millis();
  
  do {
    if (!gps_get_new_line() && (millis()>begin_t+GPS_TOUT))
      return false;
    max_tries--;
    if (strstr(gps_buf,wait4)!=NULL) return true;
  } while (max_tries>0);
  return false;
}

/* sentences = 0 => disabled
               1 => RMC
               2 => GGA
*/

void gps_nmea_enable(byte sentences)
{
  char command[50];
  byte parity = 0;
 
  strcpy_P(command, PMTK_SET_NMEA_OUTPUT_OFF);
  
  // RMC?
  if (sentences & 0x01) {
    command[11]='1';
    parity++;
  }
  // GGA?
  if (sentences & 0x02) {
    command[15]='1';
    parity++;
  }
  if (parity % 2) command[strlen(command)-1]='9';
  Serial1.println(command);
}

// Parse the time and return a pointer to the next param in the sentence

char * parse_time()
{
  char * p = gps_buf;
  unsigned long l;
  
  p = strchr(p,',')+1;
  l = atol(p);
  gps_data.hours = l / 10000;
  gps_data.sec = l%100;
  l /= 100;
  gps_data.minutes = l % 100;
  p += 7;
  gps_data.msec = atoi(p);
  p = strchr(p,',')+1;
  return p;
}

/* Get the correct sentence which is stored in gps_buf until next call to gps sentences download functions (get_new_line,get_nmea,waitForSentence)
   Parse the sentence and set the gps_data fields accordingly
   Note that we do not parse latitude and longitude as we only use them as strings
 */
 
boolean gps_get_nmea(byte sentences)
{
  char * p;
  unsigned long l;
  byte i;
 
  gps_nmea_enable(sentences);
  if (sentences == GPS_RMC) {
    if (!gps_wait_for_sentence("$GPRMC",5)) return false;
    p = parse_time();
    // Fix
    gps_data.fix = (*p=='A');
    for (i=0;i<7;i++) p = strchr(p,',')+1;
    l = atol(p);
    gps_data.year = 2000+l%100;
    l /= 100;
    gps_data.day = l/100;
    gps_data.month = l%100;
    sentence_type = 1;
  }
  else {
    if (!gps_wait_for_sentence("$GPGGA",5)) return false;
    sentence_type = 2;
    // Time first
    p = parse_time();
    // Jump to the sixth parameter: fix
    
    for (i=0;i<4;i++) p = strchr(p,',')+1;
    gps_data.fix = *p-'0';
    p += 2;
    gps_data.nb_sats = atoi(p);
    p = strchr(p,',')+1;
    p = strchr(p,',')+1;
    gps_data.altitude = strtol(p,&p,10)*10;
    // If there was a decimal, take it into account
    if (*p=='.') gps_data.altitude += *(++p)-'0';
  }
  return true;
}

boolean gps_LOCUS_status()
{
  // Query status
  Serial1.println("$PMTK183*38");
  if (!gps_wait_for_sentence("$PMTKLOG",5)) return false;
  #ifdef DEBUG
  Serial.println("LOCUS_status");
  #endif
  char * p = strchr(gps_buf,',')+1;
  gps_data.LOCUS_serial = (uint16_t)atol(p);
  for (byte i = 0;i<7;i++) p = strchr(p,',')+1;
  gps_data.LOCUS_logging = (*p=='2');
  p = strchr(p,',')+1;
  gps_data.LOCUS_log_nb = (uint16_t) atol(p);
  p = strchr(p,',')+1;  
  gps_data.LOCUS_percent = atoi(p);
  return true;
}

boolean gps_LOCUS_start()
{
  Serial1.println("$PMTK185,0*22");
  return gps_wait_for_sentence("$PMTK001,185,3",5);
}

boolean gps_LOCUS_stop()
{
  Serial1.println("$PMTK185,1*23");
  return gps_wait_for_sentence("$PMTK001,185,3",5);
}

boolean gps_LOCUS_erase_mem()
{
  Serial1.println("$PMTK184,1*22");
  return gps_wait_for_sentence("$PMTK001,184,3",5);
}
