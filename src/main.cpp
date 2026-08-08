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
#define FAST_MOVING_CLOCK                 1   /* Delay between pulses to clock faster */
#define STEPPER_FAST_TIME_ADJUSTMENT    100   /* Time in miliseconds for stepper pulse to adjust the time faster */
#define ONE_MINUTES_STEPS               333   /* Number of impuls for one minute + 20 impuls for one hour (lose in 1 hour) */

#define CW_DIR                         HIGH   /* Clockwise direction */
#define CCW_DIR                         LOW   /* Counterclockwise direction */

static void moveClockHands(uint8_t directionToMove, uint16_t minutesToMove);

void setup()
{
  // start serial connection
  Serial.begin(9600);
  Serial.println("Program started ... write f for cw or b for ccw direction, follow by the numbers of hours and minutes.");
  Serial.println("");
  Serial.println("Example forward for 2 hours and 30 minutes is: " );
  Serial.println("f 2 30" );
  
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
  if (Serial.available())
  {
    String command = Serial.readStringUntil(' ');
    uint8_t directionToMove = (command == "f") ? CW_DIR : CCW_DIR;
    uint16_t num1 = Serial.parseInt();
    uint16_t num2 = Serial.parseInt();
    
    uint16_t minutesToMove = (num1 * 60) + num2; /* Convert hours and minutes to seconds */

    Serial.print((directionToMove == CW_DIR) ? "CW" : "CCW");
    Serial.print(" direction for: ");
    Serial.print(minutesToMove);
    Serial.println(" minutes.");

    moveClockHands(directionToMove, minutesToMove);
    
    Serial.println("");
    Serial.println("Done!\nCW direction from now!");

    while (Serial.available() > 0) {
      Serial.read();
    }
  }

  digitalWrite(STEPPER_PULSE_PIN, LOW);    /* Start stepper pulse */
  delayMicroseconds(STEPPER_PULSE_TIME);   /* The pulse is on */
  digitalWrite(STEPPER_PULSE_PIN, HIGH);   /* Stop stepper pulse (pulse is off) */
  delay(NORMAL_TIME_CLOCK);                /* Clock function parameter */
  delayMicroseconds(CLOCK_PRECISION);      /* Clock precision */
}

/***************************************************************************************************************
 * @brief Move the clock hands fast using the stepper motor.
 * @param directionToMove: Direction to move the clock hands (CW or CCW).
 * @param minuteToStep: Number of minutes to move the clock hands.
 * @return None
 * @note This function moves the clock hands very fast using the stepper motor in the specified
 *       direction for the specified number of minutes to set the clock hands correctly.
 *      The function calculates the number of steps required and sends pulse signals to the stepper motor driver.
 */
static void moveClockHands(uint8_t directionToMove, uint16_t minutesToMove)
{/* 20.000 de microsteps for one turn */
    uint16_t count = 0;
    uint16_t noOfsteps = (minutesToMove * 333 + (minutesToMove) / 3); /* Convert minutes to steps and correct lost steps */
    
    digitalWrite(STEPPER_DIR_PIN, directionToMove); /* Set direction */

    for (uint32_t step = 1; step < noOfsteps; step++)
    {
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delayMicroseconds(STEPPER_FAST_TIME_ADJUSTMENT);
        if(!((step) % ONE_MINUTES_STEPS))
        { // it is use for feedback: 
          count++;                        //    it count and print minutes 
          Serial.print(count);            //    through serial monitor.
          Serial.print(", ");
        }
    }
    digitalWrite(STEPPER_DIR_PIN, CW_DIR); /* Set direction cw */
}