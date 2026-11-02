#pragma once
#include "wire.hpp"
namespace turn {
struct Durable {
 Membership committed{},pending{};uint64_t reservation=0;Mac candidate{};
 bool deciding=false,retired=false;uint16_t number=0;
};
constexpr size_t storageSize=2*wireSize+12;
using StorageBytes=std::array<uint8_t,storageSize>;
inline uint32_t checksum(const uint8_t* p,size_t n){uint32_t h=2166136261u;for(size_t i=0;i<n;++i){h^=p[i];h*=16777619u;}return h;}
inline StorageBytes saveBytes(const Durable& d){
 StorageBytes b{};Message m;m.sender={2,0,0,0,0,1};m.membership=d.committed;m.enrollment=d.reservation;
 auto p=encode(m);for(size_t i=0;i<wireSize;++i)b[i]=p[i];
 m.membership=d.pending;m.sender=d.candidate==nobody?Mac{2,0,0,0,0,1}:d.candidate;p=encode(m);
 for(size_t i=0;i<wireSize;++i)b[wireSize+i]=p[i];
 auto tail=b.data()+2*wireSize;tail[0]=1;tail[1]=d.deciding;tail[2]=d.retired;tail[3]=uint8_t(d.number);tail[4]=uint8_t(d.number>>8);
 put32(b.data()+storageSize-4,checksum(b.data(),storageSize-4));return b;
}
// Pending enrollment may have 0..8 selections, unlike committed membership.
inline bool loadBytes(const StorageBytes& b,Durable& d){
 if(get32(b.data()+storageSize-4)!=checksum(b.data(),storageSize-4)||b[2*wireSize]!=1)return false;
 auto read=[&](size_t offset,Membership& member){
  member.id=get64(b.data()+offset+26);member.count=b[offset+40];if(member.count>8)return false;
  for(size_t i=0;i<6;++i)member.coordinator[i]=b[offset+34+i];
  for(size_t j=0;j<8;++j)for(size_t i=0;i<6;++i)member.order[j][i]=b[offset+41+j*6+i];return true;
 };
 Durable value;if(!read(0,value.committed)||!read(wireSize,value.pending))return false;
 if(value.committed.id&&!value.committed.valid())return false;
 value.reservation=get64(b.data()+18);for(size_t i=0;i<6;++i)value.candidate[i]=b[wireSize+4+i];
 if(!value.reservation)value.candidate=nobody;
 value.deciding=b[2*wireSize+1];value.retired=b[2*wireSize+2];value.number=b[2*wireSize+3]|uint16_t(b[2*wireSize+4])<<8;d=value;return true;
}
struct Storage {virtual ~Storage()=default;virtual bool write(const Durable&)=0;};
}
