#pragma once
#include "gesture.hpp"
#include "storage.hpp"
namespace turn {
constexpr uint32_t heartbeatMs=500,leaseMs=3000,startupMs=3500;
struct AnimationTiming {uint16_t enrollment=1000,claim=500;uint32_t lead=1500,flash=200;};
inline bool reached(uint32_t now,uint32_t deadline){return int32_t(now-deadline)>=0;}
enum class Status { Discovering, Unassigned, Enrollment, Selected, Finishing, WaitingClaim, Playing, Paused, Conflict, StorageError };
struct Peer {Mac mac{};uint32_t lastSeen=0,boot=0;bool used=false;};
struct Event {uint32_t time=0;uint8_t code=0;uint32_t value=0;};
class Node {
 AnimationTiming timing;Mac self;uint32_t boot,sequence=0,lastPulse=0,lastDiscovery=0,started=0;bool first=true;
 Storage* storage=nullptr;uint32_t serial=0;
 uint64_t invitation=0;Mac inviter{};uint32_t inviteSeen=0,conflictUntil=0;
 uint32_t session=0,revision=0,lastAuthority=0;uint8_t active=255;
 std::array<uint32_t,8> seen{};std::array<bool,8> acknowledged{};
 bool confirmed=false,playing=false,conflict=false,selectionConfirmed=false;
 uint32_t animation=0,animationStart=0,completedAnimation=0;uint16_t animationInterval=0;uint8_t animationPosition=0;
 int32_t offset=0;uint32_t feedbackUntil=0,enrollmentActivity=0;bool threshold=false,feedbackError=false;
 bool requestPending=false;Kind requestKind=Kind::Pass;uint32_t requestId=0,requestRevision=0,lastRequest=0;uint8_t requestBurst=0;
 uint32_t conflictNonce=0,challenge=0,inviteQuery=0,previousQuery=0,selectionBarrier=0;
 uint64_t fresh(){return (uint64_t(boot)<<32)|++serial;}
 bool persist(const Durable& next,uint32_t now){
  if(storage&&!storage->write(next)){++persistenceErrors;status=Status::StorageError;playing=false;log(now,9);return false;}
  durable=next;if(status==Status::StorageError)status=Status::Paused;return true;
 }
 void log(uint32_t now,uint8_t code,uint32_t value=0){events[eventNext++%events.size()]={now,code,value};}
 Message base(Kind kind,uint32_t now){Message m;m.kind=kind;m.sender=self;m.boot=boot;m.sequence=sequence++;m.stamp=now;return m;}
 bool coordinator()const{return durable.committed.id&&durable.committed.coordinator==self;}
 bool candidate()const{return durable.reservation&&durable.candidate==self&&durable.pending.id==durable.reservation;}
 bool allSeen(uint32_t now)const {
  for(uint8_t i=0;i<durable.committed.count;++i)if(durable.committed.order[i]!=self&&(!acknowledged[i]||uint32_t(now-seen[i])>=leaseMs))return false;
  return true;
 }
 template<class Send> void state(uint32_t now,Send send,const Mac& destination=broadcast){
  Message m=base(Kind::State,now);m.membership=durable.committed;m.session=session;m.revision=revision;m.active=active;m.flags=uint8_t((playing?1:0)|(durable.deciding?2:0));
  m.animation=animation;m.start=animationStart;m.interval=animationInterval;send(destination,encode(m));
 }
 template<class Send> void commit(uint32_t now,Send send){Message m=base(Kind::Commit,now);m.membership=durable.committed;m.enrollment=durable.committed.id;m.session=session;send(broadcast,encode(m));}
 template<class Send> void sync(uint32_t now,Send send){
  Message m=base(Kind::Sync,now);m.membership=durable.committed;m.session=session;m.revision=revision;if(!challenge)challenge=(++serial)^boot;m.request=challenge;send(durable.committed.coordinator,encode(m));
 }
 template<class Send> void accepted(uint32_t now,const Mac& target,Send send){Message m=base(Kind::Selected,now);m.enrollment=durable.reservation;m.flags=durable.pending.count;if(target!=self)send(target,encode(m));}
 template<class Send> void select(uint32_t now,const Mac& selected,Send send){
  if(!candidate()||durable.deciding||conflict)return;
  if(durable.pending.position(selected)>=0){accepted(now,selected,send);feedbackUntil=now+250;return;}
  if(durable.pending.count>=8){++capacity;feedbackUntil=now+1500;feedbackError=true;Message m=base(Kind::Selected,now);m.enrollment=durable.reservation;m.flags=255;send(selected,encode(m));log(now,8);return;}
  enrollmentActivity=now;feedbackError=false;selectionBarrier=serial;Durable next=durable;next.pending.order[next.pending.count++]=selected;
  if(persist(next,now)){accepted(now,selected,send);feedbackUntil=now+250;log(now,3,next.pending.count);}
 }
 template<class Send> void begin(uint32_t now,Send send){
  Durable next=durable;next.reservation=fresh();next.candidate=self;next.retired=true;next.pending={};next.pending.id=next.reservation;next.pending.coordinator=self;next.deciding=false;
  if(!persist(next,now))return;
  offset=0;animation=completedAnimation=0;playing=false;confirmed=false;requestPending=false;invitation=durable.reservation;inviter=self;inviteSeen=now;status=Status::Enrollment;enrollmentActivity=now;feedbackError=false;button.transition();log(now,2);
  Message m=base(Kind::Invite,now);m.enrollment=invitation;previousQuery=inviteQuery;inviteQuery=m.request=++serial;send(broadcast,encode(m));
 }
 template<class Send> void finalize(uint32_t now,Send send){
  if(!candidate()||conflict||!durable.pending.valid()){feedbackUntil=now+1500;feedbackError=true;log(now,8);return;}
  Durable next=durable;next.committed=next.pending;next.deciding=true;next.retired=false;
  if(!persist(next,now))return;
  session=boot;revision=1;active=0;playing=false;confirmed=true;acknowledged.fill(false);seen.fill(0);
  status=Status::Finishing;log(now,4);commit(now,send);
 }
 template<class Send> void request(uint32_t now,Kind kind,Send send){
  if(!confirmed||durable.reservation||!session||conflict||durable.retired)return;
  if(!coordinator()&&uint32_t(now-lastAuthority)>=leaseMs)return;
  if(kind==Kind::Pass&&(!playing||active==255||durable.committed.order[active]!=self||animating(now)))return;
  if(requestPending)return;
  requestPending=true;requestKind=kind;requestId=++serial;requestRevision=revision;requestBurst=0;lastRequest=now-200;sendRequest(now,send);
 }
 template<class Send> void sendRequest(uint32_t now,Send send){
  Message m=base(requestKind,now);m.membership=durable.committed;m.session=session;m.revision=requestRevision;m.request=requestId;
  if(coordinator())handleRequest(now,m,send);else send(durable.committed.coordinator,encode(m));
  lastRequest=now;if(requestBurst){++retries;log(now,11,requestId);}if(requestBurst<5)++requestBurst;
 }
 template<class Send> void handleRequest(uint32_t now,const Message& m,Send send){
  if(!coordinator()||!session||m.session!=session||m.membership.id!=durable.committed.id||durable.reservation||durable.retired||!allSeen(now))return;
  int pos=durable.committed.position(m.sender);if(pos<0)return;
  if(m.revision!=revision){state(now,send,m.sender);return;}
  if(m.kind==Kind::Pass){if(!playing||active!=pos||animating(now))return;active=uint8_t((active+1)%durable.committed.count);animation=0;}
  else {active=uint8_t(pos);animation=++serial;animationPosition=active;animationInterval=timing.claim;animationStart=now+timing.lead;}
  ++revision;playing=true;status=Status::Playing;log(now,6,revision);requestPending=false;state(now,send);
 }
public:
 Durable durable{};Button button;Status status=Status::Discovering;
 std::array<Peer,8> peers{};uint32_t rejected=0,capacity=0,persistenceErrors=0,retries=0;
 std::array<Event,64> events{};uint32_t eventNext=0;
 explicit Node(Mac mac,uint32_t epoch,Storage* store=nullptr,Durable saved={},AnimationTiming config={}):timing(config),self(mac),boot(epoch?epoch:1),storage(store),durable(saved){}
 const Mac& identity()const{return self;}
 uint32_t currentSession()const{return session;}uint32_t currentRevision()const{return revision;}
 uint8_t activePosition()const{return active;}bool isPlaying()const{return playing;}
 bool hasPendingRequest()const{return requestPending;}
 bool animating(uint32_t now)const {
  if(!animation||!animationInterval||completedAnimation==animation)return false;
  uint32_t coordinatorNow=now+uint32_t(offset);
  return int32_t(coordinatorNow-animationStart)<int32_t(durable.committed.count*animationInterval+timing.flash);
 }
 bool forget(uint32_t now){if(!persist(Durable{},now))return false;playing=confirmed=false;session=0;active=255;invitation=0;conflict=false;status=Status::Unassigned;button.transition();return true;}
 bool number(uint16_t value,uint32_t now){auto next=durable;next.number=value;return persist(next,now);}
 template<class Send> void tick(uint32_t now,bool down,Send send){
  if(animation&&animationInterval&&reached(now+uint32_t(offset),animationStart+durable.committed.count*animationInterval+timing.flash))completedAnimation=animation;
  if(first){first=false;started=lastPulse=lastDiscovery=now;if(durable.deciding&&coordinator()){session=boot;revision=1;active=0;confirmed=true;}Message m=base(Kind::Discover,now);send(broadcast,encode(m));}
  if(conflict&&reached(now,conflictUntil)){
   conflict=false;invitation=0;
   // Withdraw only undecided candidates. Durable final decisions remain recoverable.
   if(candidate()&&!durable.deciding){Durable next=durable;next.reservation=0;next.candidate=nobody;next.pending={};persist(next,now);}
   status=durable.committed.id?Status::Paused:Status::Unassigned;button.transition();
  }
  if(invitation&&inviter!=self&&uint32_t(now-inviteSeen)>=leaseMs)invitation=0;
  if(!durable.committed.id&&!durable.reservation&&!invitation&&!conflict)status=Status::Unassigned;
  if(durable.reservation&&!conflict&&status!=Status::StorageError)status=durable.deciding?Status::Finishing:(candidate()?Status::Enrollment:Status::Selected);
  if(!conflict&&status!=Status::StorageError&&durable.committed.id&&!durable.reservation){
   if(coordinator()&&!session&&uint32_t(now-started)>=startupMs&&!durable.retired){session=boot;revision=1;active=255;confirmed=true;status=Status::WaitingClaim;}
   if(coordinator()&&durable.retired){playing=false;status=Status::Unassigned;}
   else if(coordinator()&&session){
    bool ready=allSeen(now);bool wasPlaying=playing;
    if(!ready){playing=false;status=Status::Paused;}
    else if(active!=255){playing=true;status=Status::Playing;}
    else status=Status::WaitingClaim;
    if(playing!=wasPlaying)++revision;
   }else if(confirmed&&uint32_t(now-lastAuthority)>=leaseMs){playing=false;status=Status::Paused;requestPending=false;}
  }
  // Durable decision retries until every member has adopted and responded in this epoch.
  if(coordinator()&&durable.deciding&&!durable.retired&&!conflict&&allSeen(now)){
   Durable next=durable;next.deciding=false;next.reservation=0;next.candidate=nobody;next.pending={};
   if(persist(next,now)){++revision;invitation=0;animation=++serial;animationPosition=0;animationInterval=timing.enrollment;animationStart=now+timing.lead;active=0;playing=true;status=Status::Playing;log(now,5);state(now,send);}
  }
  Mode mode=(invitation||candidate())&&!durable.deciding?Mode::Enrollment:(animating(now)?Mode::Animation:Mode::Normal);
  if(mode==Mode::Enrollment&&!durable.reservation&&invitation)status=Status::Enrollment;
  if(conflict)mode=Mode::Animation;
  Gesture g=button.tick(now,down,mode,candidate());
  if(g!=Gesture::None)log(now,1,uint32_t(g));
  threshold=g==Gesture::Threshold?true:(down?threshold:false);
  if(!conflict){
   if(g==Gesture::Enroll)begin(now,send);
   if(g==Gesture::Finalize)finalize(now,send);
   if(g==Gesture::Select){
    if(candidate())select(now,self,send);
    else if(invitation||durable.reservation){
     uint64_t target=invitation?invitation:durable.reservation;Mac c=invitation?inviter:durable.candidate;
     if(durable.reservation!=target){selectionConfirmed=false;Durable next=durable;next.reservation=target;next.candidate=c;next.pending={};next.deciding=false;if(!persist(next,now))return;}
     playing=false;confirmed=false;selectionConfirmed=false;status=Status::Selected;
     Message m=base(Kind::Select,now);m.enrollment=durable.reservation;send(durable.candidate,encode(m));
    }
   }
   if(g==Gesture::Tap)request(now,Kind::Pass,send);
   if(g==Gesture::Claim)request(now,Kind::Claim,send);
  }
  if(requestPending&&uint32_t(now-lastRequest)>=(requestBurst<5?200:heartbeatMs))sendRequest(now,send);
  if(uint32_t(now-lastPulse)>=heartbeatMs){lastPulse=now;
   if(uint32_t(now-lastDiscovery)>=2000+boot%251){lastDiscovery=now;Message discovery=base(Kind::Discover,now);send(broadcast,encode(discovery));}
   if(candidate()&&!durable.deciding&&!conflict){Message m=base(Kind::Invite,now);m.enrollment=durable.reservation;previousQuery=inviteQuery;inviteQuery=m.request=++serial;send(broadcast,encode(m));}
   else if(durable.reservation&&!selectionConfirmed&&!durable.deciding&&!candidate()&&!conflict){Message m=base(Kind::Select,now);m.enrollment=durable.reservation;send(durable.candidate,encode(m));}
   if(coordinator()&&durable.deciding&&!conflict)commit(now,send);
   if(durable.committed.id&&!durable.reservation&&!conflict){if(coordinator()&&session)state(now,send);else sync(now,send);}
  }
 }
 template<class Send> void receive(uint32_t now,const Mac& source,const uint8_t* p,size_t n,Send send){
  Message m;if(!decode(p,n,source,m)){++rejected;return;}if(source==self)return;
  if(m.membership.id&&m.membership.id==durable.committed.id&&!(m.membership==durable.committed)){++rejected;return;}
  Peer* slot=nullptr;for(auto& peer:peers)if(peer.used&&peer.mac==source){slot=&peer;break;}
  if(!slot)for(auto& peer:peers)if(!peer.used||uint32_t(now-peer.lastSeen)>5000){slot=&peer;break;}
  if(slot)*slot={source,now,m.boot,true};else ++capacity;
  if(m.kind==Kind::Discover){Message reply=base(Kind::Presence,now);reply.membership=durable.committed;reply.enrollment=durable.reservation;reply.session=session;reply.revision=revision;reply.flags=uint8_t(status);send(source,encode(reply));return;}
  // Presence locates devices, but never authorizes play or adopts membership.
  if(m.kind==Kind::Presence){
   // A fresh query response from a saved selected member can retire an
   // abandoned pending process; stale discovery hints cannot do so.
   if(candidate()&&!durable.deciding&&m.animation==boot&&m.request>selectionBarrier&&(m.request==inviteQuery||m.request==previousQuery)&&durable.pending.position(source)>=0&&m.enrollment!=durable.reservation&&m.membership.id!=durable.reservation){
    Durable next=durable;next.pending={};next.reservation=0;next.candidate=nobody;next.retired=true;
    if(persist(next,now)){invitation=0;playing=false;status=Status::Unassigned;button.transition();log(now,10);}
   }return;
  }
  if(m.kind==Kind::Invite){
   if(!m.enrollment)return;
   Message observed=base(Kind::Presence,now);observed.membership=durable.committed;observed.enrollment=durable.reservation;observed.session=session;observed.revision=revision;observed.flags=uint8_t(status);observed.request=m.request;observed.animation=m.boot;send(source,encode(observed));
   if(m.enrollment==durable.committed.id&&!durable.reservation)return;
   if(candidate()&&!durable.deciding&&m.enrollment!=durable.reservation){
    conflict=true;playing=false;status=Status::Conflict;conflictUntil=now+leaseMs;
    Message reply=base(Kind::Conflict,now);reply.enrollment=m.enrollment;reply.request=++conflictNonce;send(source,encode(reply));return;
   }
   if(durable.deciding&&coordinator())return;
   if(invitation&&invitation!=m.enrollment&&uint32_t(now-inviteSeen)<leaseMs){conflict=true;playing=false;status=Status::Conflict;conflictUntil=now+leaseMs;
    Message reply=base(Kind::Conflict,now);reply.enrollment=m.enrollment;send(source,encode(reply));reply.enrollment=invitation;send(inviter,encode(reply));return;}
   if(invitation!=m.enrollment)enrollmentActivity=now;invitation=m.enrollment;inviter=source;inviteSeen=now;
   if(!durable.reservation)status=Status::Enrollment;
   return;
  }
  if(m.kind==Kind::Conflict){if(candidate()&&!durable.deciding&&m.enrollment==durable.reservation){conflict=true;playing=false;status=Status::Conflict;conflictUntil=now+leaseMs;}return;}
  if(m.kind==Kind::Select){if(m.enrollment==durable.reservation)select(now,source,send);return;}
  if(m.kind==Kind::Selected){if(m.enrollment==durable.reservation&&source==durable.candidate&&!selectionConfirmed){feedbackUntil=now+(m.flags==255?1500:250);feedbackError=m.flags==255;selectionConfirmed=true;log(now,3,m.flags);}return;}
  if(m.kind==Kind::Commit){
   if(source!=m.membership.coordinator||m.membership.position(self)<0)return;
   if(m.membership.id!=durable.committed.id){
    if(m.membership.id!=durable.reservation||source!=durable.candidate)return;
    Durable next=durable;next.committed=m.membership;next.reservation=0;next.pending={};next.candidate=nobody;next.deciding=false;next.retired=false;
    if(!persist(next,now))return;
    confirmed=false;playing=false;animation=completedAnimation=0;animationInterval=0;offset=0;session=m.session;revision=0;active=255;invitation=0;status=Status::Finishing;button.transition();log(now,4);
   }
   if(durable.reservation||durable.retired)return;
   Message reply=base(Kind::Adopted,now);reply.membership=durable.committed;reply.session=m.session;send(source,encode(reply));return;
  }
  if(m.kind==Kind::Sync||m.kind==Kind::Adopted){
   if(coordinator()&&m.membership.id==durable.committed.id&&session){
    int pos=durable.committed.position(source);
    if(pos>=0){
     if(m.session==session){seen[size_t(pos)]=now;acknowledged[size_t(pos)]=true;}
     if(m.kind==Kind::Sync){Message reply=base(Kind::State,now);reply.membership=durable.committed;reply.session=session;reply.revision=revision;reply.active=active;reply.flags=uint8_t((playing?1:0)|(durable.deciding?2:0));reply.animation=animation;reply.start=animationStart;reply.interval=animationInterval;reply.request=m.request;send(source,encode(reply));}
    }return;
   }
   if(m.kind==Kind::Sync&&m.membership.coordinator==self&&m.membership.position(self)>=0){Message reply=base(Kind::Departed,now);reply.membership=m.membership;reply.session=m.session;send(source,encode(reply));}
   return;
  }
  if(m.kind==Kind::State){
   if(m.membership.position(self)>=0&&source==m.membership.coordinator&&(m.membership.id!=durable.committed.id||durable.reservation||durable.retired)){
    Message reply=base(Kind::Departed,now);reply.membership=m.membership;reply.session=m.session;send(source,encode(reply));return;
   }
   if(m.membership.id!=durable.committed.id||source!=durable.committed.coordinator)return;
   if(!m.session)return;
   if(session!=m.session){
    // A new coordinator boot needs direct request/response, not participant hints.
    if(m.session!=m.boot||!challenge||m.request!=challenge)return;
    challenge=0;
    session=m.session;revision=0;confirmed=false;animation=completedAnimation=0;requestPending=false;
   }
   if(m.revision<revision)return;
   if(challenge&&m.request==challenge)challenge=0;
   // The authority's boot epoch must match its session. IDs are equality tokens.
   if(m.boot!=session)return;
   offset=int32_t(m.stamp-now);lastAuthority=now;confirmed=true;
   if(m.revision>revision){revision=m.revision;requestPending=false;}
   active=m.active;playing=(m.flags&1)!=0;status=(m.flags&2)?Status::Finishing:(playing?Status::Playing:(active==255?Status::WaitingClaim:Status::Paused));
   if(m.animation!=animation){animation=m.animation;animationPosition=active;animationStart=m.start;animationInterval=m.interval;log(now,7,animation);}
   return;
  }
  if(m.kind==Kind::Pass||m.kind==Kind::Claim){handleRequest(now,m,send);return;}
  if(m.kind==Kind::Departed&&coordinator()&&m.membership.id==durable.committed.id&&m.session==session&&durable.committed.position(source)>=0){
   Durable next=durable;next.retired=true;if(persist(next,now)){playing=false;status=Status::Paused;log(now,10);}return;
  }
 }
 bool led(uint32_t now)const {
  if(status==Status::StorageError||status==Status::Conflict)return now%300<100;
  if(!reached(now,feedbackUntil)&&feedbackError)return now%300<100;
  if(threshold)return true;
  if(status==Status::Paused)return now%2000<100||(now%2000>=250&&now%2000<350);
  if(status==Status::Finishing)return now%400<200;
  if(status==Status::Selected||status==Status::Enrollment){if(!reached(now,feedbackUntil))return true;return now%(uint32_t(now-enrollmentActivity)>30000?2000:500)<100;}
  if(requestPending)return now%200<100;
  if(playing&&active!=255&&uint32_t(now-lastAuthority)<leaseMs&&!coordinator())return playLed(now);
  if(playing&&coordinator())return playLed(now);
  return now%2000<100;
 }
private:
 bool playLed(uint32_t now)const {
  if(animating(now)){
   int32_t elapsed=int32_t(now+uint32_t(offset)-animationStart);if(elapsed<0)return false;
   uint32_t circuit=durable.committed.count*animationInterval;
   if(uint32_t(elapsed)>=circuit)return true;
   return durable.committed.order[(animationPosition+uint32_t(elapsed)/animationInterval)%durable.committed.count]==self;
  }
  return durable.committed.order[active]==self;
 }
};
}
