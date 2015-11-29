#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_interface.h"
#include "user_config.h"


struct espconn dweet_conn;
ip_addr_t dweet_ip;
esp_tcp dweet_tcp;

char dweet_host[] = "dweet.io";
char dweet_path[] = "/dweet/for/eccd882c-33d0-11e5-96b7-10bf4884d1f9";
char json_data[ 256 ];
char buffer[ 2048 ];


void user_rf_pre_init( void )
{
}


void data_received( void *arg, char *pdata, unsigned short len )
{
    struct espconn *conn = arg;
    
    os_printf( "%s: %s\n", __FUNCTION__, pdata );
    
    espconn_disconnect( conn );
}


void tcp_connected( void *arg )
{
    int temperature = 55;   // test data
    struct espconn *conn = arg;
    
    os_printf( "%s\n", __FUNCTION__ );
    espconn_regist_recvcb( conn, data_received );

    os_sprintf( json_data, "{\"temperature\": \"%d\" }", temperature );
    os_sprintf( buffer, "POST %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %d\r\n\r\n%s", 
                         dweet_path, dweet_host, os_strlen( json_data ), json_data );
    
    os_printf( "Sending: %s\n", buffer );
    espconn_sent( conn, buffer, os_strlen( buffer ) );
}


void tcp_disconnected( void *arg )
{
    struct espconn *conn = arg;
    
    os_printf( "%s\n", __FUNCTION__ );
    wifi_station_disconnect();
}


void dns_done( const char *name, ip_addr_t *ipaddr, void *arg )
{
    struct espconn *conn = arg;
    
    os_printf( "%s\n", __FUNCTION__ );
    
    if ( ipaddr == NULL) 
    {
        os_printf("DNS lookup failed\n");
        wifi_station_disconnect();
    }
    else
    {
        os_printf("Connecting...\n" );
        
        conn->type = ESPCONN_TCP;
        conn->state = ESPCONN_NONE;
        conn->proto.tcp=&dweet_tcp;
        conn->proto.tcp->local_port = espconn_port();
        conn->proto.tcp->remote_port = 80;
        os_memcpy( conn->proto.tcp->remote_ip, &ipaddr->addr, 4 );

        espconn_regist_connectcb( conn, tcp_connected );
        espconn_regist_disconcb( conn, tcp_disconnected );
        
        espconn_connect( conn );
    }
}


void wifi_callback( System_Event_t *evt )
{
    os_printf( "%s: %d\n", __FUNCTION__, evt->event );
    
    switch ( evt->event )
    {
        case EVENT_STAMODE_CONNECTED:
        {
            os_printf("connect to ssid %s, channel %d\n",
                        evt->event_info.connected.ssid,
                        evt->event_info.connected.channel);
            break;
        }

        case EVENT_STAMODE_DISCONNECTED:
        {
            os_printf("disconnect from ssid %s, reason %d\n",
                        evt->event_info.disconnected.ssid,
                        evt->event_info.disconnected.reason);
            
            deep_sleep_set_option( 0 );
            system_deep_sleep( 60 * 1000 * 1000 );  // 60 seconds
            break;
        }

        case EVENT_STAMODE_GOT_IP:
        {
            os_printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR,
                        IP2STR(&evt->event_info.got_ip.ip),
                        IP2STR(&evt->event_info.got_ip.mask),
                        IP2STR(&evt->event_info.got_ip.gw));
            os_printf("\n");
            
            espconn_gethostbyname( &dweet_conn, dweet_host, &dweet_ip, dns_done );
            break;
        }
        
        default:
        {
            break;
        }
    }
}


void user_init( void )
{
    static struct station_config config;
    
    uart_div_modify( 0, UART_CLK_FREQ / ( 115200 ) );
    os_printf( "%s\n", __FUNCTION__ );

    wifi_station_set_hostname( "dweet" );
    wifi_set_opmode_current( STATION_MODE );

    gpio_init();
    
    config.bssid_set = 0;
    os_memcpy( &config.ssid, "TESTAP", 32 );
    os_memcpy( &config.password, "password", 64 );
    wifi_station_set_config( &config );
    
    wifi_set_event_handler_cb( wifi_callback );
}


/* log

ruser_init
mode : sta(18:fe:34:a0:19:8c)
add if0
scandone
state: 0 -> 2 (b0)
state: 2 -> 3 (0)
state: 3 -> 5 (10)
add 0
aid 1

connected with TESTAP, channel 1
dhcp client start...
cnt 
wifi_callback: 0
connect to ssid TESTAP, channel 1
ip:192.168.10.112,mask:255.255.255.0,gw:192.168.10.1
wifi_callback: 3
ip:192.168.10.112,mask:255.255.255.0,gw:192.168.10.1
dns_done
Connecting...
tcp_connected
Sending: POST /dweet/for/eccd882c-33d0-11e5-96b7-10bf4884d1f9 HTTP/1.1
Host: dweet.io
Connection: close
Content-Type: application/json
Content-Length: 22

{"temperature": "55" }
data_received: HTTP/1.1 200 OK
Access-Control-Allow-Origin: *
Content-Type: application/json
Content-Length: 172
Date: Sun, 29 Nov 2015 20:07:07 GMT
Connection: close

{"this":"succeeded","by":"dweeting","the":"dweet","with":{"thing":"eccd882c-33d0-11e5-96b7-10bf4884d1f9","created":"2015-11-29T20:07:07.786Z","content":{"temperature":55}}}
tcp_disconnected
state: 5 -> 0 (0)
rm 0
wifi_callback: 1
disconnect from ssid TESTAP, reason 8
del if0
usl

*/

