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

volatile data sensor_data;
volatile data sensor_data_copy;

volatile uint8_t* to_read_current;
volatile uint8_t* to_write_current;
volatile int s;

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

ISR (SPI_STC_vect) {
  byte c = SPDR;
  *to_read_current++ = c;
  SPDR = *to_write_current++;
  s++;
}

void setup() {
  SPCR |= bit (SPE);
  pinMode(MISO, OUTPUT);
  SPI.attachInterrupt();

  s = 0;

  sensor_data._salt = 0; sensor_data_copy._salt = 1;
  sensor_data.pls   = 0; sensor_data_copy.pls   = 1;
  sensor_data.ct1   = 0; sensor_data_copy.ct1   = 1;
  sensor_data.ct2   = 0; sensor_data_copy.ct2   = 1;
  sensor_data.ct3   = 0; sensor_data_copy.ct3   = 1;
  sensor_data.pad   = 0; sensor_data_copy.pad   = 0;
  sensor_data.hash  = calc_hash(sensor_data);
  sensor_data_copy.hash  = calc_hash(sensor_data_copy);
}

void loop() {
  if ( s == sizeof(sensor_data)) {
    sensor_data_copy._salt = sensor_data._salt + 1;
    sensor_data_copy.pls   = sensor_data.pls + 1;
    sensor_data_copy.ct1   = sensor_data.ct1 + 1;
    sensor_data_copy.ct2   = sensor_data.ct2 + 1;
    sensor_data_copy.ct3   = sensor_data.ct3 + 1;
    sensor_data_copy.hash  = calc_hash(sensor_data_copy);

    to_read_current = reinterpret_cast<volatile uint8_t*>(&sensor_data);
    to_write_current = reinterpret_cast<volatile uint8_t*>(&sensor_data_copy);
    SPDR = *to_write_current++;

    s = 0;
  }
}
