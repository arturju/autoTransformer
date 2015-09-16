/*		The circuit:
 * LCD RS pin to digital pin 12
 * LCD Enable pin to digital pin 11
 * LCD D4 pin to digital pin 10
 * LCD D5 pin to digital pin 9
 * LCD D6 pin to digital pin 8
 * LCD D7 pin to digital pin 7
 * LCD R/W pin to ground
 * ends to +5V and ground	*/

#include <LiquidCrystal.h>
#include <serLCD.h>

LiquidCrystal lcd(12, 11, 10, 9, 8, 7);
byte plusOrMinus[8] = {
	B00000,
	B00100,
	B01110,
	B00100,
	B00000,
	B01110,
	B00000,
	B00000
};

// Encoder Tutorial: http://playground.arduino.cc/Main/RotaryEncoders
const int encoderPinA = 2;
const int encoderPinB = 3;
int encoderALast = LOW;
int encoderANow;						// int encoderBNow;
int value = 120;


// Voltage and smoothing variables
const int voltageInput = A0;
int analogVoltage;
float convertedVoltage;
const int numOfReadings = 8;				// for smoothing
int readings[numOfReadings];	int index = 0;		float total = 0;	float average = 0;
float tolerance = 2;															// GLOBAL VARIABLE FROM EEPROM. will read this value if no EEPROM value is found
	long timerForVolts = 0;
	long voltTimeElapsed = 0;
	long updateVoltageTime = 300;

// Variables for Relays
const int relayOne = A1;
const int relayTwo = A2;
	long timerForRelays = 0;						// waits Xms after trigger signal
	long relayTimeElapsed = 0;
	long updateRelayTime = 350;														// GLOBAL VARIABLE FROM EEPROM. will read this value if no EEPROM value is found

long timerForSwitchingBetweenRelays = 0;
long switchBetweenRelaysElapsed = 0;
long timeBetweenRelaySwitching = 150;		//150ms

// for physical button
int physicalButton = 6;
int buttonStateNow;
int lastButtonState = buttonStateNow;

#include <EEPROM.h>
// Calibrate Analog Value for:	        80, 90, 100, 110, 120, 130, 140, 150 VAC
const int calibrationAddressesArray[] = {0, 10, 20 ,  30, 40,  50,  60,  70};
const int setPointAddressesArray[] = { 100,110,120,  130,140 ,150, 160 ,170};	
const int relayResponseAddress = 80;
const int toleranceAddress = 90;
	
int sizeOfArray = 8;
int calibrationValues[sizeof(calibrationAddressesArray)];	
float setPointValues[sizeof(calibrationAddressesArray)];
	
int maxScreenState;
int screenState = 0;						// 0 = line/set Voltage, 1 = set Error, 2 = relay response, 3 = Calibration sequence
boolean calibrationComplete = 1;

void setup() {
	Serial.begin(9600);
	lcd.begin(16, 2);
	lcd.print("Booting...");
	delay(200);	
	lcd.createChar(0, plusOrMinus);	
	
	pinMode(encoderPinA, INPUT);
	pinMode(encoderPinB, INPUT);
	pinMode(voltageInput, INPUT);
	pinMode(physicalButton, INPUT);
	
	pinMode(relayOne, OUTPUT);
	pinMode(relayTwo, OUTPUT);
	
	/*		// write values to eeprom **first time**
	EEPROMWriteLong(relayResponseAddress, 25);
	EEPROMWriteLong(toleranceAddress, 2);
	EEPROMWriteLong(setPointAddressesArray[0], 80.0); EEPROMWriteLong(setPointAddressesArray[1], 90.0); EEPROMWriteLong(setPointAddressesArray[2], 100.0);
	EEPROMWriteLong(setPointAddressesArray[3], 110.0); EEPROMWriteLong(setPointAddressesArray[4], 120.0); EEPROMWriteLong(setPointAddressesArray[5], 130.0);
	EEPROMWriteLong(setPointAddressesArray[6], 140.0); EEPROMWriteLong(setPointAddressesArray[7], 150.0);
	*/
	
	// gET VALUES FROM EEPROM: calibration values & setpoints, tolerance, updateRelayTime
			for (int i = 0; i < sizeOfArray; i++)
			{
				calibrationValues[i] = EEPROMReadlong(calibrationAddressesArray[i]);
				setPointValues[i] = EEPROMReadlong(setPointAddressesArray[i]);
			}
			
			tolerance = EEPROMReadlong(toleranceAddress);
			updateRelayTime = EEPROMReadlong(relayResponseAddress);
	// END OF GETTING VALUES FROM EEPROM
	
	for (int thisReading = 0; thisReading < numOfReadings; thisReading++)
		readings[thisReading] = 120;					// start array at an average of 120VAC
	
	// If button is pressed on boot, enter calibration sequence
	if ( digitalRead(physicalButton) == 0)	
	{
		EEPROM.write(200,11);				// maxScreen = 11
		calibrationComplete = 0;
	}
	maxScreenState = EEPROM.read(200);
	
	if (calibrationComplete)	delay(2000);		// when booting delay so transducer can stabilize
	/*	// for troubleshooting..
	Serial.print("\r\nSize of Calibration Array: "); Serial.println(sizeOfArray);
	for (int i = 0; i < sizeOfArray; i++)
	{
		Serial.print(calibrationValues[i]); Serial.print(" , ");		
	}
	Serial.println();
	Serial.print("Max Screen Value: "); Serial.println(maxScreenState);
	*/
}

//This function will write a 4 byte (32bit) long to the EEPROM at the specified address to aDdress + 3.
void EEPROMWriteLong( int address, long value)
{
	byte four = (value & 0xFF);
	byte three = ((value >> 8) & 0xFF);
	byte two = ((value >> 16) & 0xFF);
	byte one = ((value >> 24) & 0xFF);
	
	//Write the 4 bytes into the eeprom memory.
	EEPROM.write(address, four);
	EEPROM.write(address + 1, three);
	EEPROM.write(address + 2, two);
	EEPROM.write(address + 3, one);
}

long EEPROMReadlong(long address)
{
	//Read the 4 bytes from the eeprom memory.
	long four = EEPROM.read(address);
	long three = EEPROM.read(address + 1);
	long two = EEPROM.read(address + 2);
	long one = EEPROM.read(address + 3);

	//Return the recomposed long by using bitshift.
	return ((four << 0) & 0xFF) + ((three << 8) & 0xFFFF) + ((two << 16) & 0xFFFFFF) + ((one << 24) & 0xFFFFFFFF);
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void buttonPressedFunction()			// only types first row of 16x2 LCD
{
	screenState++;
	if (screenState >= maxScreenState)		screenState = 0;
	lcd.clear();
	
	switch (screenState){
		case 0:
			lcd.print("LINE:   SET:");	break;
		
		case 1:
			lcd.print("Tolerance:");	break;
		
		case 2:
			lcd.print("Relay Response:"); break;
			
		case 3:			// 80.xx VAC
			lcd.print("Calibrate ~80VAC "); break;
			
		case 4:
			lcd.print("Calibrate ~90VAC "); break;
			
		case 5:
			lcd.print("Calibrate ~100VAC "); break;		
		
		case 6:			// 110VAC
			lcd.print("Calibrate ~110VAC "); break;
		
		case 7:
			lcd.print("Calibrate ~120VAC "); break;
			
		case 8:
			lcd.print("Calibrate ~130VAC "); break;
		
		case 9:
			lcd.print("Calibrate ~140VAC "); break;
		
		case 10:			// 150VAC
			lcd.print("Calibrate ~150VAC ");
			maxScreenState = 1;		EEPROM.write(200, maxScreenState);			// will go up to (maxScreenState - 1)
			calibrationComplete = 1;
			break;
	}
}		// first 

void chooseScreenFunction()				// types in 2nd row of 16x2 LCD
{
	lcd.setCursor(0,1);					// bottom left corner
	switch (screenState){
		case 0:							// LINE:	SET:
			lcd.print((float)convertedVoltage,1);	lcd.print("V  ");			
			lcd.print(value);						lcd.print(" VAC ");
			break;
			
		case 1:							// Tolerance:
			lcd.write(byte(0));	//?
			lcd.print((float)tolerance,1);			lcd.print(" V  ");
			lcd.print((int)average);				lcd.print(" ADU ");
			EEPROM.write(toleranceAddress, tolerance);
			break;
			
		case 2:
			lcd.print(updateRelayTime);				lcd.print(" mS  ");
			EEPROM.write(relayResponseAddress, updateRelayTime);	
			break;
			
		// CALIBRATION ROUTINE: Saves values to EEPROM
		case 3:
			calibrationValues[0] = analogRead(voltageInput);				// 80VAC
			lcd.print(calibrationValues[0]); lcd.print(" ADU = "); lcd.print((float)setPointValues[0],1);
			EEPROMWriteLong(calibrationAddressesArray[0], calibrationValues[0]);
			break;
			
		case 4:
			calibrationValues[1] = analogRead(voltageInput);				// 90VAC
			lcd.print(calibrationValues[1]); lcd.print(" ADU = "); lcd.print((float)setPointValues[1],1);
			EEPROMWriteLong(calibrationAddressesArray[1], calibrationValues[1]);
			break;	
			
		case 5:
			calibrationValues[2] = analogRead(voltageInput);				// 100VAC
			lcd.print(calibrationValues[2]); lcd.print(" ADU = "); lcd.print((float)setPointValues[2],1);
			EEPROMWriteLong(calibrationAddressesArray[2], calibrationValues[2]);
			break;
			
		case 6:
			calibrationValues[3] = analogRead(voltageInput);				// 110V
			lcd.print(calibrationValues[3] ); lcd.print(" ADU = "); lcd.print((float)setPointValues[3],1);
			EEPROMWriteLong(calibrationAddressesArray[3], calibrationValues[3] );
			break;
			
		case 7:
			calibrationValues[4] = analogRead(voltageInput);				// 120V
			lcd.print(calibrationValues[4] ); lcd.print(" ADU = "); lcd.print((float)setPointValues[4],1);
			EEPROMWriteLong(calibrationAddressesArray[4], calibrationValues[4] );
			break;
			
		case 8:
			calibrationValues[5] = analogRead(voltageInput);				// 130V
			lcd.print(calibrationValues[5] ); lcd.print(" ADU = "); lcd.print((float)setPointValues[5],1);
			EEPROMWriteLong(calibrationAddressesArray[5], calibrationValues[5] );
			break;
		
		case 9:
			calibrationValues[6] = analogRead(voltageInput);				// 140V
			lcd.print(calibrationValues[6] ); lcd.print(" ADU = "); lcd.print((float)setPointValues[6],1);
			EEPROMWriteLong(calibrationAddressesArray[6], calibrationValues[6] );
			break;
		
		case 10:
			calibrationValues[7] = analogRead(voltageInput);				// 150V
			lcd.print(calibrationValues[7] ); lcd.print(" ADU = "); lcd.print((float)setPointValues[7],1);
			EEPROMWriteLong(calibrationAddressesArray[7], calibrationValues[7] );
			break;		
	}
	
}

void updateLineVoltage()
{
	timerForVolts = millis();
	
	//analogVoltage = analogRead(voltageInput);						// without smoothing
	total = total - readings[index];
	readings[index] = analogRead(voltageInput);
	total = total + readings[index];
	index = index + 1;
	
	if (index >= numOfReadings)
		index = 0;
		
	average = total / numOfReadings;	
	
	if ( convertedVoltage >= calibrationValues[0] && convertedVoltage < calibrationValues[1] )	// Between 80VAC and 90VAC
		convertedVoltage = mapfloat(average, calibrationValues[0], calibrationValues[1], setPointValues[0], setPointValues[1]);
	
	else if ( convertedVoltage >= calibrationValues[1] && convertedVoltage < calibrationValues[2] )	// Between 90VAC and 100VAC
		convertedVoltage = mapfloat(average, calibrationValues[1], calibrationValues[2], setPointValues[1], setPointValues[2]);

	else if ( convertedVoltage >= calibrationValues[2] && convertedVoltage < calibrationValues[3] )	// Between 100VAC and 110VAC
		convertedVoltage = mapfloat(average, calibrationValues[2], calibrationValues[3], setPointValues[2], setPointValues[3]);	

	else if ( convertedVoltage >= calibrationValues[3] && convertedVoltage < calibrationValues[4] )	// Between 110VAC and 120VAC
		convertedVoltage = mapfloat(average, calibrationValues[3], calibrationValues[4], setPointValues[3], setPointValues[4]);	
		
	else if ( convertedVoltage >= calibrationValues[4] && convertedVoltage < calibrationValues[5] )	// Between 120VAC and 130VAC
		convertedVoltage = mapfloat(average, calibrationValues[4], calibrationValues[5], setPointValues[4], setPointValues[5]);		

	else if ( convertedVoltage >= calibrationValues[5] && convertedVoltage < calibrationValues[6] )	// Between 130VAC and 140VAC
		convertedVoltage = mapfloat(average, calibrationValues[5], calibrationValues[6], setPointValues[5], setPointValues[6]);

	else if ( convertedVoltage >= calibrationValues[6] && convertedVoltage < calibrationValues[7] )	// Between 140VAC and 150VAC
		convertedVoltage = mapfloat(average, calibrationValues[6], calibrationValues[7], setPointValues[6], setPointValues[7]);
	
	else																							// if <80 & > 150, use range to map
		convertedVoltage = mapfloat(average, calibrationValues[0], calibrationValues[7], setPointValues[0], setPointValues[7]);
	
	
	//convertedVoltage = mapfloat(average, calibration80Value, calibration120Value, 80.0, 130.0);		
}

void updateRelaysFunction()
{	
	switchBetweenRelaysElapsed = millis() - timerForSwitchingBetweenRelays;
	
	if ( (convertedVoltage - tolerance) > value)								//relay 6 ON, 7 OFF
	{
		digitalWrite(relayTwo, LOW);
		if (switchBetweenRelaysElapsed > timeBetweenRelaySwitching)				// 150ms have expired
		{
			digitalWrite(relayOne, HIGH);
			timerForSwitchingBetweenRelays = millis();
		}
			
	}
	else if ( (convertedVoltage + tolerance) < value)								// relay 6 OFF, 7 ON
	{
		digitalWrite(relayOne, LOW);
		if (switchBetweenRelaysElapsed > timeBetweenRelaySwitching)					// 150ms have expired
		{
			digitalWrite(relayTwo, HIGH);
			timerForSwitchingBetweenRelays = millis();
		}		
	}
	else												// relay 6 OFF, 7 OFF
	{
		digitalWrite(relayOne, LOW);
		digitalWrite(relayTwo, LOW);
	}
}

void loop() {
	
// ENCODER to adjust setvalue
	encoderANow = digitalRead(encoderPinA); 	
	int encoderBNow = digitalRead(encoderPinB);	
	//Serial.print(encoderANow); Serial.print(" | "); Serial.println(encoderBNow);
	
	if ( (encoderALast == LOW) && (encoderANow == HIGH) )				// RISING EDGE on A
	{
		encoderBNow = digitalRead(encoderPinB);
		if (encoderBNow == LOW)							
		{			
			switch(screenState){
				case 0:
					if (value <280)
						value++;
					break;
				case 1:
					if (tolerance < 5)
						tolerance +=0.5;
					break;
				case 2:
					if (updateRelayTime <300)
						updateRelayTime += 25;
					break;
					
				// CALIBRATION screens: saving values to EEPROM
				case 3:
					setPointValues[0] += 0.1; break;
					
				case 4:
					setPointValues[1] += 0.1; break;
				
				case 5:
					setPointValues[2] += 0.1; break;
				
				case 6:
					setPointValues[3] += 0.1; break;
				
				case 7:
					setPointValues[4] += 0.1; break;		
					
				case 8:
					setPointValues[5] += 0.1; break;
				
				case 9:
					setPointValues[6] += 0.1; break;
	
				case 10:
					setPointValues[7] += 0.1; break;
			}
		}
		else
		{
			switch(screenState){
				case 0:
					if (value > 80)
						value--;
					break;
				case 1:
					if (tolerance > 0.5)
						tolerance -= 0.5;
					break;
				case 2:
					if (updateRelayTime > 50)
						updateRelayTime -= 25;
					break;
					
				// CALIBRATION screens: saving values to EEPROM
				case 3:
					setPointValues[0] -= 0.1; break;
				
				case 4:
					setPointValues[1] -= 0.1; break;
				
				case 5:
					setPointValues[2] -= 0.1; break;
				
				case 6:
					setPointValues[3] -= 0.1; break;
				
				case 7:
					setPointValues[4] -= 0.1; break;
				
				case 8:
					setPointValues[5] -= 0.1; break;
					
				case 9:
					setPointValues[6] -= 0.1; break;
					
				case 10:
					setPointValues[7] -= 0.1; break;	
			}
		}		
	}		// end of rising edge function
	encoderALast = encoderANow;

	// LCD screen Displays
	chooseScreenFunction();


// PUSH BUTTON to switch Screens
	buttonStateNow = digitalRead(physicalButton);	
	if ( lastButtonState == LOW && buttonStateNow == HIGH)				buttonPressedFunction();			// trigger when pressed (rising edge)		
	lastButtonState = buttonStateNow;


// update line voltage every xS
	if ( voltTimeElapsed> updateVoltageTime)		updateLineVoltage();
	voltTimeElapsed = millis() - timerForVolts;

// relay control, every 1s. ONLY after calibration is complete
	if ( relayTimeElapsed > updateRelayTime && calibrationComplete)			// time to update relay
	{		
		updateRelaysFunction();		
		timerForRelays = millis();		
	}
	relayTimeElapsed = millis() - timerForRelays;
	
	
	
	
	
}