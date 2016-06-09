// http://gammon.com.au/spi
#include <SPI.h>

typedef struct {
  uint32_t hash;
  uint32_t _salt;
  uint32_t pls;
  uint16_t ct1;
  uint16_t ct2;
  uint16_t ct3;
  uint16_t pad;
} data;

uint32_t count, error, old_error;

data sensor_data, sensor_data_copy;

unsigned long startMillis;

/* for hash */
static uint32_t fnv_1_hash_32(uint8_t *bytes, size_t length) {
  static const uint32_t FNV_OFFSET_BASIS_32 = 2166136261U;
  static const uint32_t FNV_PRIME_32 = 16777619U;
  uint32_t hash = FNV_OFFSET_BASIS_32;;
  for (size_t i = 0 ; i < length ; ++i) hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
  return hash;
}

template <class T> uint32_t calc_hash(T& data) {
  return fnv_1_hash_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(T) - sizeof(data.hash));
}

void read_write_payload(void* to_read_buf, void* to_write_buf, uint8_t data_len) {
  uint8_t* to_read_current = reinterpret_cast<uint8_t*>(to_read_buf);
  uint8_t* to_write_current = reinterpret_cast<uint8_t*>(to_write_buf);
  // tested to 6000000
  SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
  while ( data_len-- ) {
    *to_read_current++ = SPI.transfer(*to_write_current++);
    delayMicroseconds(8);
  }
  SPI.endTransaction();
}

void print_sensor_data() {
  Serial.print("  _salt : ");
  Serial.print(sensor_data._salt);
  Serial.print("/");
  Serial.print(sensor_data_copy._salt);

  Serial.print(" -- pls : ");
  Serial.print(sensor_data.pls);
  Serial.print("/");
  Serial.print(sensor_data_copy.pls);

  Serial.print(" -- ct1 : ");
  Serial.print(sensor_data.ct1);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct1);

  Serial.print(" -- ct2 : ");
  Serial.print(sensor_data.ct2);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct2);

  Serial.print(" -- ct3 : ");
  Serial.print(sensor_data.ct3);
  Serial.print("/");
  Serial.print(sensor_data_copy.ct3);

  Serial.println("");

}

void setup() {
  Serial.begin(115200);

  SPI.begin ();
  SPI.setHwCs(true);

  sensor_data._salt = sensor_data_copy._salt = 0;
  sensor_data.pls   = sensor_data_copy.pls   = 10;
  sensor_data.ct1   = sensor_data_copy.ct1   = 20;
  sensor_data.ct2   = sensor_data_copy.ct2   = 30;
  sensor_data.ct3   = sensor_data_copy.ct3   = 40;
  sensor_data.pad   = sensor_data_copy.pad   = 0;
  sensor_data.hash  = calc_hash(sensor_data);
  sensor_data_copy.hash  = calc_hash(sensor_data_copy);

  delay(100);
  /*
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));
  */
  
  count = error = old_error = 0;
  Serial.println("");
  Serial.println("");

  startMillis = millis();
}


void loop() {
  count++;

  sensor_data._salt++;
  sensor_data.pls++;
  sensor_data.ct1++;
  sensor_data.ct2++;
  sensor_data.ct3++;
  sensor_data.hash = calc_hash(sensor_data);
  
  read_write_payload(&sensor_data_copy, &sensor_data, sizeof(sensor_data));

/*  
  if ( sensor_data._salt != sensor_data_copy._salt ||
       sensor_data.pls   != sensor_data_copy.pls ||
       sensor_data.ct1   != sensor_data_copy.ct1 ||
       sensor_data.ct2   != sensor_data_copy.ct2 ||
       sensor_data.ct3   != sensor_data_copy.ct3 )
  {
    error++;
  }
*/
  if ( sensor_data.hash != calc_hash(sensor_data) || sensor_data_copy.hash != calc_hash(sensor_data_copy)) {
    error++;
  }

  if ( error != old_error || count % 1000 == 0 ) {

    Serial.print("=====> count : ");
    Serial.print(count);
    Serial.print(" - error : ");
    Serial.print(error);
    Serial.print(" - millis : ");
    Serial.print((millis() - startMillis));
    
    print_sensor_data();
    old_error = error;
    startMillis = millis();
  }
  delay(1);
}
