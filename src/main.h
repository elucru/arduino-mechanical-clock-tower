/* Last update: 2026-08-16 */
#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <RTClib.h>
#include <EEPROM.h>

/* This is for testing with 3D printed clock, commented or deleted for normal clock tower */
// #define PRINTED_CLOCK_3D 

/* To set the RTC time and date, choose one option from: 
   1. Manual adjust date and time (uncomment the line below)
   2. Automatic adjust date and time with compensation of seconds lost in flashing 
  (uncomment one of the lines below. Choose only one option for adjusting date and time */

// #define MANUAL_ADJUST_DATE_TIME
// #define AUTOMATIC_ADJUST_DATE_TIME_WITH_COMPENSATION
/* and, after choosing the option, uncomment the corresponding line and flash it again */

/* Pins for the stepper motor */
#define STEPPER_PULSE_PIN                	8   /* Arduino pin for driver motor comand */
#define STEPPER_DIR_PIN                 	9   /* Arduino pin for driver motor direction */
#define CONTROL_YELLOW_LED_PIN            7   /* Pin for the yellow control LED */

/* Pins to checks the power supply of the stepper motor and the Arduino power voltage */
#define POWER_DOWN_PIN                		3   /* Power up pin is HIGH, power down pin is LOW */

#define BUTTON_PIN                        6   /* Pin for the user button */

#define DEBOUNCE_DELAY                 	 5000   /* Seconds of debounce delay for the power down check */

#ifdef PRINTED_CLOCK_3D                         /* This clock has 25:1 reduction and 16 x 200 steps per revolution (16 microsteps per full step), one pulse = 1 step */
  #define STEPPER_DRIVER_ENABLE_PIN        10  /* Enable/disable the stepper motor driver */  
  #define NORMAL_TIME_CLOCK                44   /* Delay between pulses to set correct time clock, 45 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 969   /* Clock precision in microseconds (968 original microseconds) to set correct time clock */
  #define CW_DIR                          LOW   /* Clockwise direction */
  #define CCW_DIR                        HIGH   /* Counterclockwise direction */
  #define A_QUARTER                     20000   /* Number of impuls for 15 minute */
  #define STEPPER_FAST_TIME_ADJUSTMENT    200   /* Time in miliseconds for stepper pulse to adjust the time faster */
#else                                           /* This clock has 50:1 reduction and 200 steps per revolution, one pulse = 1/2 step */
  #define NORMAL_TIME_CLOCK               179   /* Delay between pulses to set correct time clock, 178 miliseconds (180 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 876   /* Clock precision in microseconds (984 original microseconds) to set correct time clock */
  #define CW_DIR                         HIGH   /* Clockwise direction */
  #define CCW_DIR                         LOW   /* Counterclockwise direction */
  #define A_QUARTER                      5000   /* Number of impuls for 15 minute */
  #define STEPPER_FAST_TIME_ADJUSTMENT    800   /* Time in miliseconds for stepper pulse to adjust the time faster */
#endif

#define STEPPER_PULSE_TIME                5   /* Time in microsec for stepper pulse */

/* Real seconds needed to physically move the hands through 1 hour worth of correction pulses at the fast
   (catch-up) pulse rate: steps/hour (A_QUARTER * 4) times the per-step delay, divided back to seconds.
   This depends on A_QUARTER and STEPPER_FAST_TIME_ADJUSTMENT, which differ per clock model and can be
   retuned, so it's derived here instead of hardcoded - it always reflects whatever those are set to. */
#define MOVE_COMPENSATION_SEC_PER_HOUR   (((uint32_t)A_QUARTER * 4UL * (STEPPER_PULSE_TIME + STEPPER_FAST_TIME_ADJUSTMENT)) / 1000000UL)

#define EEPROM_MAX_USE_SIZE             1021u   /* 0x03FDu -> Size of EEPROM 1024 from 0 to 1023, and we write 3 Bytes at once, so max use size is 1021u. */

#define UPPER_BYTE(x)                   ((x & 0xFF00) >> 8) /* Get the upper byte of a 16-bit number */
#define LOWER_BYTE(x)                   (x & 0x00FF)        /* Get the lower byte of a 16-bit number */
#define SET_TIME_RECOVER_FLAG(x)        (x | 0x80)          /* Set bit 7 to signal time is ready for recover */
#define CLEAR_TIME_RECOVER_FLAG(x)      (x & 0x7F)          /* Clear bit 7 to signal that time was recovered */
#define CHECK_TIME_RECOVER_FLAG(x)      ((x & 0x80) >> 7)   /* Check bit 7 to signal time is ready for recover */
#define SUCCESS                         0u
#define ERROR                           1u
#define MAX_MILLIS_IN_DELAY            4294962000u /* Maximum value for unsigned long millis() is 4,294,967,295u; power debounce time is 5 sec + 295 ms robustness. */

#endif
/* MAIN_H End of file*/
