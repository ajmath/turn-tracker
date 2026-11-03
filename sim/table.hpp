#pragma once
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>
#include "turn/node.hpp"
using namespace turn;
struct Disk:Storage {
 Durable saved{};unsigned writes=0;bool failing=false,crash=false,afterWrite=false;unsigned cutAt=0;
 bool write(const Durable& d)override{++writes;if(failing)return false;if(cutAt==writes&&!afterWrite){crash=true;return false;}auto b=saveBytes(d);Durable restored;if(!loadBytes(b,restored))throw std::runtime_error("storage roundtrip");saved=restored;if(cutAt==writes){crash=true;return false;}return true;}
};
struct Puck {
 Mac mac{};Disk disk;std::unique_ptr<Node> core;bool down=false,powered=true;uint32_t boots=0;int32_t offset=0,ppm=0;
 uint32_t clock(uint32_t time)const{return time+uint32_t(offset)+uint32_t(int64_t(time)*ppm/1000000);}
 void restart(){++boots;core.reset(new Node(mac,0x10000+boots*31+mac[5],&disk,disk.saved));down=false;}
};
struct Delivery {uint32_t at;size_t from,to;Packet packet;};
class Table {
public:
 uint32_t time=0,seed;std::array<bool,9> visible{};std::array<unsigned,9> statuses{};std::mt19937 rng;std::vector<Puck> pucks;std::vector<Delivery> queue;std::ostringstream trace;
 std::array<std::array<bool,9>,9> blocked{};
 std::vector<std::pair<size_t,Packet>> passes;
 bool faults=false;size_t isolated=99;unsigned dropped=0;size_t failAfter=0;bool partition=false;
 explicit Table(size_t count,uint32_t s):seed(s),rng(s),pucks(count){
  for(size_t i=0;i<count;++i){auto& p=pucks[i];p.mac={2,0,17,0,0,uint8_t(i*23+7)};p.offset=int32_t(i*91);p.ppm=int32_t(i*7)*(i%2?-1:1);p.restart();}
 }
 void require(bool value,const char* message){if(!value){trace<<"FAIL t="<<time<<" "<<message<<'\n';throw std::runtime_error(message);}}
 auto sender(size_t from){return [this,from](const Mac& to,const Packet& packet){
  if(packet[3]==uint8_t(Kind::Pass)){if(passes.size()>=512)passes.erase(passes.begin());passes.push_back({from,packet});}
  for(size_t j=0;j<pucks.size();++j)if(j!=from&&(to==broadcast||to==pucks[j].mac)){
   if(blocked[from][j]||(partition&&(from==isolated||j==isolated))||!pucks[j].powered||(faults&&rng()%8==0)){++dropped;trace<<"drop "<<time<<" "<<from<<">"<<j<<" kind="<<unsigned(packet[3])<<'\n';continue;}
   if(queue.size()>=512){++dropped;continue;}
   uint32_t delay=faults?10+uint32_t(rng()%20)*10:10;
   queue.push_back({time+delay,from,j,packet});
   if(faults&&rng()%11==0&&queue.size()<512)queue.push_back({time+delay+250,from,j,packet});
  }
 };}
 void run(uint32_t duration){
  for(uint32_t elapsed=0;elapsed<duration;elapsed+=10,time+=10){
   for(size_t i=0;i<pucks.size();++i)if(pucks[i].powered){unsigned before=pucks[i].disk.writes;pucks[i].core->tick(pucks[i].clock(time),pucks[i].down,sender(i));if(before!=pucks[i].disk.writes)trace<<"persist "<<time<<" puck="<<i<<" writes="<<pucks[i].disk.writes<<" membership="<<pucks[i].disk.saved.committed.id<<" reservation="<<pucks[i].disk.saved.reservation<<'\n';if(pucks[i].disk.crash){pucks[i].disk.crash=false;reboot(i);}}
   std::vector<Delivery> due;for(auto it=queue.begin();it!=queue.end();)if(reached(time,it->at)){due.push_back(*it);it=queue.erase(it);}else ++it;
   // Per-device bounded receive queues, as in firmware; excess is dropped.
   std::vector<unsigned> processed(pucks.size());
   for(const auto& d:due)if(pucks[d.to].powered){
    if(processed[d.to]++>=16){++dropped;continue;}
    Message m;decode(d.packet.data(),d.packet.size(),pucks[d.from].mac,m);
    trace<<"rx "<<time<<" "<<d.from<<">"<<d.to<<" kind="<<unsigned(m.kind)<<" membership="<<m.membership.id<<" rev="<<m.revision<<'\n';
    unsigned before=pucks[d.to].disk.writes;
    pucks[d.to].core->receive(pucks[d.to].clock(time),pucks[d.from].mac,d.packet.data(),d.packet.size(),sender(d.to));
    if(pucks[d.to].disk.crash){pucks[d.to].disk.crash=false;reboot(d.to);}
    if(before!=pucks[d.to].disk.writes)trace<<"persist "<<time<<" puck="<<d.to<<" writes="<<pucks[d.to].disk.writes<<" reservation="<<pucks[d.to].disk.saved.reservation<<'\n';
   }
   for(size_t i=0;i<pucks.size();++i){bool led=pucks[i].powered&&pucks[i].core->led(pucks[i].clock(time));unsigned status=unsigned(pucks[i].core->status);if(visible[i]!=led||statuses[i]!=status){visible[i]=led;statuses[i]=status;trace<<"visible "<<time<<" puck="<<i<<" LED="<<led<<" status="<<status<<" revision="<<pucks[i].core->currentRevision()<<'\n';}}
   if(time%500==0){trace<<"table "<<time<<" LEDs=";for(auto& p:pucks)trace<<(p.powered&&p.core->led(p.clock(time))?'*':'.');trace<<" status=";for(auto& p:pucks)trace<<unsigned(p.core->status)<<',';trace<<" queue="<<queue.size()<<'\n';}
   for(auto& p:pucks)require(p.core->durable.committed.count<=8,"bounded membership");
  }
 }
 void tap(size_t i){trace<<"tap "<<time<<" puck="<<i<<'\n';pucks[i].down=true;run(100);pucks[i].down=false;run(100);}
 void hold(size_t i){trace<<"hold "<<time<<" puck="<<i<<'\n';pucks[i].down=true;run(3100);pucks[i].down=false;run(100);}
 void enroll(size_t i){hold(i);tap(i);tap(i);run(3000);}
 void reboot(size_t i){trace<<"restart "<<time<<" puck="<<i<<'\n';pucks[i].restart();}
 void setup(size_t count,size_t coordinator=0){
  enroll(coordinator);
  for(size_t i=0;i<count;++i){tap(i);for(unsigned wait=0;wait<30&&pucks[coordinator].disk.saved.pending.position(pucks[i].mac)<0;++wait)run(100);require(pucks[coordinator].disk.saved.pending.position(pucks[i].mac)>=0,"selection acknowledgment before next puck");run(300);}
  run(1000);hold(coordinator);run(12000);
 }
 void consistent(size_t count){auto id=pucks[0].core->durable.committed.id;require(id!=0,"membership exists");for(size_t i=0;i<count;++i){require(pucks[i].core->durable.committed.id==id,"membership convergence");require(pucks[i].core->isPlaying(),"playable convergence");require(pucks[i].core->activePosition()==pucks[0].core->activePosition(),"turn convergence");}}
 void abandoned(){
  pucks[2].powered=false;run(4000);pucks[2].powered=true;reboot(2);run(3000);
  enroll(0);tap(1);tap(2);run(35000);require(pucks[0].disk.saved.pending.count==2,"inactivity keeps selections");
  pucks[0].powered=false;run(5000);enroll(1);tap(1);tap(2);run(1000);hold(1);run(10000);
  require(pucks[1].core->isPlaying()&&pucks[2].core->isPlaying(),"missing candidate replaced without debug controls");
  pucks[0].powered=true;reboot(0);run(8000);require(!pucks[0].disk.saved.pending.id,"returning abandoned candidate retires from fresh member evidence");require(pucks[1].core->isPlaying()&&pucks[2].core->isPlaying(),"returned pending process does not restore old game");
 }
 void persistenceBoundary(){
  run(4000);enroll(0);if(!pucks[0].disk.saved.pending.id)enroll(0);
  for(size_t i=0;i<3;++i){tap(i);run(2000);if(pucks[0].disk.saved.pending.position(pucks[i].mac)<0){tap(i);run(2000);}}
  require(pucks[0].disk.saved.pending.count==3,"selection retry after power cut");
  hold(0);run(10000);
  if(!pucks[0].disk.saved.committed.id){hold(0);run(10000);}
  if(!pucks[0].core->isPlaying()&&!pucks[0].disk.saved.deciding){hold(0);run(10000);}
  consistent(3);tap(0);run(3000);require(pucks[0].core->activePosition()==1,"recovered coordinator handles requests");
 }
 void edgeScenarios(){
  // Capacity is membership capacity, not the number of discovered pucks.
  if(pucks.size()==9){run(4000);enroll(0);for(size_t i=0;i<9;++i)tap(i);run(2000);require(pucks[0].disk.saved.pending.count==8,"capacity eight");require(pucks[0].core->capacity>0,"capacity feedback");return;}
  run(4000);enroll(0);
  hold(0);require(!pucks[0].disk.saved.committed.id,"reject unselected coordinator");
  tap(0);tap(0);tap(1);run(1500);require(pucks[0].disk.saved.pending.count==2,"duplicates do not reorder");
  // Storage failure and a partition at finalization: no rollback, no extra hold.
  pucks[1].disk.failing=true;isolated=1;partition=true;hold(0);run(3000);
  require(pucks[0].disk.saved.deciding,"durable decision retained");require(!pucks[0].core->isPlaying(),"partial finalization not playable");
  reboot(0);run(1500);partition=false;run(2000);require(!pucks[0].core->isPlaying(),"failed adoption not playable");
  pucks[1].disk.failing=false;run(10000);consistent(2);
  // Claims preserve the enrollment escape while animation is scheduled.
  isolated=0;partition=true;hold(1);tap(1);tap(1);run(3000);require(pucks[1].disk.saved.pending.id!=0,"claim-to-enrollment escape");
  tap(1);tap(2);run(1500);hold(1);run(10000);
  auto replacement=pucks[1].core->durable.committed.id;
  require(pucks[1].core->isPlaying()&&pucks[2].core->isPlaying(),"new coordinator / arbitrary reuse");
  require(pucks[0].core->durable.committed.id!=replacement&&!pucks[0].core->isPlaying(),"excluded old coordinator relinquishes");
  partition=false;run(5000);require(pucks[0].core->durable.retired,"powered old coordinator retires after healing");
  reboot(0);run(5000);require(!pucks[0].core->isPlaying(),"retirement survives restart");
  // Expand the replacement again with the original coordinator as participant.
  setup(3,1);consistent(3);require(pucks[0].core->durable.committed.coordinator==pucks[1].mac,"coordinator identity is not MAC ordering");
 }
 void conflictScenario(){
  run(4000);
  for(size_t i=0;i<2;++i){pucks[i].down=true;}run(3100);
  for(size_t i=0;i<2;++i){pucks[i].down=false;}run(100);
  for(unsigned tap=0;tap<2;++tap){for(size_t i=0;i<2;++i){pucks[i].down=true;}run(100);for(size_t i=0;i<2;++i){pucks[i].down=false;}run(100);}
  run(8000);require(!pucks[0].disk.saved.pending.id&&!pucks[1].disk.saved.pending.id,"contenders withdraw");
  setup(3);consistent(3);
  Message hint;hint.sender=pucks[2].mac;hint.kind=Kind::Presence;hint.membership=pucks[0].disk.saved.committed;hint.membership.id+=99;
  auto hinted=encode(hint);auto savedId=pucks[0].disk.saved.committed.id;
  pucks[0].core->receive(pucks[0].clock(time),pucks[2].mac,hinted.data(),hinted.size(),sender(0));require(pucks[0].disk.saved.committed.id==savedId,"discovery hints do not create authority");
  Message malformed;malformed.sender=pucks[1].mac;auto bad=encode(malformed);bad[2]=99;
  auto rejectBefore=pucks[0].core->rejected;pucks[0].core->receive(pucks[0].clock(time),pucks[1].mac,bad.data(),bad.size(),sender(0));require(pucks[0].core->rejected==rejectBefore+1,"unsupported version rejected");
  Message flood;flood.sender=pucks[1].mac;auto packet=encode(flood);auto beforeDrops=dropped;
  for(unsigned i=0;i<100;++i){queue.push_back({time,1,0,packet});}
  run(2000);require(dropped>beforeDrops,"bounded receive queue overflow");consistent(3);
  auto immutableOrder=pucks[0].disk.saved.committed.order;
  for(size_t i=0;i<2;++i){pucks[i].down=true;}run(3100);for(size_t i=0;i<2;++i){pucks[i].down=false;}run(7100);
  require(pucks[0].core->activePosition()==0,"simultaneous claims serialized");require(pucks[0].disk.saved.committed.order==immutableOrder,"competing claims preserve order");
  // Asymmetric failure pauses rather than silently skipping the unreachable node.
  blocked[1][0]=true;run(5000);require(!pucks[0].core->isPlaying(),"asymmetric failure pauses");blocked[1][0]=false;run(5000);consistent(3);
 }
 void randomized(){
  run(4000);setup(pucks.size());
  for(unsigned step=0;step<30;++step){
   size_t target=size_t(rng()%pucks.size());unsigned action=rng()%4;
   trace<<"random action="<<action<<" target="<<target<<'\n';
   if(action==0)tap(target);
   else if(action==1)hold(target);
   else if(action==2)reboot(target);
   else {isolated=target;partition=true;run(3500);partition=false;}
   run(9000);
   if(!pucks[0].core->isPlaying()){hold(0);run(9000);}
   consistent(pucks.size());
  }
 }

 void scenario(){
  run(4000);setup(pucks.size());consistent(pucks.size());
  auto order=pucks[0].core->durable.committed.order;auto rev=pucks[0].core->currentRevision();
  tap(0);run(4000);consistent(pucks.size());require(pucks[0].core->activePosition()==1,"pass advances once");require(pucks[0].core->currentRevision()==rev+1,"pass revision");
  // A real hold performs claim, and the immediate two taps escape its animation.
  auto inactiveRevision=pucks[0].core->currentRevision();tap(0);run(1000);require(pucks[0].core->currentRevision()==inactiveRevision,"inactive ordinary tap does nothing");
  tap(1);run(4000);require(!passes.empty(),"participant pass request captured");
  hold(1);run(1800);tap(1);run(7000);auto beforeReplay=pucks[0].core->currentRevision();
  for(const auto& old:passes)pucks[0].core->receive(pucks[0].clock(time),pucks[old.first].mac,old.second.data(),old.second.size(),sender(0));
  run(1000);require(pucks[0].core->currentRevision()==beforeReplay&&pucks[0].core->activePosition()==1,"delayed distinct requests cannot advance later turn");
  hold(0);run(7000);consistent(pucks.size());require(pucks[0].core->activePosition()==0,"claim moves to claimant");require(pucks[0].core->durable.committed.order==order,"claim preserves order");
  reboot(1);run(6500);consistent(pucks.size());require(pucks[1].core->activePosition()==0,"participant restart recovers turn");
  isolated=1;partition=true;run(5000);require(!pucks[0].core->isPlaying(),"member loss pauses");partition=false;run(6000);consistent(pucks.size());
  reboot(0);run(9000);for(auto& p:pucks)require(!p.core->isPlaying(),"coordinator restart requires claim");hold(0);run(9000);consistent(pucks.size());
  for(size_t i=0;i<pucks.size();++i){reboot(i);}
  run(9000);for(auto& p:pucks)require(!p.core->isPlaying(),"all restart awaits claim");hold(0);run(9000);consistent(pucks.size());
  // Acknowledged pending selections survive restart before the final decision.
  enroll(0);tap(0);tap(1);run(1000);require(pucks[0].disk.saved.pending.count==2,"durable selections");reboot(0);run(2000);hold(0);run(11000);consistent(2);
  require(pucks[0].core->durable.committed.count==2,"reduction");
 }
};
