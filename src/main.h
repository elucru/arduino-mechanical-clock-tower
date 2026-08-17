/* Last update: 2026-08-16 */
#ifndef MAIN_H
#define MAIN_H

#include <Arduino.h>
#include <RTClib.h>
#include <EEPROM.h>

/* This is for testing with 3D printed clock, commented or deleted for normal clock tower */
#define PRINTED_CLOCK_3D 

/* To set the RTC time and date, choose one option from: 
   1. Manual adjust date and time (uncomment the line below)
   2. Automatic adjust date and time (uncomment the line below)
   3. Automatic adjust date and time with compensation of 5 seconds lost in flashing (uncomment the lines below)
*/
// #define MANUAL_ADJUST_DATE_TIME
#define AUTOMATIC_ADJUST_DATE_TIME_WITH_COMPENSATION

/* Pins for the stepper motor */
#ifdef PRINTED_CLOCK_3D
  #define STEPPER_DRIVER_ENABLE_PIN        10  /* Enable/disable the stepper motor driver */
#endif
#define STEPPER_PULSE_PIN                 	8   /* Arduino pin for driver motor comand */
#define STEPPER_DIR_PIN                   	9   /* Arduino pin for driver motor direction */

/* Pins to checks the power supply of the stepper motor and the Arduino power voltage */
#define POWER_DOWN_PIN                		3   /* Power up pin is HIGH, power down pin is LOW */

#define DEBOUNCE_DELAY                 	 5000   /* Seconds of debounce delay for the power down check */

#ifdef PRINTED_CLOCK_3D                         /* This clock has 25:1 reduction and 16 x 200 steps per revolution (16 microsteps per full step), one pulse = 1 step */
  #define NORMAL_TIME_CLOCK                44   /* Delay between pulses to set correct time clock, 45 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 906   /* Clock precision in microseconds (968 original microseconds) to set correct time clock */
#else                                           /* This clock has 50:1 reduction and 200 steps per revolution, one pulse = 1/2 step */
  #define NORMAL_TIME_CLOCK               179   /* Delay between pulses to set correct time clock, 178 miliseconds (180 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 625   /* Clock precision in microseconds (984 original microseconds) to set correct time clock */
#endif

#ifdef PRINTED_CLOCK_3D
  #define CW_DIR                          LOW   /* Clockwise direction */
  #define CCW_DIR                        HIGH   /* Counterclockwise direction */
#else
  #define CW_DIR                         HIGH   /* Clockwise direction */
  #define CCW_DIR                         LOW   /* Counterclockwise direction */
#endif

#define STEPPER_PULSE_TIME                5   /* Time in microsec for stepper pulse */

#define FAST_MOVING_CLOCK                 1   /* Delay between pulses to clock faster */
#define STEPPER_FAST_TIME_ADJUSTMENT    100   /* Time in miliseconds for stepper pulse to adjust the time faster */

#ifdef PRINTED_CLOCK_3D
  #define A_QUARTER                     20000   /* Number of impuls for 15 minute */
#else
  #define A_QUARTER                      5000   /* Number of impuls for 15 minute */
#endif

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
