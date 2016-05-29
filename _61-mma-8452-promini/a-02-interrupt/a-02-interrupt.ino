#include <Wire.h>
#include <MMA8452.h>

MMA8452 accelerometer;

volatile bool interrupt;

void setup()
{
	Serial.begin(115200);
	Serial.print(F("Initializing MMA8452Q: "));


	pinMode(2, INPUT);

	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);

	Wire.begin();
	bool initialized = accelerometer.init(); 
	
	if (initialized)
	{
		Serial.println(F("ok"));

		// config
		accelerometer.setDataRate(MMA_1_56hz);	// 800hz doesn't trigger an interrupt
		accelerometer.setRange(MMA_RANGE_2G);
		accelerometer.setInterruptsEnabled(MMA_DATA_READY);
		accelerometer.configureInterrupts(false, false);
    accelerometer.setInterruptPins(true, true, true, true, true, true);
		attachInterrupt(0, accelerometerInterruptHandler, FALLING);
	}
	else
	{
		Serial.println(F("failed. Check connections."));
		while (true) {};
	}
}

void loop()
{
	if (interrupt)
	{
		interrupt = false;
		bool wakeStateChanged;
		bool movementOccurred;
		bool landscapePortrait;
		bool tap;
		bool freefall;
		bool dataReady;
		accelerometer.getInterruptEvent(&wakeStateChanged, &movementOccurred, &landscapePortrait, &tap, &freefall, &dataReady);

		// if (dataReady)
		// {
			// clear flag so new data will be read
			uint16_t x, y, z;
			accelerometer.getRawData(&x, &y, &z);

			cli();
			digitalWrite(6, LOW);
			sei();
		// }
	}
	digitalWrite(5, HIGH);
	delay(100);
	digitalWrite(5, LOW);
	delay(100);
}

void accelerometerInterruptHandler()
{
	digitalWrite(6, HIGH);
	interrupt = true;
}
