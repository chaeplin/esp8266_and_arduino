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

		accelerometer.setDataRate(MMA_100hz);
		accelerometer.setRange(MMA_RANGE_2G);
	}
	else
	{
		Serial.println(F("failed. Check connections."));
		while (true) {};
	}
}

void loop()
{
	float x;
	float y;
	float z;

	accelerometer.getAcceleration(&x, &y, &z);

	Serial.print(x, 5);
	Serial.print(F(" "));
	Serial.print(y, 5);
	Serial.print(F(" "));
	Serial.print(z, 5);
	Serial.println();

	delay(10);
}

/*
Initializing MMA8452Q: ok
0.00586 0.00098 0.98730
0.00391 -0.00098 0.99219
0.00391 -0.00098 0.98535
0.00879 0.00098 0.98730
0.00586 0.00098 0.98535
0.00586 -0.00098 0.98828
0.00684 0.00000 0.98535
0.00391 -0.00098 0.98633
0.00781 -0.00195 0.98828
0.00391 0.00098 0.98535
0.00586 0.00000 0.98828
0.00293 0.00098 0.98633
0.00000 -0.00195 0.98730
*/
 */
