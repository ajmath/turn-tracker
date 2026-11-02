#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace turn {
using Mac=std::array<uint8_t,6>;
constexpr Mac broadcast={255,255,255,255,255,255};
constexpr Mac nobody={};
enum class Kind:uint8_t { Discover=1, Presence, Invite, Select, Selected, Commit, Adopted, State, Sync, Pass, Claim, Conflict, Departed };
struct Membership {
 uint64_t id=0;Mac coordinator{};uint8_t count=0;std::array<Mac,8> order{};
 int position(const Mac& mac)const {for(uint8_t i=0;i<count;++i)if(order[i]==mac)return i;return -1;}
 bool operator==(const Membership& other)const{return id==other.id&&coordinator==other.coordinator&&count==other.count&&order==other.order;}
 bool valid()const {
  if(!id||count<2||count>8||position(coordinator)<0)return false;
  for(uint8_t i=0;i<count;++i){if(order[i]==nobody||(order[i][0]&1))return false;for(uint8_t j=0;j<i;++j)if(order[i]==order[j])return false;}return true;
 }
};
struct Message {
 Kind kind=Kind::Discover;Mac sender{};uint32_t boot=0,sequence=0;
 uint64_t enrollment=0;Membership membership{};
 uint32_t session=0,revision=0,request=0,stamp=0;
 uint32_t animation=0,start=0;uint16_t interval=0;uint8_t active=255,flags=0;
};
// Fixed length, explicit little endian integers. Never transmit C++ struct layouts.
constexpr size_t wireSize=117;
using Packet=std::array<uint8_t,wireSize>;
inline void put32(uint8_t* p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
inline uint32_t get32(const uint8_t* p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(8*i);return v;}
inline void put64(uint8_t* p,uint64_t v){put32(p,uint32_t(v));put32(p+4,uint32_t(v>>32));}
inline uint64_t get64(const uint8_t* p){return get32(p)|(uint64_t(get32(p+4))<<32);}
inline Packet encode(const Message& m){
 Packet p{};p[0]='T';p[1]='T';p[2]=1;p[3]=uint8_t(m.kind);
 for(size_t i=0;i<6;++i){p[4+i]=m.sender[i];p[34+i]=m.membership.coordinator[i];}
 put32(p.data()+10,m.boot);put32(p.data()+14,m.sequence);put64(p.data()+18,m.enrollment);put64(p.data()+26,m.membership.id);
 p[40]=m.membership.count;for(size_t j=0;j<8;++j)for(size_t i=0;i<6;++i)p[41+j*6+i]=m.membership.order[j][i];
 put32(p.data()+89,m.session);put32(p.data()+93,m.revision);put32(p.data()+97,m.request);put32(p.data()+101,m.stamp);
 put32(p.data()+105,m.animation);put32(p.data()+109,m.start);p[113]=uint8_t(m.interval);p[114]=uint8_t(m.interval>>8);p[115]=m.active;p[116]=m.flags;return p;
}
inline bool decode(const uint8_t* p,size_t n,const Mac& transport,Message& out){
 if(n!=wireSize||p[0]!='T'||p[1]!='T'||p[2]!=1||p[3]<1||p[3]>uint8_t(Kind::Departed))return false;
 Message m;m.kind=Kind(p[3]);for(size_t i=0;i<6;++i){m.sender[i]=p[4+i];m.membership.coordinator[i]=p[34+i];}
 if(m.sender!=transport||m.sender==nobody||(m.sender[0]&1))return false;
 m.boot=get32(p+10);m.sequence=get32(p+14);m.enrollment=get64(p+18);m.membership.id=get64(p+26);m.membership.count=p[40];
 if(m.membership.count>8)return false;
 for(size_t j=0;j<8;++j)for(size_t i=0;i<6;++i)m.membership.order[j][i]=p[41+j*6+i];
 m.session=get32(p+89);m.revision=get32(p+93);m.request=get32(p+97);m.stamp=get32(p+101);m.animation=get32(p+105);m.start=get32(p+109);
 m.interval=uint16_t(p[113])|(uint16_t(p[114])<<8);m.active=p[115];m.flags=p[116];
 if(m.membership.id&&!m.membership.valid())return false;
 if(m.active!=255&&m.active>=m.membership.count)return false;
 if(m.interval&&(m.interval<100||m.interval>2000))return false;
 if(m.kind==Kind::State&&(!m.membership.valid()||!m.session||(m.flags&~3u)||m.flags==3||((m.flags&1)&&m.active==255)))return false;
 if((m.kind==Kind::Pass||m.kind==Kind::Claim)&&(!m.membership.valid()||!m.session||!m.request))return false;
 if(m.kind==Kind::Commit&&(!m.membership.valid()||!m.session||m.enrollment!=m.membership.id))return false;
 if((m.kind==Kind::Invite||m.kind==Kind::Select||m.kind==Kind::Selected)&&!m.enrollment)return false;
 if(m.kind==Kind::Selected&&m.flags>8&&m.flags!=255)return false;
 out=m;return true;
}
}
