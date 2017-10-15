#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "Queue.h"

#include "/usr/local/src/hotspot_setting.h"
#include "/usr/local/src/slack_miner1_setting.h"

#define POWER_OFF 0
#define POWER_ON 1
#define DO_NOTHING 2

#define POWER_OFF_TIMER 5000
#define POWER_ON_TIMER 1000
#define DO_NOTHING_TIMER 5000

#define WORKING_STAGE_NONE 100
#define WORKING_STAGE_QUEUE 1
#define WORKING_STAGE_START 2

#define MINER_START 1
#define MINER_STOP 16

#define SELECT_ALL_MINERS 30

#define WORK_QUEUE_SIZE 64

#define FIRMWARE_VERSION "0.1.r"

struct {
  uint8_t number;
  uint8_t cmd;
  uint8_t stage;
  uint32_t tstamp;
} working_miner;

Queue<String> queue = Queue<String>(WORK_QUEUE_SIZE) ;

const char* api_fingerprint = "AC 95 5A 58 B8 4E 0B CD B3 97 D2 88 68 F5 CA C1 0A 81 E3 6E";

String msgtosend ;

/* GND ROW */
int led0 = 14;
int led1 = 12;
int led2 = 13;
int led3 = 4;

/* + COL */
int led4 = 5;
int led5 = 15;
int led6 = 16;
int led7 = 2;

WiFiClientSecure sslclient;
WebSocketsClient webSocket;

long nextCmdId = 1;
bool connected = false;

void sendPing()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "ping";
  root["id"] = nextCmdId++;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendHello()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg = "```miner reset controller : ";
  msg += FIRMWARE_VERSION;
  msg += "\n - miner from ";
  msg += MINER_START;
  msg += " to ";
  msg += MINER_STOP;
  msg += "\n - max queue size : ";
  msg += WORK_QUEUE_SIZE;
  msg += "```";
  root["text"] = msg;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendHelp()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;

  String msg =  "```[poff | pon | reboot] number --> power off / power on / reboot miner #number\n";
  msg += "[showdemo | alloff] --> show demo / power off all\n";
  msg += "miner number : ";
  msg += MINER_START;
  msg += " ~ ";
  msg += MINER_STOP;
  msg += "\nworking queue size : ";
  msg += queue.count();
  msg += "```";
  root["text"] = msg;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);
}

void sendmsg()
{
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["type"] = "message";
  root["id"] = nextCmdId++;
  root["channel"] = SLACK_CHANNEL;
  root["text"] = msgtosend;
  String json;
  root.printTo(json);
  webSocket.sendTXT(json);  
}

void ICACHE_RAM_ATTR processSlackMessage(String receivedpayload)
{
  char json[] = "{\"type\":\"message\",\"user\":\"XX0990XX\",\"text\":\"XX0990XX\",\"team\":\"XX0990XX\",\"user_team\":\"XX0990XX\",\"bot_id\":\"XX0990XX\",\"subtype\":\"bot_message\",\"username\":\"XX0990XX\",\"user_profile\":{\"avatar_hash\":\"g9c0531af70a\",\"image_72\":\"https://secure.gravatar.com/avatar/9c0531af70ad0dbed618e9f4c2198145.jpg?s=72&d=https%3A%2F%2Fa.slack-edge.com%2F66f9%2Fimg%2Favatars%2Fava_0019-72.png\",\"first_name\":null,\"real_name\":\"\",\"name\":\"XX0990XX\"},\"channel\":\"XX0990XX\",\"ts\":\"1466954463.000275\"}";
  receivedpayload.toCharArray(json, 1024);
  StaticJsonBuffer<1024> jsonBuffer;
  JsonObject& root = jsonBuffer.parseObject(json);

  if (!root.success()) {
    Serial.println("root failed");
    return;
  }

  if (root.containsKey("text"))
  {
    String receivedtext = root["text"];
    if (receivedtext.startsWith("help"))
    {
      sendHelp();
    }
    else if (receivedtext.startsWith("showdemo"))
    {
      msgtosend = "```working queue size : ";
      msgtosend += queue.count();
      msgtosend += "\nClearing all working queues\nshowing demo```";
      sendmsg(); 
      queue.clear();     
      showdemo();
    }
    else if (receivedtext.startsWith("alloff"))
    {
      msgtosend = "```working queue size : ";
      msgtosend += queue.count();
      msgtosend += "\nClearing all working queues\npower off all miners```";
      sendmsg(); 
      queue.clear();
      String que_cmd_in = String(SELECT_ALL_MINERS) + "-0";     
      queue.push(que_cmd_in);     
    }  
    else if (receivedtext.length() > 4)
    {
      int firstwhitespace = receivedtext.indexOf(' ');
      if (firstwhitespace > 0)
      {
        String miner_cmd = receivedtext.substring(0, firstwhitespace);
        int miner_number = receivedtext.substring(firstwhitespace + 1).toInt();
        if (miner_number > 0)
        {
          if (miner_number >= MINER_START &&  miner_number <= MINER_STOP)
          {
            int count = queue.count();
            if (count > WORK_QUEUE_SIZE -3 ) 
            {
              msgtosend = "*queue is full : ";
              msgtosend += receivedtext;
              msgtosend += " dropped*";
              sendmsg();
            }
            else
            {
              Serial.println("[que_cmd_in] cmd: " +  miner_cmd);
              Serial.printf("[que_cmd_in] number: %d\n", miner_number);

              String que_cmd_in = String(miner_number - MINER_START) + "-";
              Serial.println("[que_cmd_in] " +  que_cmd_in + miner_cmd);

              if (miner_cmd.startsWith("poff"))
              {
                queue.push(que_cmd_in + "0");
                msgtosend = "```";
                msgtosend += receivedtext;
                msgtosend += " is in queue - queue size : ";
                msgtosend += queue.count();
                msgtosend += "```";
                sendmsg();                
              }
              else if (miner_cmd.startsWith("pon"))
              {
                queue.push(que_cmd_in + "1");
                msgtosend = "```";
                msgtosend += receivedtext;
                msgtosend += " is in queue - queue size : ";
                msgtosend += queue.count();
                msgtosend += "```";
                sendmsg();                  
              }
              else if (miner_cmd.startsWith("reboot"))
              {
                queue.push(que_cmd_in + "0");
                queue.push(que_cmd_in + "2");
                queue.push(que_cmd_in + "1");
                msgtosend = "```";
                msgtosend += receivedtext;
                msgtosend += " is in queue - queue size : ";
                msgtosend += queue.count();
                msgtosend += "```";
                sendmsg();                  
              }
              else
              {
                msgtosend = "```";
                msgtosend += receivedtext;
                msgtosend += " is ignored```";
                sendmsg();  
              }              
            }
          }
        }
      }
    }
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t len)
{
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WebSocket] Disconnected :-( \n");
      connected = false;
      break;

    case WStype_CONNECTED:
      Serial.printf("[WebSocket] Connected to: %s\n", payload);
      sendPing();
      webSocket.loop();
      sendHello();
      break;

    case WStype_TEXT:
      
      //Serial.printf("[WebSocket] Message: %s\n", payload);
      
      String receivedpayload;
      for (int i = 0; i < len; i++)
      {
        receivedpayload += (char)payload[i];
      }
      
      if (receivedpayload.startsWith("{\"type\":\"message") || receivedpayload.startsWith("{\"text\":\""))
      {
        if (receivedpayload.indexOf(SLACK_CHANNEL) != -1 &&
            receivedpayload.indexOf(SLACK_TEAM) != -1 )
        {
          if (len < 1024)
          {
            processSlackMessage(receivedpayload);
          }
        }
      }
      break;
  }
}


bool connectToSlack()
{
  HTTPClient http;
  String uri_to_post = "/api/rtm.start?token=";
  uri_to_post += SLACK_BOT_TOKEN;

  http.begin("slack.com", 443, uri_to_post, api_fingerprint);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("HTTP GET failed with code %d\n", httpCode);
    return false;
  }

  WiFiClient *client = http.getStreamPtr();
  client->find("wss:\\/\\/");
  String host = client->readStringUntil('\\');
  String path = client->readStringUntil('"');
  path.replace("\\/", "/");

  Serial.println("WebSocket Host=" + host + " Path=" + path);
  webSocket.beginSSL(host, 443, path, "", "");
  webSocket.onEvent(webSocketEvent);
  return true;
}

void off() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void allon() {
  digitalWrite(led0, LOW);
  digitalWrite(led1, LOW);
  digitalWrite(led2, LOW);
  digitalWrite(led3, LOW);

  digitalWrite(led4, HIGH);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, HIGH);  
}

void a() {
  digitalWrite(led0, LOW);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, HIGH);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void b() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, HIGH);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void c() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, HIGH);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void d() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);

  digitalWrite(led4, HIGH);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void e() {
  digitalWrite(led0, LOW);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void f() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void g() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void h() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);

  digitalWrite(led4, LOW);
  digitalWrite(led5, HIGH);
  digitalWrite(led6, LOW);
  digitalWrite(led7, LOW);
}

void i() {
  digitalWrite(led0, LOW);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, LOW);
}

void j() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, LOW);
}

void k() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, LOW);
}

void l() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, HIGH);
  digitalWrite(led7, LOW);
}

void m() {
  digitalWrite(led0, LOW);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, HIGH);
}

void n() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, LOW);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, HIGH);
}

void o() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, LOW);
  digitalWrite(led3, HIGH);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, HIGH);
}

void p() {
  digitalWrite(led0, HIGH);
  digitalWrite(led1, HIGH);
  digitalWrite(led2, HIGH);
  digitalWrite(led3, LOW);

  digitalWrite(led4, LOW);
  digitalWrite(led5, LOW);
  digitalWrite(led6, LOW);
  digitalWrite(led7, HIGH);
}

void led_process()
{
  switch (working_miner.number) {
    case 0:
      a();
      break;

    case 1:
      b();
      break;

    case 2:
      c();
      break;

    case 3:
      d();
      break;

    case 4:
      e();
      break;

    case 5:
      f();
      break;

    case 6:
      g();
      break;

    case 7:
      h();
      break;

    case 8:
      i();
      break;

    case 9:
      j();
      break;

    case 10:
      k();
      break;

    case 11:
      l();
      break;

    case 12:
      m();
      break;

    case 13:
      n();
      break;

    case 14:
      o();
      break;

    case 15:
      p();
      break;

    case 30:
      allon();
      break;    
  }
}

void showdemo()
{
  for (int i = MINER_START; i <= MINER_STOP; i++) {
    String que_cmd_in = String(i - MINER_START) + "-";
    queue.push(que_cmd_in + "1");
  }  
  allon();
}

void que_process()
{
  if (working_miner.stage == WORKING_STAGE_NONE ) {
    int count = queue.count();
    if (count > 0) {
      String que_cmd = queue.pop();

      Serial.println("[que_pop] items : " + que_cmd);

      int firstminus = que_cmd.indexOf('-');

      if (firstminus > 0)
      {
        int miner_number = que_cmd.substring(0, firstminus).toInt();
        int work_cmd = que_cmd.substring(firstminus + 1).toInt();

        Serial.printf("[que_pop] miner_number %d\n", miner_number);
        Serial.printf("[que_pop] work_cmd %d\n", work_cmd);

        working_miner.stage = 1;
        working_miner.number = miner_number;
        working_miner.cmd = work_cmd;
        working_miner.tstamp = millis();
      }
    }
  }
}


void work_process()
{
  int count = queue.count();
  switch (working_miner.stage) {
    case WORKING_STAGE_QUEUE:
      switch (working_miner.cmd) {
        case POWER_OFF:
          led_process();
          working_miner.stage = WORKING_STAGE_START;
          working_miner.tstamp = millis();
          Serial.printf("[working] start work power off %d\n", working_miner.number);
          break;

        case POWER_ON:
          led_process();
          working_miner.stage = WORKING_STAGE_START;
          working_miner.tstamp = millis();
          Serial.printf("[working] start work power on %d\n", working_miner.number);
          break;

        case DO_NOTHING:
          working_miner.stage = WORKING_STAGE_START;
          working_miner.tstamp = millis();
          Serial.printf("[working] start work wait on %d\n", working_miner.number);
          break;
      }

    case WORKING_STAGE_START:
      switch (working_miner.cmd) {
        case POWER_OFF:
          if ( millis() - working_miner.tstamp > POWER_OFF_TIMER ) {
            off();
            working_miner.stage = WORKING_STAGE_NONE;
            working_miner.tstamp = millis();

            msgtosend = "```power_off : ";
            if (working_miner.number == SELECT_ALL_MINERS) 
            {
              msgtosend += "all miners" ;
            }
            else
            {
              msgtosend += working_miner.number + MINER_START ;
            }
            msgtosend += " complete - queue size : ";
            msgtosend += count;
            msgtosend += "```";
            sendmsg();

            Serial.printf("[working] stop work power off %d\n", working_miner.number);
          }
          break;

        case POWER_ON:
          if ( millis() - working_miner.tstamp > POWER_ON_TIMER ) {
            off();
            working_miner.stage = WORKING_STAGE_NONE;
            working_miner.tstamp = millis();

            msgtosend = "```power_on : ";
            if (working_miner.number == SELECT_ALL_MINERS) 
            {
              msgtosend += "all miners" ;
            }
            else
            {            
              msgtosend += working_miner.number + MINER_START ;
            }
            msgtosend += " complete - queue size : ";
            msgtosend += count;
            msgtosend += "```";
            sendmsg();

            Serial.printf("[working] stop work power on %d\n", working_miner.number);
          }
          break;

        case DO_NOTHING:
          if ( millis() - working_miner.tstamp > DO_NOTHING_TIMER ) {
            working_miner.stage = WORKING_STAGE_NONE;
            working_miner.tstamp = millis();
            Serial.printf("[working] stop work wait on %d\n", working_miner.number);
          }
          break;
      }

      break;

    case WORKING_STAGE_NONE:
      break;

  }
}

void setup()
{

  pinMode(led0, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  pinMode(led4, OUTPUT);
  pinMode(led5, OUTPUT);
  pinMode(led6, OUTPUT);
  pinMode(led7, OUTPUT);
  off();
  
  Serial.begin(115200);
  Serial.println("\nStarting esp...");

  if (MINER_STOP - MINER_START > 16 ||  MINER_STOP < MINER_START)
  {
    Serial.print("config errors on miners number");
    delay(10000);
    ESP.reset();
  }

  working_miner.number = WORKING_STAGE_NONE;
  working_miner.cmd    = WORKING_STAGE_NONE;
  working_miner.stage  = WORKING_STAGE_NONE;
  working_miner.tstamp = WORKING_STAGE_NONE;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(". ");
    delay(100);
  }
  Serial.println("\nWireless connected\n");

  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
}

unsigned long lastPing = 0;

void loop()
{
  webSocket.loop();

  if (connected) {
    if (millis() - lastPing > 5000)
    {
      sendPing();
      lastPing = millis();
    }
  }
  else
  {
    connected = connectToSlack();
    if (!connected)
    {
      delay(500);
    }
  }

  que_process();
  work_process();
}

// end
