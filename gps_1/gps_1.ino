/* GPS_ADA: implements GPS trace files with waypoints.
 * Copyright (c) 2014 by Emmanuel ALLAUD <eallaud@gmail.com>
 * Based upon previous works: most notably the Adafruit Ultimate GPS library
 * and the various examples for several arduino libraries
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

// include the libraries code:

#include <avr/sleep.h>
#include <avr/pgmspace.h>
#include <LiquidCrystal.h>
#include <SD.h>
#include "gps_1.h"
#include "gps_lib.h"

// GPS_LIB variables

// RX buffer for GPS sentences
char gps_buf[MAX_LINE_LEN+1];

/* Sentence type (ORed bits)
   0 : invalid or not parsed
   1 : RMC, parsed -> time,date,fix
   2 : GGA, parsed -> time,fix,nb_sats
   4 : LOCUS, parsed -> all LOCUS fields
 */

int8_t sentence_type=-1;

/* This struct has all the fields obtained by parsing RMC/GGA sentences.
   You have to know
*/
gps_data_t gps_data;

// Global variables

prog_char menu_0[] PROGMEM = "Waypoint?";
prog_char menu_1[] PROGMEM = "Date";
prog_char menu_2[] PROGMEM = "Deb. Ch.";
prog_char menu_3[] PROGMEM = "Fin Ch.";
prog_char menu_4[] PROGMEM = "Sleep";
prog_char menu_5[] PROGMEM = "Exit";

PROGMEM const char * menus[]={ menu_0,menu_1,menu_2, menu_3, menu_4, menu_5 };

prog_char msg_no_fix [] PROGMEM = "No Fix!";
prog_char msg_locus_not_sted[] PROGMEM = "Trace non act.";
prog_char msg_already_sted [] PROGMEM = "Déjà démar.";
prog_char msg_err_file [] PROGMEM = "Erreur SD";
prog_char msg_err_locus [] PROGMEM = "Erreur GPS log";

prog_char msg_ask_transfer_SD [] PROGMEM = "Transfert trace?";
prog_char msg_oui_non [] PROGMEM = "> OUI    NON";
prog_char msg_transfering_SD [] PROGMEM = "Writing to SD";
prog_char msg_erasing_mem [] PROGMEM = "Erasing mem.";

PROGMEM prog_uchar enc_states[] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};

char coord_Lat[10];
char coord_Long[11];

// Last menu change: either +1 or -1 or 0 if no change
volatile char menu_change = 0;
volatile unsigned long last_menu_change = millis();
unsigned long last_gga = 0;

volatile byte button = 0; // current state
volatile unsigned long last_button_change = millis();
byte butt_p_state = 0;    // last recorded state
unsigned long press_time = 0; // Button press begin time
boolean pressed = false;

char curr_menu= 0,displayed_menu = 0;
boolean in_menu, must_displ_coords = true;
boolean has_to_sleep = false,
        starting = true,
        sleeping = false;
        
unsigned int next_WP = 0;
boolean LOCUS_started = false;
        
unsigned int sleep_tout;

// initialize the library with the numbers of the interface pins
LiquidCrystal lcd(13, 12, 8, 7, 6, 5);                  


//interrupt service routine in sleep mode
// Also used to detect rotary button press/release
ISR(PCINT0_vect)
{
  // Check if we were sleeping
  if (sleeping) {
    sleep_disable ();         // first thing after waking from sleep
    sleeping = false;
  }
  // Else see which pins changed
  else {
    // Button is on PB5
    byte b1 = (PINB >> 5 )& 0x01;
    if (b1!=button) {
      button = b1;
      last_button_change = millis();
    } else {
      // Then it is the A or B channel (rotary encoder)
      static unsigned char old_AB = 0;

      old_AB <<= 2;                   //remember previous state
      // A channel is PB6 and B channel is PB 7
      old_AB |= (( PINB >> 6 ) & 0x03 );  //add current state

      char c = pgm_read_byte_near(enc_states+(old_AB & 0x0f ));
      if (c) {
        menu_change = c;
        last_menu_change = millis();
      }
    }
  }
}

void gps_init()
{
  Serial1.println("$PMTK000*32");
  if (!gps_wait_for_sentence("$PMTK001,0,3*30",5)) {
    #ifdef DEBUG
    Serial.println("GPS Init problem!!");
    #endif
  }

  // Switch off nmea sentences
  gps_nmea_enable(0);
  // Set rate, 1Hz
  Serial1.println("$PMTK220,1000*1F");
  delay(100);
  Serial1.flush();
  // Make sure there is no previous logging
  gps_LOCUS_stop();
}

void lcd_enable(boolean enable)
{
  if (enable) {
    lcd.display();
  } else {
    lcd.noDisplay();
  }
}

void sleepNow ()
{
  lcd_enable(false);
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  noInterrupts ();          // make sure we don't get interrupted before we sleep
  // Disable the interrupts for encoder A-B Channels
  PCMSK0 &= ~(bit (PCINT6)|bit(PCINT7));
  sleep_enable ();          // enables the sleep bit in the mcucr register
  sleeping = true;
  interrupts ();           // interrupts allowed now, next instruction WILL be executed
  sleep_cpu ();            // here the device is put to sleep
  sleep_disable();
  // Reenable the interrupts for encoder A-B Channels
  PCMSK0 |= bit (PCINT6)| bit(PCINT7);
  lcd_enable(true);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Waking up...");
  in_menu = false;
  must_displ_coords = true;
}  // end of sleepNow

void setup() {
 
  //Setup interrupts
  // rotary encoder (button and A-B channels)
  
  for (byte i=9;i<=11;i++) {
    pinMode(i, INPUT);
    digitalWrite (i, HIGH);  // enable pull-up for rotary encoder
  }
  digitalWrite(9,LOW);
  // pin change interrupt
  PCMSK0 |= bit (PCINT5)|bit(PCINT6)|bit(PCINT7);  // want pins 9,10,11
  PCIFR  |= bit (PCIF0);   // clear any outstanding interrupts
  PCICR  |= bit (PCIE0);   // enable pin change interrupts
  
  Serial1.begin(9600);
#ifdef DEBUG
  Serial.begin(57600);
  while(!Serial);
  Serial.println("Debug mode");
#endif

   // Setup lcd
  // set up the LCD's number of columns and rows
  lcd.begin(16, 2);
  // Print a message to the LCD.
  lcd.print("Start up...");
  
  pinMode(A1,OUTPUT);
  if (!SD.begin(A1)) {
    lcd.setCursor(0,1);
    lcd.print("SD not working!!");
  }
  
  // Make sure GPS is ready and set up.
  gps_init();
  
  coord_Lat[9]='\0';
  coord_Long[0]='\0'; // marks that there is no valid coord in there
  coord_Long[10]='\0';
}

unsigned long ms_elapsed_from(unsigned long time)
{
  if (time <= millis())
    return (millis()-time);
  // millis has overflowed in the mean time
  return 0xFFFFFFFF-time+millis();
}

int get_LOCUS_mem()
{
  static int mem = -1;
  static unsigned long last_time = 0;
  
  if ((mem==-1) || (millis()>last_time + 5000)) {
    if (gps_LOCUS_status()) mem = gps_data.LOCUS_percent;
    else mem = -1;
    last_time = millis();
  }
  return mem;
}

void update_coords()
{
  char * p = strchr(gps_buf,',') + 1;
  p = strchr(p,',')+1;
  #ifdef DEBUG
  Serial.print("p=");Serial.println(p);
  #endif
  strncpy(coord_Lat,p,9);
  p += 10;
  coord_Lat[4] = *p; // replaces the . with N or S
  p += 2; // skips the first zero for now
  strncpy(coord_Long,p,10);
  p += 11;
  coord_Long[5] = *(p); // replaces the . with W or E
}

void aff_coords()
{
  // Get new coords no more than every 10s
  if ((last_gga+10000<millis()) || (coord_Long[0]=='\0')) {
    Serial1.flush();
    gps_get_nmea(GPS_GGA);
    last_gga = millis();
    must_displ_coords = true;
    if (!gps_data.fix) {
      // No fix
      strcpy_P(coord_Lat, (PGM_P)msg_no_fix);
      coord_Long[0]='\0';
      #ifdef DEBUG
      Serial.print("Lat=");
      Serial.println(coord_Lat);
      #endif
    }
    else update_coords();
  }
  if (must_displ_coords) {
    must_displ_coords = false;
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(coord_Lat);
    if (LOCUS_started) lcd.print(" *");
    else lcd.print("  ");
    int mem = get_LOCUS_mem();
    if (mem>=0) lcd.print(mem);
    else lcd.print("--");
    lcd.print("%");
    lcd.print(gps_data.nb_sats);
    lcd.setCursor(0,1);
    lcd.print(coord_Long);
    lcd.print(" ");
    lcd.print(gps_data.altitude/10);
    #ifdef DEBUG
    Serial.print(must_displ_coords);
    Serial.print(" coords=");
    Serial.print(coord_Lat);
    Serial.print(" * ");
    Serial.println(coord_Long);
    #endif
  }
}

void display_menu()
{
  char buffer[17];
  
  lcd.clear();
  // set the cursor to column 0, line 0
  lcd.setCursor(0, 0);
  // print the number of seconds since reset:
  strcpy_P(buffer, (PGM_P)pgm_read_word(menus+displayed_menu));
  
  lcd.print(buffer);
  lcd.setCursor(0, 1);
}

void button_pressed()
{
  #ifdef DEBUG
  Serial.println("Button pressed!");
  #endif
  if (in_menu) {
    switch(displayed_menu) {
      case Menu_WP:
        menu_add_wp();
        break;
      case Menu_Time:
        menu_time();
        break;
      case Menu_Start_Path:
        menu_start_path();
        break;
      case Menu_End_Path:
        menu_end_path();
        break;
      case Menu_Sleep:
        has_to_sleep = true;
        break;
      case Menu_Exit:;
    }
    in_menu = false;
    must_displ_coords = true;
    return;
  }
  in_menu = true;
  // To make sure we display the menu
  displayed_menu = MENU_MAX+1;
}

void loop() {
  if (in_menu) {
    if (displayed_menu!=curr_menu) {
      displayed_menu = curr_menu;
      display_menu();
      sleep_tout = SLEEP_MENU_TOUT;
    }
    noInterrupts();
    char m = menu_change;
    unsigned long l = last_menu_change;
    interrupts();
    // Check if the menu has changed
    if (m && (millis()>l+100)) {
      curr_menu += m;
      if (curr_menu > MENU_MAX) curr_menu = 0;
      if (curr_menu < 0) curr_menu = MENU_MAX;
      noInterrupts();
      menu_change = 0;
      interrupts();    
    }
  } else {
    aff_coords();
    sleep_tout = SLEEP_TOUT;
  }

  // Check button state
  noInterrupts();
  unsigned long l = last_button_change;
  byte b = button;
  interrupts();
  if (millis()>l+100) {
    if (b) {
      if (millis()>l+200)
        pressed = true;
    } else if (pressed) {
      button_pressed();
      pressed = false;
    }
  }
  
  // Check if it is time to sleep
  if (!has_to_sleep)
    has_to_sleep = (ms_elapsed_from(last_menu_change)/1000>sleep_tout)
                 &&(ms_elapsed_from(last_button_change)/1000>sleep_tout);
  if (has_to_sleep) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Sleeping");
    sleepNow();
    #ifdef DEBUG
    Serial.println("Out of sleep");
    #endif
    starting = true;
    noInterrupts();
    last_button_change = millis();
    interrupts();
    has_to_sleep = false;
  }
  delay(30);
}

