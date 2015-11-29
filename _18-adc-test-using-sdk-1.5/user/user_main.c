#include "ets_sys.h"
#include "osapi.h"
#include "user_interface.h"
os_timer_t adc_test_t;

void adc_test()
{
    int sampleI = 0;
    int Number_of_Samples = 1500;
    int sumI = 0;
    int count = 0;
    uint32 s, t;

    s = system_get_time(); 
  
    while ( count < Number_of_Samples )
    {
        sampleI = system_adc_read();
        sumI += sampleI;
        count++;
    }
    
    t = system_get_time(); 

    os_printf( "total delay (1500 samples) in adc / ms : %d", (t - s)/1000);
    os_printf( " --> average delay of adc / us : %d", (t - s)/Number_of_Samples);
    os_printf( " --> average of analogRead : %d\n", ( sumI / Number_of_Samples)) ;
}


void user_init(void)
{
    os_printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_opmode(STATION_MODE);
    adc_test();
    os_timer_disarm(&adc_test_t);
    os_timer_setfn(&adc_test_t,adc_test,NULL);
    os_timer_arm(&adc_test_t,1000,1);
}


/* with wifi_set_opmode(NULL_MODE);

n  8 2013,rst cause:2, boot mode:(3,7)

load 0x40100000, len 26524, room 16 
tail 12
chksum 0xba
ho 0 tail 12 room 4
load 0x3ffe8000, len 884, room 12 
tail 8
chksum 0xde
load 0x3ffe8378, len 388, room 0 
tail 4
chksum 0x1a
csum 0x1a
�q���*�nE����n=�
SDK ver: 1.5.0 compiled @ Nov 27 2015 13:37:51
phy ver: 484, pp ver: 9.5

SDK version:1.5.0
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
mode : null
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299

*/


/* with wifi_set_opmode(STATION_MODE);

ets Jan  8 2013,rst cause:2, boot mode:(3,7)

load 0x40100000, len 26524, room 16 
tail 12
chksum 0xaa
ho 0 tail 12 room 4
load 0x3ffe8000, len 884, room 12 
tail 8
chksum 0xce
load 0x3ffe8378, len 388, room 0 
tail 4
chksum 0x0a
csum 0x0a
�q���*��E����n=�
SDK ver: 1.5.0 compiled @ Nov 27 2015 13:37:51
phy ver: 484, pp ver: 9.5

SDK version:1.5.0
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
mode : sta(18:fe:34:a0:19:8c)
add if0
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
scandone
state: 0 -> 2 (b0)
state: 2 -> 3 (0)
state: 3 -> 5 (10)
add 0
aid 7
cnt 

connected with RSPBRRY, channel 10
dhcp client start...
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
ip:192.168.10.112,mask:255.255.255.0,gw:192.168.10.1
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
pm open,type:2 0
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 60 --> average delay of adc / us : 40 --> average of analogRead : 305
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 299
total delay (1500 samples) in adc / ms : 141 --> average delay of adc / us : 94 --> average of analogRead : 300

*/
