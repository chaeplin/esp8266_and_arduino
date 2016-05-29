#include <Wire.h>
#include <MMA8452.h>

MMA8452 accelerometer;

bool highpass = false;

void setup()
{
	Serial.begin(5600);

	Wire.begin();

	pinMode(5, OUTPUT);
	pinMode(6, OUTPUT);
	pinMode(9, OUTPUT);

	bool initialized = accelerometer.init();
	accelerometer.setDataRate(MMA_800hz);
	accelerometer.setRange(MMA_RANGE_2G);
}

void loop()
{
	if (Serial.available())
	{
		uint8_t c = Serial.read();
		if (c >= 0 && c <= 2)
		{
			accelerometer.setRange((mma8452_range_t)c);

			digitalWrite(5, c == 0);
			digitalWrite(6, c == 1);
			digitalWrite(9, c == 2);
			return;
		}

		if (c == 'q')
		{
			accelerometer.setHighPassFilter(false);
			highpass = false;
		} else if (c == 'w') {
			accelerometer.setHighPassFilter(true, MMA_HP1);
		} else if (c == 'e') {
			accelerometer.setHighPassFilter(true, MMA_HP2);
		} else if (c == 'r') {
			accelerometer.setHighPassFilter(true, MMA_HP3);
		} else if (c == 't') {
			accelerometer.setHighPassFilter(true, MMA_HP4);
		}

		digitalWrite(5, highpass);
		digitalWrite(6, highpass);
		digitalWrite(9, highpass);

		while (Serial.available()) Serial.read();
	}

	float x;
	float y;
	float z;

	accelerometer.getAcceleration(&x, &y, &z);

	Serial.print(x, 4);
	Serial.print(F(" "));
	Serial.print(y, 4);
	Serial.print(F(" "));
	Serial.print(z, 4);
	Serial.println();

	delay(1);
}
