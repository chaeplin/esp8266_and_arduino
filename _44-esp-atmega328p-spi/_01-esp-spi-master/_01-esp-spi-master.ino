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

data sensor_data;

uint32_t count;
uint32_t error;

void read_payload(void* buf, uint8_t data_len) {
  uint8_t* current = reinterpret_cast<uint8_t*>(buf);
  unsigned int i;

  //SPI.beginTransaction(SPISettings(3000000, MSBFIRST, SPI_MODE0));
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  SPI.transfer(0);
  delayMicroseconds(15);
  for (i = 1 ; i <= data_len ; i++) {
    *current++ = SPI.transfer(i);
    delayMicroseconds(7);
  }
  SPI.endTransaction();
}

void setup (void)
{
  Serial.begin(115200);

  count = error = 0;

  SPI.begin ();
  SPI.setHwCs(true);
  Serial.println("");
}

void loop (void)
{
  read_payload(&sensor_data, sizeof(sensor_data));
  
  uint16_t test = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3;

  if ( sensor_data.pad != test ) {
    error++;
  }

  if ( count % 1000 == 0 ) {
    Serial.print("count : ");
    Serial.print(count);
    Serial.print(" - error : ");
    Serial.println(error);
  }
  count++;
  delay(5);
}
