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

		accelerometer.setDataRate(MMA_800hz);	// we need a quick sampling rate
		accelerometer.setRange(MMA_RANGE_2G);

		accelerometer.enableSingleTapDetector(MMA_X);
		accelerometer.enableDoubleTapDetector(MMA_X, 0x22, 0xCC);
		accelerometer.setTapThreshold(0x55, 0x55, 0x33);
	}
	else
	{
		Serial.println(F("failed. Check connections."));
		while (true) {};
	}
}

void loop()
{
	bool singleTap;
	bool doubleTap;
	bool x;
	bool y;
	bool z;
	bool negX;
	bool negY;
	bool negZ;
	accelerometer.getTapDetails(&singleTap, &doubleTap, &x, &y, &z, &negX, &negY, &negZ);

	if (singleTap || doubleTap)
	{

		Serial.print(millis());
		Serial.print(F(": "));
		if (doubleTap) Serial.print(F("Double"));
		Serial.print(F("Tap on "));
		if (x)
		{
			Serial.print(F("X "));
			Serial.print(negX ? F("left ") : F("right "));
		}
		if (y)
		{
			Serial.print(F("Y "));
			Serial.print(negY ? F("down ") : F("up "));
		}
		if (z)
		{
			Serial.print(F("Z "));
			Serial.print(negZ ? F("out ") : F("in "));
		}
		Serial.println();
	}
}

/*
1Initializing MMA8452Q: ok
6074: Tap on X left 
6858: Tap on X left 
12052: Tap on X left 
15564: Tap on X right 
16242: Tap on X right 
16455: Tap on X left 
17061: Tap on X right 
17256: DoubleTap on X right 
18835: Tap on X right 
19355: Tap on X right 
20180: Tap on X left 
21469: Tap on X right 
23390: Tap on X right 
23586: Tap on X left 
*/

 */
