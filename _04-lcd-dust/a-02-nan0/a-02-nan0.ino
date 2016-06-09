#include <IRremote.h>
#include <Wire.h>
#include <Average.h>

// A6  : DUST IN
// D2  : DUST OUT
//
// D3  : IR OUT
// 
// D6  : IR IN
// D7  : OUT GND
//
// D12 : OUT GND

/* dust */
int measurePin = A6;
int ledPower = 2;

int moisturePin = A2;

int IR_receive_recv_PIN = 6;
int IR_receive_GND_PIN  = 7;
int IR_send_GND_PIN     = 12;

/* dust and moisture */
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

int voMeasured = 0;
int moMeasured = 0;

float calcVoltage = 0;
float dustDensity = 0;

/* ac */
const int AC_TYPE    = 0;     // 0 : TOWER, 1 : WALL
int AC_POWER_ON      = 0;     // 0 : off, 1 : on
int AC_AIR_ACLEAN    = 0;     // 0 : off,  1 : on --> power on
int AC_TEMPERATURE   = 27;    // temperature : 18 ~ 30
int AC_FLOW          = 0;     // 0 : low, 1 : mid , 2 : high
int AC_FLOW_TOWER[3] = {0, 4, 6};
int AC_FLOW_WALL[3]  = {0, 2, 4};

unsigned long AC_CODE_TO_SEND;

unsigned long startMills;

volatile bool haveData = false;

volatile struct {
  uint32_t hash;
  uint16_t voMeasured;
  uint16_t moMeasured;
  uint8_t tempmode;
  uint8_t tempset;
  uint8_t tempflow;
  uint8_t tempnone;
} device_data;

Average<float> ave1(10);
Average<float> ave2(10);

//IRrecv irrecv(IR_receive_recv_PIN); // Receive on pin 6
IRsend irsend;

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

/* for i2c */
template <typename T> unsigned int I2C_readAnything(T& value) {
  byte * p = (byte*) &value;
  unsigned int i;
  for (i = 0; i < sizeof value; i++)
    *p++ = Wire.read();
  return i;
}

template <typename T> unsigned int I2C_writeAnything (const T& value) {
  Wire.write((byte *) &value, sizeof (value));
  return sizeof (value);
}

void setup() {
  Serial.begin(115200);

  device_data.voMeasured = 0;
  device_data.moMeasured = 0;
  device_data.tempmode   = 0;
  device_data.tempset    = 27;
  device_data.tempflow   = 0;
  device_data.tempnone   = 0;

  startMills = millis();

  Serial.println("Starting Sensors");
  pinMode(ledPower, OUTPUT);

  pinMode(IR_receive_GND_PIN, OUTPUT);
  pinMode(IR_send_GND_PIN, OUTPUT);

  digitalWrite(IR_receive_GND_PIN, LOW);
  digitalWrite(IR_send_GND_PIN, LOW);

  //irrecv.enableIRIn(); // Start the receiver

  Wire.begin(2);                // join i2c bus with address #2
  Wire.onRequest(requestEvent); // register event
  Wire.onReceive(receiveEvent);
}

void loop() {
  if (haveData) {
    if (device_data.hash == calc_hash(device_data)) {
      Serial.print("Msg received : ");
      switch (device_data.tempmode) {
        case 0:
          Serial.println("IR -----> AC OFF");
          ac_power_down();
          delay(50);
          break;

        case 1:
          Serial.println("IR -----> AC ON");
          ac_activate(device_data.tempset, device_data.tempflow);
          delay(50);
          break;

        default:
          break;
      }
    }
    haveData = false;
  }

  if ((millis() - startMills) > 200 ) {
    digitalWrite(ledPower, LOW); // power on the LED
    delayMicroseconds(samplingTime);

    voMeasured = analogRead(measurePin); // read the dust value

    delayMicroseconds(deltaTime);
    digitalWrite(ledPower, HIGH); // turn the LED off
    delayMicroseconds(sleepTime);

    moMeasured = analogRead(moisturePin);

    ave1.push(voMeasured);
    ave2.push(moMeasured);

    startMills = millis();

    Serial.print("ave1 : ");
    Serial.print(uint16_t(ave1.mean()));
    Serial.print(" ave2 : ");
    Serial.print(uint16_t(ave2.mean()));
    Serial.print(" dust : ");
    Serial.println((0.17 * (ave1.mean() * 0.0049) - 0.1));
  }
}

void requestEvent() {
  device_data.voMeasured = uint16_t(ave1.mean());
  device_data.moMeasured = uint16_t(ave2.mean());
  device_data.hash = calc_hash(device_data);
  I2C_writeAnything(device_data);
}

void receiveEvent(int howMany) {
  if (howMany >= sizeof(device_data)) {
    I2C_readAnything(device_data);
  }
  haveData = true;
}

void ac_send_code(unsigned long code) {
  Serial.print("code to send : ");
  //  Serial.print(code, BIN);
  //  Serial.print(" : ");
  Serial.println(code, HEX);

  delay(100);
  irsend.sendLG(code, 28);

  //delay(100);
  // irrecv.enableIRIn(); // Start the receiver
}

void ac_activate(int temperature, int air_flow) {
  int AC_MSBITS1 = 8;
  int AC_MSBITS2 = 8;
  int AC_MSBITS3 = 0;
  int AC_MSBITS4 = 0;
  int AC_MSBITS5 = temperature - 15;
  int AC_MSBITS6 ;

  if ( AC_TYPE == 0) {
    AC_MSBITS6 = AC_FLOW_TOWER[air_flow];
  } else {
    AC_MSBITS6 = AC_FLOW_WALL[air_flow];
  }

  int AC_MSBITS7 = (AC_MSBITS3 + AC_MSBITS4 + AC_MSBITS5 + AC_MSBITS6) & B00001111;

  AC_CODE_TO_SEND =  AC_MSBITS1 << 4 ;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS2) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS3) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS4) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS5) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS6) << 4;
  AC_CODE_TO_SEND =  (AC_CODE_TO_SEND + AC_MSBITS7);

  ac_send_code(AC_CODE_TO_SEND);

  AC_POWER_ON = 1;
  AC_TEMPERATURE = temperature;
  AC_FLOW = air_flow;
}

void ac_power_down() {
  AC_CODE_TO_SEND = 0x88C0051;
  ac_send_code(AC_CODE_TO_SEND);
  AC_POWER_ON = 0;
}


