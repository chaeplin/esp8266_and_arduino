#include <Wire.h>

const int SLAVE_ADDRESS = 8;

typedef struct
{
  uint32_t _salt;
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
} data;

volatile data sensor_data, sensor_data_copy;

uint32_t count, error, old_error;
unsigned long wtime, rtime;

template <typename T> unsigned int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}

template <typename T> unsigned int I2C_writeAnything (const T& value)
{
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}

void setup ()
{
  delay(20);
  Serial.begin(115200);
  Wire.begin(4, 5);
  delay(200);

  sensor_data._salt = 0;
  sensor_data.pls   = 10;
  sensor_data.ct1   = 20;
  sensor_data.ct2   = 30;
  sensor_data.ct3   = 40;
  sensor_data.pad   = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  count = error = old_error = wtime = rtime = 0;
}


void loop ()
{
  count++;
  
  sensor_data._salt++;
  sensor_data.pls++;
  sensor_data.ct1++;
  sensor_data.ct2++;
  sensor_data.ct3++;
  sensor_data.pad = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  unsigned long now = micros();
  Wire.beginTransmission(SLAVE_ADDRESS);
  I2C_writeAnything(sensor_data);
  Wire.endTransmission();
  wtime += ( wtime + (micros() - now));

  delay(1);

  now = micros();
  if (Wire.requestFrom(SLAVE_ADDRESS, sizeof(sensor_data_copy)))
  {
    I2C_readAnything(sensor_data_copy);
  }
  rtime += ( rtime + (micros() - now));

  if ( sensor_data._salt != sensor_data_copy._salt ||
       sensor_data.pls   != sensor_data_copy.pls ||
       sensor_data.ct1   != sensor_data_copy.ct1 ||
       sensor_data.ct2   != sensor_data_copy.ct2 ||
       sensor_data.ct3   != sensor_data_copy.ct3 ||
       sensor_data.pad   != sensor_data_copy.pad )
  {
    error++;
  }

  if ( error != old_error || count % 100 == 0 ) {
    Serial.print("wtime/us : ");
    Serial.print(wtime/count);
    Serial.print(" - rtime/us : ");
    Serial.print(rtime/count);
    Serial.print(" - pad1 : ");
    Serial.print(sensor_data.pad);
    Serial.print(" - pad2 : ");
    Serial.print(sensor_data_copy.pad);
    Serial.print(" - count : ");
    Serial.print(count);
    Serial.print(" - error : ");
    Serial.println(error);
    old_error = error;
  }

  delay(1);
}
