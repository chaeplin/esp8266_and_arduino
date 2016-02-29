// http://gammon.com.au/i2c
// http://gammon.com.au/spi
#include <SPI.h>
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

uint32_t count, error, old_error;

data sensor_data, sensor_data_copy, sensor_data_slave, sensor_data_slave_copy;

template <typename T> unsigned int I2C_readAnything(T& value)
{
  byte * p = (byte*) &value;
  unsigned int i;
  if (Wire.requestFrom(SLAVE_ADDRESS, sizeof(value))) {
    for (i = 0; i < sizeof(value); i++) {
      *p++ = Wire.read();
    }
  }
  return i;
}

void read_write_payload(void* to_read_buf, void* to_write_buf, uint8_t data_len)
{
  uint8_t* to_read_current = reinterpret_cast<uint8_t*>(to_read_buf);
  uint8_t* to_write_current = reinterpret_cast<uint8_t*>(to_write_buf);
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  while ( data_len-- ) {
    *to_read_current++ = SPI.transfer(*to_write_current++);
    delayMicroseconds(8);
  }
  SPI.endTransaction();
}

void print_sensor_data()
{
  Serial.print("  _salt : ");
  Serial.print(sensor_data._salt);
  Serial.print("/");
  Serial.print(sensor_data_copy._salt);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave._salt);
    Serial.print("/");
    Serial.print(sensor_data_slave_copy._salt);
    `*/

  Serial.print(" -- pls : ");
  Serial.print(sensor_data.pls);
  Serial.print("/");
  Serial.print(sensor_data_copy.pls);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave.pls);
    Serial.print("/");
    Serial.print(sensor_data_slave_copy.pls);
  */

  Serial.print(" -- ct1 : ");
  Serial.print(sensor_data.ct1);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct1);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave.ct1);
    Serial.print("/");
    Serial.print(sensor_data_slave_copy.ct1);
  */

  Serial.print(" -- ct2 : ");
  Serial.print(sensor_data.ct2);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct2);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave.ct2);
    Serial.print("/");
    Serial.print(sensor_data_slave_copy.ct2);
    `*/

  Serial.print(" -- ct3 : ");
  Serial.print(sensor_data.ct3);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct3);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave.ct3);
    Serial.print("/");
    Serial.print(sensor_data_slave_copy.ct3);
  */

  Serial.print(" -- pad : ");
  Serial.print(sensor_data.pad);
  Serial.print("/");
  Serial.print(sensor_data_copy.pad);
  /*
    Serial.print("->");
    Serial.print(sensor_data_slave.pad);
    Serial.print("/");
    Serial.println(sensor_data_slave_copy.pad);
  */

  Serial.println("");

}

void setup ()
{
  Serial.begin(115200);

  Wire.begin(4, 5);
  twi_setClock(400000);

  SPI.begin ();
  SPI.setHwCs(true);

  sensor_data._salt = sensor_data_copy._salt = 0;
  sensor_data.pls   = sensor_data_copy.pls   = 10;
  sensor_data.ct1   = sensor_data_copy.ct1   = 20;
  sensor_data.ct2   = sensor_data_copy.ct2   = 30;
  sensor_data.ct3   = sensor_data_copy.ct3   = 40;
  sensor_data.pad   = sensor_data_copy.pad   = sensor_data._salt + sensor_data.pls + sensor_data.ct1 + sensor_data.ct2 + sensor_data.ct3 ;

  delay(100);
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));

  count = error = old_error = 0;
  Serial.println("");
  Serial.println("");
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
  
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));

  /* for debug
  I2C_readAnything(sensor_data_slave);
  I2C_readAnything(sensor_data_slave_copy);
  print_sensor_data();
  */
  
  if ( sensor_data._salt != sensor_data_copy._salt ||
       sensor_data.pls   != sensor_data_copy.pls ||
       sensor_data.ct1   != sensor_data_copy.ct1 ||
       sensor_data.ct2   != sensor_data_copy.ct2 ||
       sensor_data.ct3   != sensor_data_copy.ct3 ||
       sensor_data.pad   != sensor_data_copy.pad )
  {
    error++;
  }

  if ( error != old_error || count % 1000 == 0 ) {

    Serial.print("=====> count : ");
    Serial.print(count);
    Serial.print(" - error : ");
    Serial.print(error);

    print_sensor_data();
    old_error = error;
  }

  delay(2);
}
