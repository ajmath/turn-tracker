#include <cassert>
#include <iostream>
#include "turn/node.hpp"
using namespace turn;
int main(){
 Mac mac={2,9,0,0,0,17};Message m{Kind::Discover,mac,0xffffffff,123};auto p=encode(m);Message decoded;
 assert(decode(p.data(),p.size(),mac,decoded));assert(decoded.boot==m.boot&&decoded.sequence==123);
 for(size_t n=0;n<p.size();++n)assert(!decode(p.data(),n,mac,decoded));
 auto bad=p;bad[2]=2;assert(!decode(bad.data(),bad.size(),mac,decoded));
 Mac other=mac;other[5]++;assert(!decode(p.data(),p.size(),other,decoded));
 Button b;auto tick=[&](uint32_t t,bool d,Mode mode=Mode::Normal){return b.tick(t,d,mode);};
 tick(0,false);tick(10,true);assert(tick(39,true)==Gesture::None);tick(40,true);
 tick(50,false);assert(tick(80,false)==Gesture::Tap);
 tick(100,true);tick(130,true);assert(tick(3129,true)==Gesture::None);assert(tick(3130,true)==Gesture::Threshold);
 tick(3140,false);assert(tick(3170,false)==Gesture::Claim);
 tick(3200,true,Mode::Animation);tick(3230,true,Mode::Animation);tick(3250,false,Mode::Animation);assert(tick(3280,false,Mode::Animation)==Gesture::None);
 tick(3300,true,Mode::Animation);tick(3330,true,Mode::Animation);tick(3350,false,Mode::Animation);assert(tick(3380,false,Mode::Animation)==Gesture::Enroll);
 Button held;held.tick(0,true,Mode::Normal);held.tick(4000,true,Mode::Normal);held.tick(4010,false,Mode::Normal);assert(held.tick(4040,false,Mode::Normal)==Gesture::None);
 Button wrap;wrap.tick(0xffffff00,false,Mode::Normal);wrap.tick(0xfffffff0,true,Mode::Normal);wrap.tick(20,true,Mode::Normal);wrap.tick(40,false,Mode::Normal);assert(wrap.tick(70,false,Mode::Normal)==Gesture::Tap);
 Node node(mac,1);unsigned sends=0;auto send=[&](const Mac&,const Packet&){++sends;};node.tick(0,false,send);assert(sends==1);node.tick(2000,false,send);assert(sends==1);node.tick(2500,false,send);assert(sends==2);
 // Persistent records use an explicit checksum and survive byte roundtrips.
 Durable durable;durable.pending.id=42;durable.pending.coordinator=mac;durable.pending.order[0]=mac;durable.pending.count=1;durable.reservation=42;durable.candidate=mac;
 Durable restored;auto bytes=saveBytes(durable);assert(loadBytes(bytes,restored));assert(restored.pending.count==1&&restored.reservation==42);
 bytes[70]^=1;assert(!loadBytes(bytes,restored));
 // An escape-window press released after expiration must not pass a turn.
 Button boundary({10,100,50});boundary.tick(0,false,Mode::Normal);boundary.tick(10,true,Mode::Normal);boundary.tick(20,true,Mode::Normal);assert(boundary.tick(120,true,Mode::Normal)==Gesture::Threshold);boundary.tick(130,false,Mode::Normal);assert(boundary.tick(140,false,Mode::Normal)==Gesture::Claim);
 boundary.tick(150,true,Mode::Normal);boundary.tick(160,true,Mode::Normal);boundary.tick(185,false,Mode::Normal);assert(boundary.tick(195,false,Mode::Normal)==Gesture::None);
 // Finishing hold on another device is consumed without claim or nested setup.
 Button participant({10,100,50});participant.tick(0,false,Mode::Enrollment);participant.tick(10,true,Mode::Enrollment);participant.tick(20,true,Mode::Enrollment);participant.tick(120,true,Mode::Enrollment);participant.tick(130,false,Mode::Enrollment);assert(participant.tick(140,false,Mode::Enrollment)==Gesture::None);
 // Presses spanning enrollment and animation boundaries cannot leak actions.
 Button modes({10,100,50});modes.tick(0,false,Mode::Normal);modes.tick(10,true,Mode::Normal);modes.tick(20,true,Mode::Normal);modes.tick(30,false,Mode::Enrollment);assert(modes.tick(40,false,Mode::Enrollment)==Gesture::None);
 Button animationTap({10,100,50});animationTap.tick(0,false,Mode::Animation);animationTap.tick(10,true,Mode::Animation);animationTap.tick(20,true,Mode::Animation);animationTap.tick(30,false,Mode::Normal);assert(animationTap.tick(40,false,Mode::Normal)==Gesture::None);
 // Debounced hold boundaries use inclusive equality, independent of clock wrap.
 for(uint32_t origin:{100u,0xffffff00u})for(uint32_t duration:{99u,100u,101u}){
  Button edge({10,100,50});edge.tick(origin,false,Mode::Normal);edge.tick(origin+10,true,Mode::Normal);edge.tick(origin+20,true,Mode::Normal);
  edge.tick(origin+10+duration,false,Mode::Normal);
  auto result=edge.tick(origin+20+duration,false,Mode::Normal);assert(result==(duration>=100?Gesture::Claim:Gesture::Tap));
 }
 std::cout<<"core: wire validation, gestures, startup, wraparound, discovery passed\n";
}
