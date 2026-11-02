#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <atomic>
#include <esp_now.h>
#include <esp_wifi.h>
#include "turn/diagnostics.hpp"
#ifndef BUTTON_PIN
#define BUTTON_PIN 4
#endif
#ifndef LED_PIN
#define LED_PIN 5
#endif
#ifndef LED_ACTIVE_HIGH
#define LED_ACTIVE_HIGH 1
#endif
#ifndef RADIO_CHANNEL
#define RADIO_CHANNEL 1
#endif
static_assert(RADIO_CHANNEL>=1&&RADIO_CHANNEL<=11,"choose a region-appropriate common channel");
// GPIO4/5 = D2/D3. External driver required; no direct illuminated-button drive.
static_assert(ESP_NOW_MAX_TOTAL_PEER_NUM>=9,"8 players plus broadcast must fit");
static_assert(ESP_NOW_MAX_DATA_LEN>=turn::wireSize,"payload limit");
struct Input {turn::Mac source;turn::Packet bytes;};
Preferences preferences;
struct NvsStorage:turn::Storage {
 bool write(const turn::Durable& record)override {
  auto bytes=turn::saveBytes(record);
  return preferences.putBytes("state",bytes.data(),bytes.size())==bytes.size();
 }
} storage;
QueueHandle_t inbox;turn::Node* node=nullptr;std::atomic<uint32_t> dropped{0};uint32_t sendErrors=0;
void received(const uint8_t* mac,const uint8_t* data,int length) {
 if(length!=int(turn::wireSize)||data[0]!='T'||data[1]!='T'||data[2]!=1)return;
 Input input;memcpy(input.source.data(),mac,6);memcpy(input.bytes.data(),data,turn::wireSize);
 if(xQueueSend(inbox,&input,0)!=pdTRUE)++dropped;
}
void sendPacket(const turn::Mac& destination,const turn::Packet& packet) {
 if(!esp_now_is_peer_exist(destination.data())) {
  esp_now_peer_info_t peer{};memcpy(peer.peer_addr,destination.data(),6);peer.channel=RADIO_CHANNEL;peer.ifidx=WIFI_IF_STA;peer.encrypt=false;
  if(esp_now_add_peer(&peer)!=ESP_OK){
   // Discovery can encounter arbitrarily many historical devices. Evict a
   // unicast cache entry; broadcast always remains installed.
   esp_now_peer_info_t old{};
   if(esp_now_fetch_peer(true,&old)==ESP_OK&&memcmp(old.peer_addr,turn::broadcast.data(),6)!=0)esp_now_del_peer(old.peer_addr);
   if(esp_now_add_peer(&peer)!=ESP_OK){++sendErrors;return;}
  }
 }
 if(esp_now_send(destination.data(),packet.data(),packet.size())!=ESP_OK)++sendErrors;
}
void setup() {
 pinMode(LED_PIN,OUTPUT);digitalWrite(LED_PIN,!LED_ACTIVE_HIGH);
 pinMode(BUTTON_PIN,INPUT_PULLUP);Serial.begin(115200);
 if(!WiFi.mode(WIFI_STA)||esp_wifi_set_channel(RADIO_CHANNEL,WIFI_SECOND_CHAN_NONE)!=ESP_OK){Serial.println("{\"error\":\"channel_init\"}");return;}
 WiFi.setSleep(false);
 turn::Mac mac{};WiFi.macAddress(mac.data());turn::Durable saved{};preferences.begin("turn",false);
 turn::StorageBytes bytes{};
 if(preferences.getBytesLength("state")==bytes.size()){
  preferences.getBytes("state",bytes.data(),bytes.size());
  if(!turn::loadBytes(bytes,saved))Serial.println("{\"error\":\"storage_corrupt\"}");
 }
 static turn::Node instance(mac,esp_random(),&storage,saved);node=&instance;
 inbox=xQueueCreate(16,sizeof(Input));
 if(!inbox||esp_now_init()!=ESP_OK){Serial.println("{\"error\":\"radio_init\"}");node=nullptr;return;}
 esp_now_register_recv_cb(received);
}
void loop() {
 if(!node){digitalWrite(LED_PIN,millis()%300<100?LED_ACTIVE_HIGH:!LED_ACTIVE_HIGH);return;}
 const auto now=millis();Input input;
 for(unsigned i=0;i<16&&xQueueReceive(inbox,&input,0)==pdTRUE;++i)
  node->receive(now,input.source,input.bytes.data(),input.bytes.size(),sendPacket);
 node->tick(now,digitalRead(BUTTON_PIN)==LOW,sendPacket);
 digitalWrite(LED_PIN,node->led(now)?LED_ACTIVE_HIGH:!LED_ACTIVE_HIGH);
 static char command[32]{};static uint8_t used=0;static bool overflow=false;
 for(unsigned consumed=0;consumed<32&&Serial.available();++consumed){
  char c=char(Serial.read());if(c=='\r')continue;
  if(c=='\n'){
   if(overflow){overflow=false;used=0;continue;}
   command[used]=0;
   if(strcmp(command,"forget")==0)node->forget(now);
   else if(strncmp(command,"number ",7)==0){char* end=nullptr;long value=strtol(command+7,&end,10);if(end&&!*end&&value>=0&&value<=65535)node->number(uint16_t(value),now);}
   else if(strcmp(command,"log")==0)turn::eventLog(*node,[](const char* line){Serial.print(line);});
   else {
    turn::snapshot(*node,[](const char* line){Serial.print(line);});
    Serial.printf("{\"queue_drops\":%u,\"send_errors\":%u,\"rejected\":%u,\"capacity\":%u}\n",dropped.load(),sendErrors,node->rejected,node->capacity);
   }
   used=0;
  }else if(!overflow&&used<sizeof(command)-1)command[used++]=c;
  else overflow=true;
 }
}
