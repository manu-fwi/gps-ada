#include <SD.h>

prog_char wp_file_name [] PROGMEM = "WYPNT000.TXT";
prog_char trace_file_name [] PROGMEM = "TRACE000.TXT";

File wp_file;
unsigned int wp_file_n; // number

// Find the next available filename
// using the prefix already stored in name using this format: PREFIX000.SUFFIX
// PREFIX is any char sequence of max 5 chars no digits
// SUFFIX is any char sequence of max 3 chars.
// name must be a buffer of 12 chars

uint16_t new_filename(char * name)
{
  char * p = strchr(name,'0');
  uint16_t j;
 
  for (j = 0; j < 1000; j++) {
    uint16_t i = j;
    p[2] = '0' + i % 10;
    i/=10;
    p[1] = '0' + i % 10;
    p[0] = '0' + i /10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(name)) {
      break;
    }
  }
  return j;
}

// Name must be a 12 chars buffer
// format: PREFIX000.SUFFIX
// PREFIX must NOT contain no digit and is 5 chars at most
// SUFFIX is 3 chars at most
// 000 will be replaced by num

void build_filename(char * name, unsigned int num)
{
  char * p = strchr(name,'0');

  p[2] = num % 10+'0';
  num /= 10;
  p[1] = num % 10+'0';
  p[0] = num /10+'0';
}

void print_date(Print& s)
{
  s.print(GPS.day);
  s.print("/");
  s.print(GPS.month);
  s.print("/");
  s.print(GPS.year);

}

void print_time(Print& s)
{
  s.print(GPS.hour);
  s.print(":");
  if (GPS.minute<10) s.print('0');
  s.print(GPS.minute);
  s.print(":");
  if (GPS.seconds<10) s.print('0');  
  s.print(GPS.seconds);
}

boolean createWPFile(const char * name, File& f)
{
  f = SD.open(name, FILE_WRITE);
  if (f) {
    f.println("GPS: Waypoints file");
    f.print("Date: ");
    print_date(f);
    f.print(" <> Time : ");
    print_time(f);
    return true;
  }
  return false;
}

void addWP(File& f, unsigned int num)
{
  f.print("Waypoint=");
  f.print(num);
  f.print(" Serial #=");
  f.println(GPS.LOCUS_serial);
}

void save_trace(char * name)
{
  byte i;
  unsigned int n_tot, n = 0;
  File f = SD.open(name, FILE_WRITE);
  if (f) {
    GPS.sendCommand("$PMTK622,1*29");
    do {
      do {
        while (!GPS.newNMEAreceived()) GPS.read();
        #ifdef DEBUG
        Serial.println(GPS.lastNMEA());
        #endif
      } while (!strstr(GPS.lastNMEA(),"$PMTKLOX"));
      #ifdef DEBUG
      Serial.println(GPS.lastNMEA());
      #endif
      char * p = strchr(GPS.lastNMEA(),',')+1;
      i = *p - '0';

      if (i==1) {
        f.println(GPS.lastNMEA());
        if (++n%5==0) {
          lcd.setCursor(12,1);
          lcd.print(n*100/n_tot);
          lcd.print('%');
        } 
      } else if (i==0) {
        p = strchr(p,',')+1;
        n_tot = atoi(p);
      }
    } while (i!=2);
    lcd.setCursor(12,1);
    lcd.print("100%");
    Serial1.flush();
    f.close();
  }
}
