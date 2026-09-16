/*  Tower clock update - implements automatic time adjustment after power supply is restored.
    It uses an RTC to keep the exact time and write to EEPROM the moment of power failure (connected: SCL -> A5, SDA -> A4).
    The power failure sensor is a voltage divider with resistors, calculated to have a 4,5V output, 
      mounted on the output of the stepper motor driver power supply 36V.
    It is set to run the code, one pulse, in approximately 180 milliseconds (two pulses = 1 step), on original clock tower, 
      and 45 milliseconds (1/16 step, 200 pulses = 1 hour) on 3D printed clock tower, 
    with a pulse width of 5 microseconds, using delay function to control the timing of the pulses.

    On serial monitor for manual time adjustment write f for cw or b for ccw direction, follow by the numbers of hours, 
      minutes and seconds. Example for forwarding 2 hours, 30 minutes and 10 seconds is:
          f 2 30 10      
      hit enter and the clock hands will move fast to the new time.
    Every command must be followed by 3 numbers, even for commands that ignore them (ret, rtc, reset), e.g. "ret 0 0 0":
      without them Serial.parseInt() blocks waiting for input and readStringUntil(' ') keeps the trailing newline.
 *
**/

#include <Arduino.h>
#include <avr/wdt.h>
#include <main.h>

/* Define global variables */
static const char daysOfTheWeek[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
static bool saveTimeToEEPROM = false;           /* true (1) means hour and minutes was saved to EEPROM, false (0) means not saved */
static boolean powerDown = false;               /* false (0) means Clock has power, true (1) means power down detected - Clock has no power */
static uint8_t writeToEEPROMStatus = SUCCESS;   /* Status of the write operation ERROR (1) means that the EEPROM is failing, broken or burned. */

RTC_DS3231 rtc;

/* Functions prototipes */
static void handlePowerDown(void);
static void handlePowerUp(void);
static void saveTime(void);
static void resetCounters(void);
static void moveClockHands(uint8_t directionToMove, uint32_t minutesToMove);
static void printFormatedDateAndTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year);
static uint8_t setClock(void);
static uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, uint8_t currentDay, uint8_t currentMonth, uint8_t currentYear);
static void processSerialCommand();
static void resetEEPROM(void);
static void softwareReset(void);

/***************************************************************************************************************
 * @brief Setup function initializes the RTC, EEPROM, and uC pin modes.
 * @param None
 * @return None
 * @note This function is called once at start/reset to set up the necessary components and settings.
****************************************************************************************************************/
void setup(void)
{
  /* Initialize serial communication and Wait to be established. */
  Serial.begin(9600);

  /* Check if the RTC work properly, otherwhise it will stop the program to run. */
  if(!rtc.begin())
  {
      Serial.println("Couldn't find RTC!");
      Serial.flush();
      while(1)
      {/* Stop the program if RTC is not found */
          if(rtc.begin())
          {/* Start the program if RTC is found */
              Serial.println("RTC OK now!");
              break;
          }
      }
  }

/* Set the RTC time and date */
#if defined(MANUAL_ADJUST_DATE_TIME)
  rtc.adjust(DateTime(2026, 9, 15, 09, 10, 11));
#elif defined(AUTOMATIC_ADJUST_DATE_TIME_WITH_COMPENSATION)
  DateTime compileTime(F(__DATE__), F(__TIME__));
  DateTime adjustedTime = compileTime + TimeSpan(0, 0, 0, 10);  /* or automatic adjust date and time and compensate 5 seconds lost in flashing */
  rtc.adjust(adjustedTime);
#else
  // No adjustment, use the existing RTC time
#endif

  /* Turn off the built-in LED to save some energy ... :) */
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  /* Prepare the yellow control LED to signal status (e.g., motor running for adjustment)*/
  pinMode(CONTROL_YELLOW_LED_PIN, OUTPUT);   
  digitalWrite(CONTROL_YELLOW_LED_PIN, LOW);
  /* Prepare the button for user input, for future use */
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  /* Stepper and power down setup */
  pinMode(POWER_DOWN_PIN, INPUT);
#ifdef PRINTED_CLOCK_3D
  pinMode(STEPPER_DRIVER_ENABLE_PIN, OUTPUT);   /* configure pins for 3D printed clock */
  digitalWrite(STEPPER_DRIVER_ENABLE_PIN, LOW); /* activate 3D stepper motor driver */
#endif

  // configure pins:
  pinMode(STEPPER_PULSE_PIN, OUTPUT);     /* Set pin for stepper command */
  pinMode(STEPPER_DIR_PIN, OUTPUT);       /* Set pin for stepper direction */
  
  // set pins accordingly to 2HSS57 driver specs:
  digitalWrite(STEPPER_DIR_PIN, CW_DIR);  /* Set direction to clockwise */
  delayMicroseconds(10);                  /* This delay 10 microseconds between set direction and puls command for correct direction set */
  digitalWrite(STEPPER_PULSE_PIN, HIGH);  /* Stepper commands are in common anode set so, high means STOP! */

  /* EEPROM is used for saving date and time in case of power down. */
  /* Read index from EEPROM for robustness and to verify if was an reset or power down. */
  uint16_t index = (EEPROM[0] << 8) | EEPROM[1]; 

  /* Check if index exceeds EEPROM size */
  if(index > EEPROM_MAX_USE_SIZE)
  {/* Reset EEPROM if index exceeds limit (1024) or is a new uC. */
      resetCounters();
      EEPROM[1u] = 4u;/* This will be incremented with 3 at first power down, so time will be saved in EEPROM[7]! */
  }
  else
  {/* Check if time is allready saved in EEPROM. 
      In case of uC reset don't save time in EEPROM again, just normal clock action. */
      uint8_t recoveryFlag = CHECK_TIME_RECOVER_FLAG(EEPROM[index]);
      
      if(false != recoveryFlag)
      { /* Check if the MSB bit is set, indicating that time was not recovery yet. */
          saveTimeToEEPROM = true;
          powerDown = true; /* Set powerDown to true to avoid immediate power down handling */
          Serial.println("recoveryFlag on, time is ready to recover!");

      }
  }

  Serial.println("Program started ... write f for cw or b for ccw direction, follow by the numbers of hours, minutes and seconds like:");
  Serial.println("  f 2 30 10" );
  Serial.println("Or other commands follow by zeros like:");
  Serial.println("  ret 0 0 0; rtc 0 0 0; reset 0 0 0.");
}

/***************************************************************************************************************
 * @brief Main loop function: checks for power down and handles clock and clock adjustments.
 * @param None
 * @return None
 * @note This function continuously checks the power state and handles power down and power up events.
 *       It also includes a debounce mechanism to avoid false triggers during power fluctuations.
 ****************************************************************************************************************/
void loop(void)
{
  static uint32_t lastDebounceTime = 0;
  static uint8_t lastPowerCheckState = HIGH;

  uint8_t currentPowerCheckState = digitalRead(POWER_DOWN_PIN);

  /* Handle debounce logic */
  if(currentPowerCheckState != lastPowerCheckState)
  {/* The POWER_DOWN_PIN state has been changed, start monitoring the duration. */
    lastDebounceTime = millis();
    if (lastDebounceTime >= MAX_MILLIS_IN_DELAY)
    {/* If is grather than 4,294,962,000u (49 days, 17 hours, 2 minutes and 42 sec) will be an overflow in 5 seconds, */
        lastDebounceTime = 0u; /* so reset the debounce time. */
    }  
  }

  if((millis() - lastDebounceTime) > DEBOUNCE_DELAY)
  {/* Only the change above the DEBOUNCE_DELAY threshold is taken into account, to avoid spikes and shorts power downs. */
    powerDown = (currentPowerCheckState == LOW);
  }
  /* Update the POWER_DOWN_PIN state. */
  lastPowerCheckState = currentPowerCheckState;

  if(SUCCESS == writeToEEPROMStatus)
  {/* If the last save in EEPROM is corrupted or unusable the clock stops. */
    if (true == powerDown)
    {
        handlePowerDown();
    }
    else
    {
        handlePowerUp();
    }
  }
  else
  {
      Serial.println("EEPROM write or RTC read failed or corrupted.");
  }

  if(Serial.available())
  {
    processSerialCommand();
  }

  /* Holding the button for BUTTON_RESET_HOLD_TIME triggers a full software reset of the microcontroller. */
  static uint32_t buttonPressStart = 0u;
  static bool buttonHeld = false;

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    if (!buttonHeld)
    {
        buttonHeld = true;
        buttonPressStart = millis();
    }
    else if ((millis() - buttonPressStart) >= BUTTON_RESET_HOLD_TIME)
    {
        Serial.println("Buton apasat: se reporneste microcontrolerul...");
        Serial.flush();
        softwareReset();
    }
  }
  else
  {
    buttonHeld = false;
  }
}

/***************************************************************************************************************
 * @brief Handle power down events and save the current time to EEPROM.
 * @param None
 * @return None
 * @note This function checks if the time is allready saved in EEPROM otherwhise it saves it.
 */
static void handlePowerDown(void)
{
    if (true != saveTimeToEEPROM)
    {
        saveTime(); /* Save RTC data to EEPROM */
    }
}

/***************************************************************************************************************
 * @brief Save the current date and time in EEPROM.
 * @param None
 * @return None
 * @note This function saves the current hour and minutes and seconds to EEPROM at the specified index.
 *  It also saves the current day, month, and year to fixed location in EEPROM if are different from what is
 *   allready saved.
 * Also it sets a flag to indicate that time has been saved to EEPROM and not recover yet. When the power is up,
 *  the flag is reset.
 */
static void saveTime(void)
{
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];
    uint16_t countResetIdx = (EEPROM[2] << 8) | EEPROM[3];
    uint8_t currentHour = currentTime.hour();
    uint8_t currentMinute = currentTime.minute();
    uint8_t currentSecond = currentTime.second();
    uint8_t currentDay = currentTime.day();
    uint8_t currentMonth = currentTime.month();
    uint8_t currentYear = currentTime.year();

    if ((currentHour >= 24) || (currentMinute >= 60) || (currentSecond >= 60))
    { /* Retry once: an out-of-range value is more likely a transient I2C glitch than a permanent fault. */
        currentTime = rtc.now();
        currentHour = currentTime.hour();
        currentMinute = currentTime.minute();
        currentSecond = currentTime.second();
        currentDay = currentTime.day();
        currentMonth = currentTime.month();
        currentYear = currentTime.year();
    }

    if ((currentHour >= 24) || (currentMinute >= 60) || (currentSecond >= 60))
    { /* Still invalid after retry: don't persist a bogus time. */
        writeToEEPROMStatus = ERROR;
        return;
    }

    index += 3u; /* Increment index to save the current time */

    if (index > EEPROM_MAX_USE_SIZE)
    {/* Check if index exceeds EEPROM size */
        resetCounters(); /* Reset EEPROM if index exceeds limit (1024 - 2 Bytes) */
        index = 7u; /* Reset index to EEPROM[7] where start the savings. This will be incremented with 3 at first power down, so time will be saved in EEPROM[7] */
        countResetIdx++; /* Increment the counter for reset index */
        EEPROM[1u] = index; /* Write new index to EEPROM */
        EEPROM[2u] = UPPER_BYTE(countResetIdx); /* Write upper byte of index */
        EEPROM[3u] = LOWER_BYTE(countResetIdx); /* Write lower byte of index */
    }

    currentYear -= 2000u; /* Convert year to 2-digit format */
/* Use the EEPROM date if it is the same to avoid overwriting the EEPROM */
    if (currentDay == EEPROM[4u])
    {
        currentDay = 0u; /* Use the day from EEPROM if it matches */
    }

    if (currentMonth == EEPROM[5u])
    {
        currentMonth = 0u; /* Use the month from EEPROM if it matches */
    }

    if (currentYear == EEPROM[6u])
    {
        currentYear = 0u; /* Use the year from EEPROM if it matches */
    }
    /* Set the MSB bit 8 to signal time is ready for recover */
    currentHour = SET_TIME_RECOVER_FLAG(currentHour); 
    /* Write the data in EEPROM and check the status of writing. */
    writeToEEPROMStatus = writeToEEPROM(index, currentHour, currentMinute, currentSecond, currentDay, currentMonth, currentYear); /* Write current time to EEPROM */
    /* Only mark as saved if the write actually succeeded, otherwise the flag would lie about the EEPROM content. */
    if (SUCCESS == writeToEEPROMStatus)
    {
        saveTimeToEEPROM = true;
    }
}

/***************************************************************************************************************
 * @brief Write time to EEPROM with error handling and verification.
 * @param index: EEPROM index where the time is saveing.
 * @param currentHour: hour when power down was detected.
 * @param currentMinute: minute when power down was detected.
 * @param currentSecond: second when power down was detected.
 * @param currentDay: day when power down was detected.
 * @param currentMonth: month when power down was detected.
 * @param currentYear: year when power down was detected.
 * @return status of the write operation:
 *                  0: EEPROM write successful,
 *                  1: failed to write in EEPROM.
 *
 * @note This function writes the current hour, minutes and seconds to the EEPROM at the specified index.
 * It also writes the upper and lower bytes of the index to the first two EEPROM locations, and the current day,
 * month, and year to fixed locations in the EEPROM if they are not zero (different from the previous one).
 * The function checks for successful write operations by reading it back to verify that the value
 *  was written correctly and prints error messages if any write fails if DEBUGGING is enabled.
 *
 * The function uses the EEPROM library to write data to the EEPROM.
 */
static uint8_t writeToEEPROM(uint16_t index, uint8_t currentHour, uint8_t currentMinute, uint8_t currentSecond, uint8_t currentDay, uint8_t currentMonth, uint8_t currentYear)
{
    /* Write upper and lower bytes of the index */
    EEPROM.write(0u, UPPER_BYTE(index));
    if (EEPROM.read(0u) != UPPER_BYTE(index))
    {
        Serial.println("Error: Failed to write upper byte of index to EEPROM, status ERROR. ");

        return ERROR;
    }

    EEPROM.write(1u, LOWER_BYTE(index));
    if (EEPROM.read(1u) != LOWER_BYTE(index))
    {
        Serial.println("Error: Failed to write lower byte of index to EEPROM, status ERROR. ");

        return ERROR;
    }

    /* Write current hour */
    EEPROM.write(index, currentHour);
    if (EEPROM.read(index) != currentHour)
    {
        Serial.println("Error: Failed to write current hour to EEPROM, status ERROR. ");

        return ERROR;
    }

    /* Write current minute next to hour */
    EEPROM.write(index + 1u, currentMinute);
    if (EEPROM.read(index + 1u) != currentMinute)
    {
        Serial.println("Error: Failed to write current minute to EEPROM, status ERROR. ");

        return ERROR;
    }

    /* Write current seconds next to minutes */
    EEPROM.write(index + 2u, currentSecond);
    if (EEPROM.read(index + 2u) != currentSecond)
    { 
        Serial.println("Error: Failed to write current seconds to EEPROM, status ERROR. ");

        return ERROR;
    }

    if (currentDay != 0u)
    {
        Serial.println("Write current day to EEPROM, status OK. ");
        /* Write current day */
        EEPROM.write(4u, currentDay);
        if (EEPROM.read(4u) != currentDay)
        {
            Serial.println("Error: Failed to write current day to EEPROM, status ERROR. ");

            return ERROR;
        }
    }

    if (currentMonth != 0u)
    {
        /* Write current month */
        Serial.println("Write current month to EEPROM, status Ok. ");
        EEPROM.write(5u, currentMonth);
        if (EEPROM.read(5u) != currentMonth)
        {
            Serial.println("Error: Failed to write current month to EEPROM, status ERROR. ");

            return ERROR;
        }
    }

    if (currentYear != 0u)
    {
        /* Write current year */
        Serial.println("Write current year to EEPROM, status Ok. ");
        EEPROM.write(6u, currentYear);
        if (EEPROM.read(6u) != currentYear)
        {
            Serial.println("Error: Failed to write current year to EEPROM, status ERROR. ");

            return ERROR;
        }
    }

    Serial.println("EEPROM write successful.");

    return SUCCESS; /* Return success status */
}

/***************************************************************************************************************
 * @brief Reset the EEPROM counter to 0.
 * @param None
 * @return None
 * @note This function resets the upper and lower bytes of the index and the data location in EEPROM to 0.
 *       It is used to clear the EEPROM data when the index exceeds the maximum size.
 ***************************************************************************************************************/
static void resetCounters(void)
{
    for (uint8_t i = 0; i < 7u; i++)
    {
        EEPROM.write(i, 0);/* Reset index, counter and date.*/
    }
}

/***************************************************************************************************************
 * @brief Handle power-up events and adjust the clock hands accordingly.
 * @param None
 * @return None
 * @note This function checks if the power is up and adjusts the clock hands using the stepper motor.
 *       If the power is down, it saves the current time to EEPROM.
 ***************************************************************************************************************/
static void handlePowerUp(void)
{
    if (false == saveTimeToEEPROM)
    { /* Normal clock function */
        digitalWrite(STEPPER_PULSE_PIN, HIGH);  /* Stop stepper pulse (pulse is off) */
        delayMicroseconds(STEPPER_PULSE_TIME);  /* The pulse is on */
        digitalWrite(STEPPER_PULSE_PIN, LOW);   /* Start stepper pulse */ 
        delay(NORMAL_TIME_CLOCK);                /* Clock function parameter */
        delayMicroseconds(CLOCK_PRECISION);      /* Clock precision */
    }
    else
    { /* Adjust clock using EEPROM data */
        if (!powerDown)
        { /* Check if the power is up and signal if the clock was set correctly. */
            uint8_t setClockFlag = SUCCESS;

            setClockFlag = setClock();

            if (SUCCESS != setClockFlag)
            {/* Set error status */
                writeToEEPROMStatus = ERROR; 
            }
        }
    }
}

/***************************************************************************************************************
 * @brief Adjust the clock hands using the saved time from EEPROM.
 * @param None
 * @return ERROR if the saved hour is invalid, otherwise SUCCESS.
 * @note This function retrieves the current time from the RTC and compares it with the saved time
 *        in EEPROM. It calculates the difference in hours, minutes and seconds, and moves the clock hands
 *        accordingly using the stepper motor.
 *       The function also resets the stepper motor direction to default (CW), resets the flag indicating 
 *        that time recovery is complete and compensates the time spent moving the clock hands.
 ***************************************************************************************************************/
static uint8_t setClock(void)
{ /* Adjust clock using EEPROM data */
    DateTime currentTime = rtc.now();
    uint16_t index = (EEPROM[0] << 8) | EEPROM[1];  /* Read index from EEPROM where the saved time is stored. */
    uint8_t currentHour = currentTime.hour();       /* Read current hour from RTC */
    uint8_t currentMinute = currentTime.minute();   /* Read current minute from RTC */
    uint8_t currentSecond = currentTime.second();   /* Read current second from RTC */
    uint8_t savedHour = EEPROM[index];          /* Read saved hour from EEPROM */
    uint8_t savedMinute = EEPROM[index + 1];    /* Read saved minute from EEPROM */
    uint8_t savedSecond = EEPROM[index + 2];    /* Read saved seconds from EEPROM */
    uint8_t directionToMove = CW_DIR;
    uint16_t minuteToStep = 0;
    uint16_t secondsToStep = 0;

    if ((currentHour >= 24) || (currentMinute >= 60) || (currentSecond >= 60))
    { /* Retry once: an out-of-range value is more likely a transient I2C glitch than a permanent fault. */
        currentTime = rtc.now();
        currentHour = currentTime.hour();
        currentMinute = currentTime.minute();
        currentSecond = currentTime.second();

        if ((currentHour >= 24) || (currentMinute >= 60) || (currentSecond >= 60))
        { /* Still invalid after retry: abort instead of computing a correction from a bogus current time. */
            return ERROR;
        }
    }

    /* Clear the MSB bit of the saved hour to indicate recovery is complete */
    savedHour = CLEAR_TIME_RECOVER_FLAG(savedHour);

    /* Robustness for a new EEPROM (all 0xFFu) or a corrupted EEPROM read */
    if ((savedHour >= 24) || (savedMinute >= 60) || (savedSecond >= 60))
    { /* Invalid saved time: abort instead of moving the hands from a fabricated baseline. */
        return ERROR;
    }

    /* Save hour after clear the recovery flag to mark that recovery was done. */
    EEPROM[index] = savedHour;

    /* Convert to 12-hour format */
    savedHour = (savedHour == 0) ? 12 : (savedHour > 12 ? savedHour - 12 : savedHour);
    currentHour = (currentHour == 0) ? 12 : (currentHour > 12 ? currentHour - 12 : currentHour);

    /* Calculate hour difference */
    uint8_t hourDifference = (currentHour >= savedHour) ? currentHour - savedHour : savedHour - currentHour;

    if (hourDifference >= 6)
    {
        hourDifference = 12 - hourDifference;
        directionToMove = (currentHour > savedHour) ? CCW_DIR : CW_DIR;
    }
    else
    {
        directionToMove = (currentHour >= savedHour) ? CW_DIR : CCW_DIR;
    }

    /* Convert the difference from hours to minutes. */
    minuteToStep = hourDifference * 60;
    /* Calculate minute difference */
    uint8_t minuteDifference = (currentMinute >= savedMinute) ? currentMinute - savedMinute : savedMinute - currentMinute;

    if (directionToMove == CW_DIR)
    {
        if (currentMinute >= savedMinute)
        {
            minuteToStep = minuteToStep + minuteDifference;
        }
        else
        {
            if (0u == minuteToStep)
            {
                directionToMove = CCW_DIR;
            }
            
            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        
    }
    else
    {
        if (currentMinute >= savedMinute)
        {
            if (0u == minuteToStep)
            {
                directionToMove = CW_DIR;
            }

            minuteToStep = (minuteToStep >= minuteDifference) ? minuteToStep - minuteDifference : minuteDifference - minuteToStep;
        }
        else
        {
            minuteToStep = minuteToStep + minuteDifference;
        }
    }

    /* Calculate seconds difference */
    uint8_t secondsDifference = (currentSecond >= savedSecond) ? currentSecond - savedSecond : savedSecond - currentSecond;
    /* Convert minutes to seconds */
    secondsToStep = minuteToStep * 60;

    /* If the saved second is greater than the current second, adjust the seconds to step */
    if (directionToMove == CW_DIR)
    {
        if (savedSecond > currentSecond)
        {
            if (0u == secondsToStep)
            {
                directionToMove = CCW_DIR; /* Change direction to CCW if no seconds to step */
            }

            secondsToStep = (secondsToStep >= secondsDifference) ? secondsToStep - secondsDifference : secondsDifference - secondsToStep; /* Subtract seconds if moving clockwise */
        }
        else
        {
            secondsToStep = secondsToStep + secondsDifference; /* Add seconds if moving clockwise */
        }
    }
    else
    {
        if (savedSecond > currentSecond)
        {
            secondsToStep = secondsToStep + secondsDifference; /* Add seconds if moving clockwise */
        }
        else
        {
            if (0u == secondsToStep)
            {
                directionToMove = CW_DIR; /* Change direction to CW if no seconds to step */
            }

            secondsToStep = (secondsToStep >= secondsDifference) ? secondsToStep - secondsDifference : secondsDifference - secondsToStep; /* Subtract seconds if moving counterclockwise */
        }
    }

    /* Compensate the time needed to set the clock: real time keeps elapsing while moveClockHands() executes,
       proportional to the total movement (not just whole hours), so scale by the full secondsToStep here,
       plus 1 second of margin for the fixed overhead (RTC read, single EEPROM byte write) before the move
       starts - measured at only a few milliseconds, so 1 whole second already gives ample safety margin.
       This must always be added towards CW (forward in time): when the shorter path is CCW, the compensation
       has to be subtracted from the CCW amount (or flipped to a small CW move) instead of added to it, otherwise
       the clock hands end up overshooting by roughly twice the compensation whenever a CCW correction is used. */
    uint16_t stepCompensation = (uint16_t)(((uint32_t)secondsToStep * MOVE_COMPENSATION_SEC_PER_HOUR) / 3600UL) + 1u;

    if (directionToMove == CW_DIR)
    {
        secondsToStep += stepCompensation;
    }
    else if (secondsToStep >= stepCompensation)
    {
        secondsToStep -= stepCompensation;
    }
    else
    {
        secondsToStep = stepCompensation - secondsToStep;
        directionToMove = CW_DIR;
    }

    /* Print the actual time the clock hands will be moved (after compensation), not the raw component differences. */
    printFormatedDateAndTime(secondsToStep / 3600u, (secondsToStep % 3600u) / 60u, secondsToStep % 60u, 0u, 0u, 0u);
    Serial.println((directionToMove == CW_DIR) ? "Direction: CW" : "Direction: CCW");

    /* Move the hands of the clock with the respective secondes. */
    moveClockHands(directionToMove, secondsToStep);
    /* Reset the stepper motor direction to default (CW) */
    digitalWrite(STEPPER_DIR_PIN, CW_DIR);
    /* Reset the flag indicating time recovery is complete */
    saveTimeToEEPROM = false;

    Serial.print("Clock adjustment complete! ");

    return SUCCESS;
}

/***************************************************************************************************************
 * @brief Move the clock hands fast for a specified number of seconds.
 * @param directionToMove: Direction to move the clock hands (CW or CCW).
 * @param secondsToMove: Number of seconds to move the clock hands.
 * @return None
 * @note This function quickly moves the clock hands using the stepper motor in the specified direction for the 
 *          specified number of seconds to correctly set the clock hands according to the chosen driver model.
 ***************************************************************************************************************/
static void moveClockHands(uint8_t directionToMove, uint32_t secondsToMove)
{
    bool ledState = 0;
    /* Convert seconds to steps: A_QUARTER*4 steps per hour (same calibration as MOVE_COMPENSATION_SEC_PER_HOUR),
       rounded to the nearest step instead of the old 22+1/4 approximation, which only matched exactly on whole
       hours and otherwise overshot by ~0.125% (up to ~4-5 seconds of extra movement) for any other duration. */
    uint32_t noOfsteps = (((uint32_t)secondsToMove * A_QUARTER * 4UL) + 1800UL) / 3600UL;

    digitalWrite(STEPPER_DIR_PIN, directionToMove); /* Set direction */

    for (uint32_t step = 0; step < noOfsteps; step++)
    {
        digitalWrite(STEPPER_PULSE_PIN, HIGH);
        delayMicroseconds(STEPPER_PULSE_TIME);
        digitalWrite(STEPPER_PULSE_PIN, LOW);
        delayMicroseconds(STEPPER_FAST_TIME_ADJUSTMENT);
        if(!((step) % (A_QUARTER/10)))
        { // it is use for feedback:
            if (ledState)
            {
                digitalWrite(CONTROL_YELLOW_LED_PIN, LOW); /* Turn off the yellow control LED for feedback */
                ledState = 0;
            } else {
                digitalWrite(CONTROL_YELLOW_LED_PIN, HIGH); /* Turn on the yellow control LED */
                ledState = 1;
            }
        }
    }
    digitalWrite(STEPPER_DIR_PIN, CW_DIR); /* Set direction cw */
    digitalWrite(CONTROL_YELLOW_LED_PIN, LOW); /* Turn off the yellow control LED for feedback */
}

/***************************************************************************************************************************
 * @brief Print time and date in a formated mode.
 * @param hour: one bytes, hour to print.
 * @param minute: one bytes, minute to print.
 * @param second: one bytes, second to print.
 * @param day: one bytes, day to print.
 * @param month: one bytes, month to print.
 * @param year: two bytes, year to print.
 * @return None
 * @note This function prints the current time and date from the RTC to the serial interface, ignore date if day is 0.
 ***************************************************************************************************************************/
static void printFormatedDateAndTime(uint8_t hour, uint8_t minute, uint8_t second, uint8_t day, uint8_t month, uint16_t year)
{
    Serial.print(hour, DEC);
    Serial.print(":");
    Serial.print(minute, DEC);
    Serial.print(":");
    Serial.println(second, DEC);

    if (0u != day)
    {
        Serial.print("Current date: ");
        Serial.print(day, DEC);
        Serial.print('/');
        Serial.print(month, DEC);
        Serial.print('/');
        Serial.println(year, DEC);
    }

    Serial.println("Done!");
    Serial.println("");
}

/***************************************************************************************************************
 * @brief Process serial command.
 * @param None
 * @return None
 * @note This function reads and processes serial commands:
 *   "f": Move clock hands forward. Expects three integers: hours, minutes, seconds.
 *   "b": Move clock hands backward. Expects three integers: hours, minutes, seconds.
 *   "ret": Read and print the last saved date and time from EEPROM.
 *   "rtc": Read and print the current date and time from the RTC.
 *   "reb": Read and print a range of bytes from EEPROM. Expects two integers: start index, end index.
 *   "wet": Write only the given hour and minute to EEPROM. Expects three integers: hour, minute, index (starting
 *          from index 7, multiple of 3 bytes).
 *  Every command, even ones that ignore them (ret, rtc, reset), must be followed by 3 numbers (e.g. "ret 0 0 0"):
 *   otherwise Serial.parseInt() blocks waiting for input and readStringUntil(' ') keeps the trailing newline.
 ***************************************************************************************************************/
static void processSerialCommand()
{
    String command = Serial.readStringUntil(' ');
    uint8_t directionToMove = (command == "f") ? CW_DIR : CCW_DIR;
    uint16_t num1 = Serial.parseInt();
    uint16_t num2 = Serial.parseInt();
    uint16_t num3 = Serial.parseInt();

    if ((command == "f") || (command == "b"))
    {
      uint32_t secondsToMove = (num1 * 3600) + (num2 * 60) + num3; /* Convert hours and minutes to seconds */
  
      Serial.print((directionToMove == CW_DIR) ? "CW" : "CCW");
      Serial.print(" direction for: ");
      Serial.print(secondsToMove);
      Serial.println(" seconds.");
  
      moveClockHands(directionToMove, secondsToMove);
      
      Serial.println("");
      Serial.println("Done!\nCW direction from now!");
    }

    if (command == "ret")
    {/* Reading and printing on serial the last saved date and time in EEPROM. */
        uint16_t index = (EEPROM[0] << 8) | EEPROM[1];

        Serial.print("read EEPROM index: ");
        Serial.println(index);

        printFormatedDateAndTime(CLEAR_TIME_RECOVER_FLAG(EEPROM[index]), EEPROM[index + 1], EEPROM[index + 2],
                                  EEPROM[4], EEPROM[5], EEPROM[6] + 2000);
    }

    if (command == "rtc")
    { /* Read RTC */
        DateTime currentTime = rtc.now();

        Serial.print("Current RTC time: ");

        printFormatedDateAndTime(currentTime.hour(), currentTime.minute(), currentTime.second(),
                                  currentTime.day(), currentTime.month(), currentTime.year());
    }

    if (command == "reb")
    {/* Read EEPROM from num1 to num2 */
        Serial.println("read bytes from EEPROM: ");

        for (uint16_t idx = num1; idx < num2; idx++)
        {
            Serial.print(EEPROM[idx]);
            Serial.print(", ");
        }
        
        Serial.println("Done!");
        Serial.println("");
    }

    if (command == "wet")
    {/* Write only the given hour and minute start from EEPROM[num3] 
        This is for testing purpose only */
        uint8_t retval = SUCCESS;
        Serial.println("write time to EEPROM ");

        num1 = SET_TIME_RECOVER_FLAG(num1); /* Set the MSB bit 8 to signal time is ready for recover */

        retval = writeToEEPROM(num3, num1, num2, 30u, 0, 0, 0); /* Write hour, minute and 30 sec*/

        Serial.print("Done with status ");
        if (retval == SUCCESS)
        {
            Serial.println("successful");
        }
        else
        {
            Serial.println("error");
        }
        
        Serial.println("");
    }

    if (command == "web")
    {/* Write num3 value to EEPROM from num1 to (num2-1) */
        Serial.println("Write bytes to EEPROM");

        for (; num1 < num2; num1++)
        {
            EEPROM[num1] = num3; /* Write num3 value to EEPROM */
        }

        Serial.println("Done!");
        Serial.println("");
    }

    if (command == "reset")
    {/* Write all EEPROM bytes with 0xFF. 
        If this command is used, the microcontroller must be restarted
        for the clock to function correctly (set the first bytes to 0)! */
        Serial.println("Reset all bytes to EEPROM to 0xFF");

        resetEEPROM();
        
        Serial.println("Done!");
        Serial.println("");
    }
    
    while (Serial.available() > 0)
    {/* Clear the serial buffer to avoid processing old commands. */
      Serial.read();
    }
}

/***************************************************************************************************************
 * @brief Reset all bytes in EEPROM to 0xFF (255).
 * @param None
 * @return None
 * @note This function resets all bytes in the EEPROM (from 0 to 1023) to 0xFF (255) to clear the stored data or 
 *       to simulate a new EEPROM when needed.
 ***************************************************************************************************************/
static void resetEEPROM(void)
{
    for (uint16_t i = 0; i < 1024; i++)
    {
        /* code */
        EEPROM.write(i, 255); /* Reset all bytes in EEPROM */
    }
}

/***************************************************************************************************************
 * @brief Perform a full software reset of the microcontroller using the watchdog timer.
 * @param None
 * @return None
 * @note Enables the watchdog with the shortest timeout and waits for it to fire, which restarts execution
 *       from setup() as if the board had just been powered on.
 ***************************************************************************************************************/
static void softwareReset(void)
{
    wdt_enable(WDTO_15MS);

    while (1)
    {/* Wait here until the watchdog timer fires and resets the microcontroller. */
    }
}
