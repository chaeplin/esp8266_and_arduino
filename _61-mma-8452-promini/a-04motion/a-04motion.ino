#include <Wire.h>
#include <MMA8452.h>

MMA8452 accelerometer;

void setup()
{
	Serial.begin(115200);
	Serial.print(F("Initializing MMA8452Q: "));

	Wire.begin();

	bool initialized = accelerometer.init(); 
	
	if (initialized)
	{
		Serial.println(F("ok"));

		accelerometer.setDataRate(MMA_400hz);
		accelerometer.setRange(MMA_RANGE_2G);

		accelerometer.setMotionDetectionMode(MMA_MOTION, MMA_ALL_AXIS);
		accelerometer.setMotionTreshold(0x11);
	}
	else
	{
		Serial.println(F("failed. Check connections."));
		while (true) {};
	}
}

void loop()
{
	bool motion = accelerometer.motionDetected();
	if (motion)
	{
		Serial.print(F("Motion @ "));
		Serial.println(millis());
	}
}
