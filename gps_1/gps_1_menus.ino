#include "gps_1.h"

void wait_button_released()
{
  byte b;
  unsigned long l;
  
  do {
    noInterrupts();
    b = button;
    l = last_button_change;
    interrupts();
    delay(30);
  } while (b || (millis()<l+100));
}

char oui_non() // -1 : non; 0 : time out; 1 : oui
{
  char buffer[17];
  char choice = 1,coord;

  for (byte i=0;i<16;i++) buffer[i]=' ';
  buffer[16]='\0';
  strcpy_P(buffer, (PGM_P)msg_oui_non);
  lcd.setCursor(0,1);
  lcd.print(buffer);
  unsigned long beg = millis(),l;
  byte b;
  
  noInterrupts();
  last_menu_change = millis();
  menu_change = 0;
  interrupts();
  
  wait_button_released();
  
  do {
    noInterrupts();
    b = button;
    l = last_button_change;
    interrupts();
    delay(30);
    if (b && (millis()>l+100)) {
      #ifdef DEBUG
      Serial.print("oui-non=");
      Serial.println((int)choice);
      #endif
      return choice;
    }
    noInterrupts();
    l = last_menu_change;
    b = menu_change;
    interrupts();
    if (millis()>l+100) {
      if (b>0) {
        choice = -1;
        lcd.setCursor(0,1);
        lcd.print(" ");
        coord = 7;
      }
      else if (b<0) {
        choice = 1;
        coord = 0;
        lcd.setCursor(7,1);
        lcd.print(" ");
      }
      noInterrupts();
      menu_change = 0;
      interrupts();
      lcd.setCursor(coord,1);
      lcd.print(">");
    }
    delay(30);
  } while (ms_elapsed_from(beg) < 30000);
  return 0;
}

void err_msg(char * msg, boolean use_delay = true)
{
   char buffer[17];
   
   for (byte i=0;i<16;i++) buffer[i]=' ';
   buffer[16]='\0';
   strcpy_P(buffer, (PGM_P)msg);
   lcd.setCursor(0,0);
   lcd.print(buffer);
   if (use_delay) delay(2000);
}

void menu_add_wp()
{
  if (LOCUS_started) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Adding WP:");
    lcd.print(next_WP);
    addWP(wp_file, next_WP);
    delay(MENU_DELAY);
    in_menu = false;
    next_WP ++;
  } else err_msg(msg_locus_not_sted);
}

void menu_time()
{
  get_rmc();
  lcd.setCursor(6,0);
  print_date(lcd);
  lcd.setCursor(0,1);
  lcd.print("Time ");
  print_time(lcd);
  delay(MENU_DELAY);
}

void menu_start_path()
{
  in_menu = false;
  if (LOCUS_started) {
    err_msg(msg_already_sted);
    return;
  }
  char name[12];
  strcpy_P(name, (PGM_P)wp_file_name);
  wp_file_n = new_filename(name);
  if (createWPFile(name,wp_file)) {
    next_WP = 0;
  } else err_msg(msg_err_file);
  if (!GPS.LOCUS_StartLogger())
  {
    err_msg(msg_err_locus);
    return;
  }
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("New path to file:");
  lcd.setCursor(0,1);
  lcd.print(name);
  delay(MENU_DELAY);
  LOCUS_started = true;
}

void menu_end_path()
{
  if (!LOCUS_started)
  {
    err_msg(msg_locus_not_sted);
    return;
  }
  err_msg(msg_ask_transfer_SD);
  wp_file.close();
  char oui = oui_non();
  if (oui == 1) {
    err_msg(msg_transfering_SD);
    char name[12];
    strcpy_P(name, (PGM_P)trace_file_name);
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(name);
    #ifdef DEBUG
    Serial.println(name);
    #endif
    save_trace(name);
    GPS.sendCommand("$PMTK185,1*23");
    GPS.waitForSentence("$PMTK001,185");    
    err_msg(msg_erasing_mem, false);
    GPS.sendCommand("$PMTK184,1*22");
    GPS.waitForSentence("$PMTK001,184,3");
    // FIXME: Check result
    Serial1.flush();
    LOCUS_started = false;
    delay(MENU_DELAY);
  }  
}
