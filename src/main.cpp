#include <Arduino.h>

/*  Clock tower update - implements serial commands for adjusting the clock time.
    The clock uses a 2HSS57 driver with a NEMA 23 stepper motor and a 50:1 gear ratio box selected when the compiler switch PRINTED_CLOCK_3D is not defined.
    For the 3D printed clock, it uses a 25:1 gear ratio box, selected through the compiler switch PRINTED_CLOCK_3D when defined.
    It is set to run the code, one pulse, in approximately 180 milliseconds (two pulses = 1 step), 
    with a pulse width of 5 microseconds, using delay function to control the timing of the pulses:
    - 179 miliseconds between pulses (originally calculated at 180 milliseconds).
    - 984 microseconds for precision.
 *
**/

/* This is for testing with 3D printed clock, commented or deleted for normal clock tower */
#define PRINTED_CLOCK_3D 

#ifdef PRINTED_CLOCK_3D
  #define STEPPER_DRIVER_ENABLE_PIN       10  /* Enable/disable the stepper motor driver */
#endif

#define STEPPER_PULSE_PIN                 8   /* Arduino pin for driver motor comand */
#define STEPPER_DIR_PIN                   9   /* Arduino pin for driver motor direction */
#define STEPPER_PULSE_TIME                5   /* Time in microsec for stepper pulse */

#ifdef PRINTED_CLOCK_3D                         /* This clock has 25:1 reduction and 16 x 200 steps per revolution (16 microsteps per full step), one pulse = 1 step */
  #define NORMAL_TIME_CLOCK                44   /* Delay between pulses to set correct time clock, 45 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 968   /* Clock precision in microseconds, 984 microseconds to set correct time clock */
#else                                           /* This clock has 50:1 reduction and 200 steps per revolution, one pulse = 1/2 step */
  #define NORMAL_TIME_CLOCK               179   /* Delay between pulses to set correct time clock, 178 miliseconds (180 miliseconds was calculated originally) */
  #define CLOCK_PRECISION                 984   /* Clock precision in microseconds, 984 microseconds to set correct time clock */
#endif

#define FAST_MOVING_CLOCK                 1   /* Delay between pulses to clock faster */
#define STEPPER_FAST_TIME_ADJUSTMENT    100   /* Time in miliseconds for stepper pulse to adjust the time faster */
#ifdef PRINTED_CLOCK_3D
  #define A_QUARTER                     20000   /* Number of impuls for 15 minute */
#else
  #define A_QUARTER                      5000   /* Number of impuls for 15 minute */
#endif
#ifdef PRINTED_CLOCK_3D
#define CW_DIR                          LOW   /* Clockwise direction */
#define CCW_DIR                        HIGH   /* Counterclockwise direction */
#else
#define CW_DIR                         HIGH   /* Clockwise direction */
#define CCW_DIR                         LOW   /* Counterclockwise direction */
#endif

static void moveClockHands(uint8_t directionToMove, uint32_t minutesToMove);

void setup()
{
  // start serial connection
  Serial.begin(9600);
  
  // configure pins for 3D printed clock:
#ifdef PRINTED_CLOCK_3D
  pinMode(STEPPER_DRIVER_ENABLE_PIN, OUTPUT);
  digitalWrite(STEPPER_DRIVER_ENABLE_PIN, LOW); /* Activate stepper motor driver */
#endif

  // configure pins:
  pinMode(STEPPER_PULSE_PIN, OUTPUT);     /* Set pin for stepper command */
  pinMode(STEPPER_DIR_PIN, OUTPUT);       /* Set pin for stepper direction */
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, CW_DIR);  /* Set direction to clockwise */
  delayMicroseconds(10);                  /* This delay 10 microseconds between set direction and puls command for correct direction set */
  digitalWrite(STEPPER_PULSE_PIN, HIGH);  /* Stepper commands are in common anode set so, high means STOP! */

  Serial.println("Program started ... write f for cw or b for ccw direction, follow by the numbers of hours, minutes and seconds.");
  Serial.println("");
  Serial.println("Example for forwarding 2 hours, 30 minutes and 10 seconds is: " );
  Serial.println("f 2 30 10" );
}

void loop()
{
  if (Serial.available())
  {
    String command = Serial.readStringUntil(' ');
    uint8_t directionToMove = (command == "f") ? CW_DIR : CCW_DIR;
    uint16_t num1 = Serial.parseInt();
    uint16_t num2 = Serial.parseInt();
    uint16_t num3 = Serial.parseInt();

    uint32_t secondsToMove = (num1 * 3600) + (num2 * 60) + num3; /* Convert hours and minutes to seconds */

    Serial.print((directionToMove == CW_DIR) ? "CW" : "CCW");
    Serial.print(" direction for: ");
    Serial.print(secondsToMove);
    Serial.println(" seconds.");

    moveClockHands(directionToMove, secondsToMove);
    
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
static void moveClockHands(uint8_t directionToMove, uint32_t secondsToMove)
{
    uint32_t count = 0;
#ifdef PRINTED_CLOCK_3D
    // uint32_t noOfsteps = (minutesToMove * 1333 + (minutesToMove) / 3); /* Convert minutes to steps and correct lost steps */
    /* Convert seconds to steps and correct lost steps (80.000 de microsteps (1/16) for one hour) */
    uint32_t noOfsteps = ((secondsToMove * (uint32_t)22) + ((secondsToMove / (uint32_t)4) - ((uint32_t)100 * (secondsToMove / (uint32_t)3600))));
#else
    /* Convert seconds to steps and correct lost steps (20.000 impulses for one hour) */
    uint32_t noOfsteps = ((secondsToMove * (uint32_t)5) + (secondsToMove / (uint32_t)4) + ((uint32_t)100 * (secondsToMove / (uint32_t)3600)));
#endif

    digitalWrite(STEPPER_DIR_PIN, directionToMove); /* Set direction */

    for (uint32_t step = 1; step < noOfsteps; step++)
    {
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delayMicroseconds(STEPPER_FAST_TIME_ADJUSTMENT);
        if(!((step) % A_QUARTER))
        { // it is use for feedback: 
          count++;                        //    It count and print quarter of hour 
          Serial.print(count);            //    through serial monitor.
          Serial.print(", ");
        }
    }
    digitalWrite(STEPPER_DIR_PIN, CW_DIR); /* Set direction cw */
}
