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
  print_unsigned_2_dig(s,gps_data.day);
  s.print("/");
  print_unsigned_2_dig(s,gps_data.month);
  s.print("/");
  print_unsigned_2_dig(s,gps_data.year%100);

}

/* Print an unsigned 2 digits number (from 0 to 99 then) making sure it is always
   2 digits by adding a leading 0 if necessary */
   
void print_unsigned_2_dig(Print& s,unsigned int n)
{
  if (n<10) s.print('0');
  s.print(n);
}

void print_time(Print& s)
{
  print_unsigned_2_dig(s, gps_data.hours);
  s.print(":");
  print_unsigned_2_dig(s, gps_data.minutes);
  s.print(":");
  print_unsigned_2_dig(s, gps_data.sec);
}

boolean createWPFile(const char * name, File& f)
{
  f = SD.open(name, FILE_WRITE);
  if (f) {
    f.println("GPS-ADA: Waypoints file");
    f.print("Date: ");
    print_date(f);
    f.print(" <> Time : ");
    print_time(f);
    f.println();
    return true;
  }
  return false;
}

/* Make sure a recent NMEA sentence (RMC/GGA) has been parsed
   as we use the gps_data members to populate the waypoint coordinates
 */
void addWP(File& f, unsigned int num)
{
  f.print("Waypoint=");
  f.print(num);
  f.print(",LOCUS#=");
  f.print(gps_data.LOCUS_log_nb);
  f.print(",Lat=");
  if (gps_data.fix) {
    update_coords();
    f.print(coord_Lat);
  } else f.print('*');
  f.print(",Long=");
  if (gps_data.fix) f.print(coord_Long);
  else f.print('*');
  f.print(",altitude=");
  if (gps_data.fix) {
    f.print(gps_data.altitude/10);
    f.print('.');
    f.print(gps_data.altitude % 10);
  } else f.print('*');
  f.print(",Time=");
  print_time(f);
  f.println();
}

void save_trace(char * name)
{
  byte i;
  unsigned int n_tot, n = 0;
  File f = SD.open(name, FILE_WRITE);
  if (f) {
    Serial1.println("$PMTK622,1*29");
    do {
      if (!gps_wait_for_sentence("$PMTKLOX",5)) {
        err_msg(msg_err_locus,true);
        return;
      }
      char * p = strchr(gps_buf,',')+1;
      i = *p - '0';

      if (i==1) {
        f.println(gps_buf);
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
