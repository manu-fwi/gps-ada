// Licence and copyright, see gps_1.ino file
   // DEFINES


#ifndef __GPS_1_H

#define __GPS_1_H

//#define DEBUG 1

#define Menu_WP         0
#define Menu_Time       1
#define Menu_Start_Path 2
#define Menu_End_Path   3
#define Menu_Sleep      4
#define Menu_Exit       5

#define MENU_MAX 5

#define MENU_DELAY 2000

#define DELAY_ERR_MSG 2000

// Timeout when in menu: 2 mn
#define SLEEP_MENU_TOUT 120

// Timeout when displaying coords: 1mn
#define SLEEP_TOUT 60

// Power saving timeout in ms
#define PWR_SAVE_TOUT 5000

void err_msg(char * msg,boolean use_delay);

#endif
