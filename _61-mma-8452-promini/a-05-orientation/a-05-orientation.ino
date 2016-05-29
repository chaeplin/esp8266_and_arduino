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

		accelerometer.setDataRate(MMA_1_56hz);
		accelerometer.setRange(MMA_RANGE_2G);
		accelerometer.enableOrientationChange(true);
	}
	else
	{
		Serial.println(F("failed. Check connections."));
		while (true) {};
	}
}

void loop()
{
	bool orientationChanged;
	bool zTiltLockout;
	mma8452_orientation_t orientation;
	bool back;

	accelerometer.getPortaitLandscapeStatus(&orientationChanged, &zTiltLockout, &orientation, &back);

	if (orientationChanged)
	{
		Serial.print("Orientation is now ");
		switch (orientation)
		{
			case MMA_PORTRAIT_UP:
				Serial.print(F("Portrait up"));
				break;
			case MMA_PORTRAIT_DOWN:
				Serial.print(F("Portrait down"));
				break;
			case MMA_LANDSCAPE_RIGHT:
				Serial.print(F("Landscape right"));
				break;
			case MMA_LANDSCAPE_LEFT:
				Serial.print(F("Landscape left"));
				break;
		}
		Serial.print(F(" (back: "));
		Serial.print(back ? F("yep )") : F("nope)"));
		Serial.print(F(" - Z Tilt lockout: "));
		Serial.println(zTiltLockout);
	}
}
