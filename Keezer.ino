/**
 * Keezer! v1.0
 */
#include <EEPROM.h> 
#include <PinChangeInt.h>
#include <LiquidCrystal.h>

/**
 * Whats on tap!?
 */
const String tap1 = "New England IPA";
const String tap1Abv = "5.1%";
float tap1Litres = 16.9;
bool reset1 = false;

const String tap2 = "Berliner Weisse";
const String tap2Abv = "2.8%";
float tap2Litres = 10.0;
bool reset2 = false;

const String tap3 = "(Empty)";
const String tap3Abv = "";
float tap3Litres = 0.0;
bool reset3 = true;

const String tap4 = "(Empty)";
const String tap4Abv = "";
float tap4Litres = 0.0;
bool reset4 = true;

// Shared LCD interface pins
const int rs = 12;
const int d4 = 9;
const int d5 = 8;
const int d6 = 7;
const int d7 = 10;

// Each LCD has its own Enable pin
const int lcdEnable1 = 2;
const int lcdEnable2 = 3;
const int lcdEnable3 = 4;
const int lcdEnable4 = 5;

// Initialize each LCD
LiquidCrystal lcd1(rs, lcdEnable1, d4, d5, d6, d7);
LiquidCrystal lcd2(rs, lcdEnable2, d4, d5, d6, d7);
LiquidCrystal lcd3(rs, lcdEnable3, d4, d5, d6, d7);
LiquidCrystal lcd4(rs, lcdEnable4, d4, d5, d6, d7);

/**
 * Flow meters
 * 
 * Pins connected to flow meter's signal (yellow wire)
 */
byte flowPin1 = A0;
byte flowPin2 = A1;
byte flowPin3 = A2;
byte flowPin4 = A3;

/**
 * Flow rate pulse characteristics of the YF-S201;
 * Frequency (Hz) = 7.5 * Flow rate (L/min) = 450 pulses per litre
 * But each meter may have a slight variation so let's
 * have a calibration for each one.
 */
float calibrationFactor1 = 450;
float calibrationFactor2 = 450;
float calibrationFactor3 = 450;
float calibrationFactor4 = 450;

/**
 * Keep track of total flow through each meter
 */ 
float totalFlow1 = 0.0;
float totalFlow2 = 0.0;
float totalFlow3 = 0.0;
float totalFlow4 = 0.0;

/**
 * Count of pulses from each meter
 */ 
volatile byte flowMeterCount1 = 0;
volatile byte flowMeterCount2 = 0;
volatile byte flowMeterCount3 = 0;
volatile byte flowMeterCount4 = 0;

/**
 * Keep track of time in the loop
 */ 
unsigned long oldTime = 0;

/**
 * There are 1.75975 pints in a litre.
 */
const float litreToPint = 1.76;

void setup() {
  
  Serial.begin(9600);
  
  /**
   * LCDs
   */
   // set up the LCD's number of rows and columns: 
  lcd1.begin(16, 2);
  lcd2.begin(16, 2);
  lcd3.begin(16, 2);
  lcd4.begin(16, 2);
 
  // Print beer names
  lcd1.print(tap1);
  lcd2.print(tap2);
  lcd3.print(tap3);
  lcd4.print(tap4);
  
  // set the cursor to column 0, line 1
  // (note: line 1 is the second row, since counting begins with 0):
  lcd1.setCursor(0, 1);
  lcd2.setCursor(0, 1);
  lcd3.setCursor(0, 1);
  lcd4.setCursor(0, 1);
 
  // Print the ABV
  lcd1.print(tap1Abv);
  lcd2.print(tap2Abv);
  lcd3.print(tap3Abv);
  lcd4.print(tap4Abv);
  
  lcd1.setCursor(6, 1);
  lcd2.setCursor(6, 1);
  lcd3.setCursor(6, 1);
  lcd4.setCursor(6, 1);
  
  lcd1.print("Pints:");
  lcd2.print("Pints:");
  lcd3.print("Pints:");
  lcd4.print("Pints:");
  
  lcd1.setCursor(12, 1);
  lcd2.setCursor(12, 1);
  lcd3.setCursor(12, 1);
  lcd4.setCursor(12, 1);

  // Get total pour amounts (in litres) in memory at addresses 100, 200, 300 and 400
  if (!reset1) {
    EEPROM.get(100, totalFlow1);  
  }

  if (!reset2) {
    EEPROM.get(200, totalFlow2);  
  }
  
  if (!reset3) {
    EEPROM.get(300, totalFlow3);  
  }

  if (!reset4) {
    EEPROM.get(400, totalFlow4);  
  }

  // Work out whats left in keg (original total minus pour total from memory) then convert our litres to pints and display
  lcd1.print((tap1Litres - totalFlow1) * litreToPint);
  lcd2.print((tap2Litres - totalFlow2) * litreToPint);
  lcd3.print((tap3Litres - totalFlow3) * litreToPint);
  lcd4.print((tap4Litres - totalFlow4) * litreToPint);
  
  /**
   * Flow meters
   */ 
  pinMode(flowPin1, INPUT);
  pinMode(flowPin2, INPUT);
  pinMode(flowPin3, INPUT);
  pinMode(flowPin4, INPUT);

  digitalWrite(flowPin1, HIGH);
  digitalWrite(flowPin2, HIGH);
  digitalWrite(flowPin3, HIGH);
  digitalWrite(flowPin4, HIGH);

  /**
   * Configure an interrupt on each flow meter pin to trigger on a FALLING state
   * change transition from HIGH state to LOW state)
   * Note: Requires the PinChangeInt library to allow more than pins 1 and 2
   * to be interrupts on the Arduino Uno.
   */
  PCintPort::attachInterrupt(flowPin1, pulseCounter1, FALLING);
  PCintPort::attachInterrupt(flowPin2, pulseCounter2, FALLING);
  PCintPort::attachInterrupt(flowPin3, pulseCounter3, FALLING);
  PCintPort::attachInterrupt(flowPin4, pulseCounter4, FALLING);
}

void loop() {
  
  /**
   * Flow meters
   * 
   * Only process the pulsecounts every 500 m/s
   */ 
  if ((millis() - oldTime) > 500) {
  
    /**
     * Disable the interrupt while calculating flow rate
     */
    PCintPort::detachInterrupt(flowPin1);
    PCintPort::detachInterrupt(flowPin2);
    PCintPort::detachInterrupt(flowPin3);
    PCintPort::detachInterrupt(flowPin4);

    Serial.print(flowMeterCount1);
    Serial.print(" | ");
    Serial.print(flowMeterCount2);
    Serial.print(" | ");
    Serial.print(flowMeterCount3);
    Serial.print(" | ");
    Serial.println(flowMeterCount4);

    /** 
     * Calculate the number of milliseconds that have passed since the 
     * last execution and use that to scale the output. We also apply the 
     * calibrationFactor to scale the output based on the number of pulses 
     * per second per units of measure (litres/minute in this case).
     */ 
    float litresFlowed1 = flowMeterCount1 / calibrationFactor1;
    float litresFlowed2 = flowMeterCount2 / calibrationFactor2;
    float litresFlowed3 = flowMeterCount3 / calibrationFactor3;
    float litresFlowed4 = flowMeterCount4 / calibrationFactor4;

    /**
     * Add the vol  passed to the cumulative total
     */
    totalFlow1 += litresFlowed1;
    totalFlow2 += litresFlowed2;
    totalFlow3 += litresFlowed3;
    totalFlow4 += litresFlowed4;

    // Put total pour amounts (L) in memory at addresses 100, 200, 300 and 400
    EEPROM.put(100, totalFlow1);
    EEPROM.put(200, totalFlow2);
    EEPROM.put(300, totalFlow3);
    EEPROM.put(400, totalFlow4);

    // Work out how many litres left in keg
    float newLitresTotal1 = tap1Litres - totalFlow1;
    float newLitresTotal2 = tap2Litres - totalFlow2;
    float newLitresTotal3 = tap3Litres - totalFlow3;
    float newLitresTotal4 = tap4Litres - totalFlow4;

    // Set cursor to col 12 on row 1 (2nd row)
    lcd1.setCursor(12, 1);
    lcd2.setCursor(12, 1);
    lcd3.setCursor(12, 1);
    lcd4.setCursor(12, 1);

    // Print to screen after converting to pints
    lcd1.print(newLitresTotal1 * litreToPint);
    lcd2.print(newLitresTotal2 * litreToPint);
    lcd3.print(newLitresTotal3 * litreToPint);
    lcd4.print(newLitresTotal4 * litreToPint);
    
    /**
     * Reset the flow meter pulse counter so we can start incrementing again
     */ 
    flowMeterCount1 = 0;
    flowMeterCount2 = 0;
    flowMeterCount3 = 0;
    flowMeterCount4 = 0;
    
    /**
     * Note the time this processing pass was executed. Note that because we've
     * disabled interrupts the millis() function won't actually be incrementing right
     * at this point, but it will still return the value it was set to just before
     * interrupts went away.
     */  
    oldTime = millis();

    /**
     * Enable the interrupt again now that we've finished sending output
     */
    PCintPort::attachInterrupt(flowPin1, pulseCounter1, FALLING);
    PCintPort::attachInterrupt(flowPin2, pulseCounter2, FALLING);
    PCintPort::attachInterrupt(flowPin3, pulseCounter3, FALLING);
    PCintPort::attachInterrupt(flowPin4, pulseCounter4, FALLING);
  }
}

/*
 * Flow meter routines. Increment the pulse counters
 */
void pulseCounter1()
{
  flowMeterCount1++;
}

void pulseCounter2()
{
  flowMeterCount2++;
}

void pulseCounter3()
{
  flowMeterCount3++;
}

void pulseCounter4()
{
  flowMeterCount4++;
}
