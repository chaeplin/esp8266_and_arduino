// http://gammon.com.au/spi
#include <SPI.h>

typedef struct
{
  uint32_t _salt;
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
} data;

data sensor_data, sensor_data_copy;

volatile uint8_t* current;
volatile byte c;

void setup (void)
{
  Serial.begin (115200);

  sensor_data._salt = 0;
  sensor_data.pls   = 10;
  sensor_data.ct1   = 20;
  sensor_data.ct2   = 30;
  sensor_data.ct3   = 40;
  sensor_data.pad   = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  sensor_data_copy = sensor_data;
  
  SPCR |= bit (SPE);
  pinMode(MISO, OUTPUT);
  SPI.attachInterrupt();
}

ISR (SPI_STC_vect)
{
  c = SPDR;
  if ( c == 0 )
  {
    current = reinterpret_cast<uint8_t*>(&sensor_data_copy);
  }
  SPDR = *(current + c);
}

void loop (void)
{
  
  sensor_data._salt++;
  sensor_data.pls++;
  sensor_data.ct1++;
  sensor_data.ct2++;
  sensor_data.ct3++;
  sensor_data.pad = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  if ( c == sizeof(sensor_data) ) {
    sensor_data_copy = sensor_data;
    c = sizeof(sensor_data) + 1;
  }
  delay(1);
}
