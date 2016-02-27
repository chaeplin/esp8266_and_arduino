#include <Wire.h>

typedef struct
{
  uint32_t _salt;
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
} data;

volatile data sensor_data;
volatile data sensor_data_copy;
volatile boolean haveData = false;

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
  Serial.begin (115200);

  sensor_data_copy._salt = sensor_data_copy.pls = sensor_data_copy.ct1 = sensor_data_copy.ct2 = sensor_data_copy.ct3 = sensor_data_copy.pad = 0 ;

  Wire.begin(8);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);
}

void loop ()
{
  if (haveData) {

    sensor_data_copy._salt = sensor_data._salt;
    sensor_data_copy.pls   = sensor_data.pls;
    sensor_data_copy.ct1   = sensor_data.ct1;
    sensor_data_copy.ct2   = sensor_data.ct2;
    sensor_data_copy.ct3   = sensor_data.ct3;
    sensor_data_copy.pad   = sensor_data.pad;

    haveData = false;
  }
}

void requestEvent()
{
  I2C_writeAnything(sensor_data_copy);
}

void receiveEvent(int howMany)
{
  if (howMany >= sizeof(sensor_data))
  {
    I2C_readAnything(sensor_data);
  }
  haveData = true;
}
