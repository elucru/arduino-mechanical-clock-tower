#include <Arduino.h>

/*  Clock tower update - implements serial commands for adjusting the clock time.
    The clock uses a 2HSS57 driver with a NEMA 23 stepper motor and a 50:1 gear ratio box.
    It is set to run the code, one pulse, in approximately 180 milliseconds (two pulses = 1 step), 
    with a pulse width of 5 microseconds, using delay function to control the timing of the pulses:
    - 179 miliseconds between pulses (originally calculated at 180 milliseconds).
    - 984 microseconds for precision.
 *
**/

#define STEPPER_PULSE_PIN                 8   /* Arduino pin for driver motor comand */
#define STEPPER_DIR_PIN                   9   /* Arduino pin for driver motor direction */
#define STEPPER_PULSE_TIME                5   /* Time in microsec for stepper pulse */
#define NORMAL_TIME_CLOCK               179   /* Delay between pulses to set correct time clock, 179 miliseconds (180 miliseconds was calculated originally) */
#define CLOCK_PRECISION                 984   /* Clock precision in microseconds, 984 microseconds to set correct time clock */

#define CW_DIR                         HIGH   /* Clockwise direction */
#define CCW_DIR                         LOW   /* Counterclockwise direction */

void setup() 
{
  // configure pins:
  pinMode(STEPPER_PULSE_PIN, OUTPUT);     /* Set pin for stepper command */
  pinMode(STEPPER_DIR_PIN, OUTPUT);       /* Set pin for stepper direction */
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, CW_DIR);  /* Set direction to clockwise */
  delayMicroseconds(10);                  /* This delay 10 microseconds between set direction and puls command for correct direction set */
  digitalWrite(STEPPER_PULSE_PIN, HIGH);  /* Stepper commands are in common anode set so, high means STOP! */
}

void loop()
{
  digitalWrite(STEPPER_PULSE_PIN, LOW);    /* Start stepper pulse */
  delayMicroseconds(STEPPER_PULSE_TIME);   /* The pulse is on */
  digitalWrite(STEPPER_PULSE_PIN, HIGH);   /* Stop stepper pulse (pulse is off) */
  delay(NORMAL_TIME_CLOCK);                /* Clock function parameter */
  delayMicroseconds(CLOCK_PRECISION);      /* Clock precision */
}
