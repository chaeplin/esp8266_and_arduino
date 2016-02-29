// http://gammon.com.au/i2c
// http://gammon.com.au/spi
#include <SPI.h>
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

volatile uint8_t* to_read_current;
volatile uint8_t* to_write_current;
volatile int s;
volatile int m;

template <typename T> unsigned int I2C_writeAnything (const T& value)
{
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}

ISR (SPI_STC_vect)
{
  byte c = SPDR;
  *to_read_current++ = c;
  SPDR = *to_write_current++;
  s++;
}

void setup ()
{
  SPCR |= bit (SPE);
  pinMode(MISO, OUTPUT);
  SPI.attachInterrupt();

  Wire.begin(8);
  Wire.onRequest(requestEvent);

  s = m = 0;

  sensor_data._salt = 0; sensor_data_copy._salt = 1;
  sensor_data.pls   = 0; sensor_data_copy.pls   = 1;
  sensor_data.ct1   = 0; sensor_data_copy.ct1   = 1;
  sensor_data.ct2   = 0; sensor_data_copy.ct2   = 1;
  sensor_data.ct3   = 0; sensor_data_copy.ct3   = 1;
  sensor_data.pad   = 0; sensor_data_copy.pad   = 1;
}

void loop ()
{
  if ( s == sizeof(sensor_data)) {
    sensor_data_copy._salt = sensor_data._salt + 1;
    sensor_data_copy.pls   = sensor_data.pls + 1;
    sensor_data_copy.ct1   = sensor_data.ct1 + 1;
    sensor_data_copy.ct2   = sensor_data.ct2 + 1;
    sensor_data_copy.ct3   = sensor_data.ct3 + 1;
    sensor_data_copy.pad   = sensor_data_copy._salt + sensor_data_copy.pls + sensor_data_copy.ct1 + sensor_data_copy.ct2 + sensor_data_copy.ct3 ;

    to_read_current = reinterpret_cast<volatile uint8_t*>(&sensor_data);
    to_write_current = reinterpret_cast<volatile uint8_t*>(&sensor_data_copy);
    SPDR = *to_write_current++;

    s = 0;
  }
}

void requestEvent()
{
  if ( m % 2 == 0 ) {
    I2C_writeAnything(sensor_data);
  } else {
    I2C_writeAnything(sensor_data_copy);
  }
  m++;
}
