#define vers "NTP GPS V01A"

#define debug false

#include <TinyGPS.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>


#define _IS_MY_HOME
// wifi
#ifdef _IS_MY_HOME
#include "/usr/local/src/ap_setting.h"
#else
#include "ap_setting.h"
#endif

extern "C" {
  #include "user_interface.h"
}

#define IPSET_STATIC { 192, 168, 10, 25 }
#define IPSET_GATEWAY { 192, 168, 10, 1 }
#define IPSET_SUBNET { 255, 255, 255, 0 }
#define IPSET_DNS { 192, 168, 10, 10 }

// ****************
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;
int32_t channel = WIFI_CHANNEL;
//byte bssid[] = WIFI_BSSID;
byte mqtt_server[] = MQTT_SERVER;
//
byte ip_static[] = IPSET_STATIC;
byte ip_gateway[] = IPSET_GATEWAY;
byte ip_subnet[] = IPSET_SUBNET;
byte ip_dns[] = IPSET_DNS;
// ****************


WiFiClient wifiClient;
WiFiUDP Udp;

// Time Server Port
#define NTP_PORT 123

static const int NTP_PACKET_SIZE = 48;

// buffers for receiving and sending data
byte packetBuffer[NTP_PACKET_SIZE];


//GPS instance
TinyGPS gps;

int year;
byte month, day, hour, minute, second, hundredths;
//unsigned long date, time, age;
unsigned long date, age;
uint32_t timestamp, tempval;

////////////////////////////////////////

void wifi_connect()
{
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.config(IPAddress(ip_static), IPAddress(ip_gateway), IPAddress(ip_subnet), IPAddress(ip_dns));

  int Attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Attempt++;
    if (Attempt == 300)
    {
      ESP.restart();
    }
  }

}

void setup() {
  // start the Ethernet and UDP:

  Serial.begin(4800);
  wifi_connect();
  Udp.begin(NTP_PORT);

#if debug
  Serial.print("Version:");
  Serial.println(vers);
#endif


  // Disable everything but $GPRMC
  // Note the following sentences are for UBLOX NEO6MV2 GPS
  /*
  Serial.write("$PUBX,40,GLL,0,0,0,0,0,0*5C\r\n");
  Serial.write("$PUBX,40,VTG,0,0,0,0,0,0*5E\r\n");
  Serial.write("$PUBX,40,GSV,0,0,0,0,0,0*59\r\n");
  Serial.write("$PUBX,40,GGA,0,0,0,0,0,0*5A\r\n");
  Serial.write("$PUBX,40,GSA,0,0,0,0,0,0*4E\r\n");
  */

}


////////////////////////////////////////

void loop() {

  if (getgps()) {

    gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
    if (age == TinyGPS::GPS_INVALID_AGE) {

#if debug
      Serial.println("Invalid GPS age");
#endif

    }
    else {
      processNTP();
    }
  }
}

////////////////////////////////////////

void processNTP() {

  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if (packetSize)
  {
    Udp.read(packetBuffer, NTP_PACKET_SIZE);
    IPAddress Remote = Udp.remoteIP();
    int PortNum = Udp.remotePort();

#if debug
    Serial.println();
    Serial.print("Received UDP packet size ");
    Serial.println(packetSize);
    Serial.print("From ");

    for (int i = 0; i < 4; i++)
    {
      Serial.print(Remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.print(PortNum);

    byte LIVNMODE = packetBuffer[0];
    Serial.print("  LI, Vers, Mode :");
    Serial.print(packetBuffer[0], HEX);

    byte STRATUM = packetBuffer[1];
    Serial.print("  Stratum :");
    Serial.print(packetBuffer[1], HEX);

    byte POLLING = packetBuffer[2];
    Serial.print("  Polling :");
    Serial.print(packetBuffer[2], HEX);

    byte PRECISION = packetBuffer[3];
    Serial.print("  Precision :");
    Serial.println(packetBuffer[3], HEX);

    for (int z = 0; z < NTP_PACKET_SIZE; z++) {
      Serial.print(packetBuffer[z], HEX);
      if (((z + 1) % 4) == 0) {
        Serial.println();
      }
    }
    Serial.println();

#endif


    packetBuffer[0] = 0b00100100;   // LI, Version, Mode
    packetBuffer[1] = 1 ;   // stratum
    packetBuffer[2] = 6 ;   // polling minimum
    packetBuffer[3] = 0xFA; // precision

    packetBuffer[7] = 0; // root delay
    packetBuffer[8] = 0;
    packetBuffer[9] = 8;
    packetBuffer[10] = 0;

    packetBuffer[11] = 0; // root dispersion
    packetBuffer[12] = 0;
    packetBuffer[13] = 0xC;
    packetBuffer[14] = 0;

    gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);

    timestamp = numberOfSecondsSince1900Epoch(year, month, day, hour, minute, second);

#if debug
    Serial.println(timestamp);
    Serial.println(day);
    print_date(gps);
#endif

    tempval = timestamp;

    packetBuffer[12] = 71; //"G";
    packetBuffer[13] = 80; //"P";
    packetBuffer[14] = 83; //"S";
    packetBuffer[15] = 0; //"0";

    // reference timestamp
    packetBuffer[16] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[17] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[18] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[19] = (tempval) & 0xFF;

    packetBuffer[20] = 0;
    packetBuffer[21] = 0;
    packetBuffer[22] = 0;
    packetBuffer[23] = 0;

    //copy originate timestamp from incoming UDP transmit timestamp
    packetBuffer[24] = packetBuffer[40];
    packetBuffer[25] = packetBuffer[41];
    packetBuffer[26] = packetBuffer[42];
    packetBuffer[27] = packetBuffer[43];
    packetBuffer[28] = packetBuffer[44];
    packetBuffer[29] = packetBuffer[45];
    packetBuffer[30] = packetBuffer[46];
    packetBuffer[31] = packetBuffer[47];

    //receive timestamp
    packetBuffer[32] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[33] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[34] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[35] = (tempval) & 0xFF;

    packetBuffer[36] = 0;
    packetBuffer[37] = 0;
    packetBuffer[38] = 0;
    packetBuffer[39] = 0;

    //transmitt timestamp

    packetBuffer[40] = (tempval >> 24) & 0XFF;
    tempval = timestamp;
    packetBuffer[41] = (tempval >> 16) & 0xFF;
    tempval = timestamp;
    packetBuffer[42] = (tempval >> 8) & 0xFF;
    tempval = timestamp;
    packetBuffer[43] = (tempval) & 0xFF;

    packetBuffer[44] = 0;
    packetBuffer[45] = 0;
    packetBuffer[46] = 0;
    packetBuffer[47] = 0;


    // Reply to the IP address and port that sent the NTP request

    Udp.beginPacket(Remote, PortNum);
    Udp.write(packetBuffer, NTP_PACKET_SIZE);
    Udp.endPacket();
  }
}




////////////////////////////////////////

static bool getgps()
{
  while (Serial.available())
  {
    char c = Serial.read();
#if debug
    //Serial.write(c);// GPS data flowing
#endif
    if (gps.encode(c))
      return true;
  }
  return false;
}



// original code
////////////////////////////////////////
/*
const uint8_t daysInMonth [] PROGMEM = {
  31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
}; //const or compiler complains

const unsigned long seventyYears = 2208988800UL; // to convert unix time to epoch

// NTP since 1900/01/01
static unsigned long int numberOfSecondsSince1900Epoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) {
  if (y >= 1970)
    y -= 1970;
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i)
    days += pgm_read_byte(daysInMonth + i - 1);
  if (m > 2 && y % 4 == 0)
    ++days;
  days += 365 * y + (y + 3) / 4 - 1;
  return days * 24L * 3600L + h * 3600L + mm * 60L + s + seventyYears;
}
*/

// from http://www.ntp.org/ntpfaq/NTP-s-related.htm
/*
 * Return Modified Julian Date given calendar year,
 * month (1-12), and day (1-31). See sci.astro FAQ.
 * - Valid for Gregorian dates from 17-Nov-1858.
 */

/*
long DateToMjd (int y, int m, int d)
{
    return
        367 * y
        - 7 * (y + (m + 9) / 12) / 4
        - 3 * ((y + (m - 9) / 7) / 100 + 1) / 4
        + 275 * m / 9
        + d
        + 1721028
        - 2400000;
}
*/

/*
 * Calculate number of seconds since 1-Jan-1900.
 * - Ignores UTC leap seconds.
 */

/*
__int64 SecondsSince1900 (int y, int m, int d)
{
    long Days;

    Days = DateToMjd(y, m, d) - DateToMjd(1900, 1, 1);
    return (__int64)Days * 86400;
}
*/
long DateToMjd (uint16_t y, uint8_t m, uint8_t d)
{
  return
    367 * y
    - 7 * (y + (m + 9) / 12) / 4
    - 3 * ((y + (m - 9) / 7) / 100 + 1) / 4
    + 275 * m / 9
    + d
    + 1721028
    - 2400000;
}

static unsigned long  numberOfSecondsSince1900Epoch(uint16_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t mm, uint8_t s) {
  long Days;

  Days = DateToMjd(y, m, d) - DateToMjd(1900, 1, 1);
  return (uint16_t)Days * 86400 + h * 3600L + mm * 60L + s;
}

////////////////////////////////////////

#if debug

////////////////////////////////////////


static void print_date(TinyGPS &gps)
{
  int year;
  byte month, day, hour, minute, second, hundredths;
  unsigned long age;
  gps.crack_datetime(&year, &month, &day, &hour, &minute, &second, &hundredths, &age);
  if (age == TinyGPS::GPS_INVALID_AGE)
    Serial.print(F("*******    *******    "));
  else
  {
    char sz[32];
    sprintf(sz, "%02d/%02d/%02d %02d:%02d:%02d :",
            month, day, year, hour, minute, second);
    Serial.print(sz);
  }
  print_int(age, TinyGPS::GPS_INVALID_AGE, 5);

}

////////////////////////////////////////

static void print_int(unsigned long val, unsigned long invalid, int len)
{
  char sz[32];
  if (val == invalid)
    strcpy(sz, "*******");
  else
    sprintf(sz, "%ld", val);
  sz[len] = 0;
  for (int i = strlen(sz); i < len; ++i)
    sz[i] = ' ';
  if (len > 0)
    sz[len - 1] = ' ';
  Serial.print(sz);

}



#endif
















