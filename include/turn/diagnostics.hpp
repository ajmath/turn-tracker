#pragma once
#include <cstdio>
#include "node.hpp"
namespace turn {
// Output is a callable accepting a newline-terminated bounded JSON string.
// It may write to USB, a test sink, or a future read-only diagnostics transport.
template<class Output> void snapshot(const Node& node,Output output){
 char line[512];
 std::snprintf(line,sizeof(line),"{\"status\":%u,\"membership\":\"%llx\",\"enrollment\":\"%llx\",\"session\":%u,\"revision\":%u,\"active\":%u,\"finishing\":%s,\"pending_request\":%s,\"retries\":%u,\"persistence_errors\":%u,\"number\":%u,\"battery\":null}\n",unsigned(node.status),static_cast<unsigned long long>(node.durable.committed.id),static_cast<unsigned long long>(node.durable.reservation),node.currentSession(),node.currentRevision(),node.activePosition(),node.durable.deciding?"true":"false",node.hasPendingRequest()?"true":"false",node.retries,node.persistenceErrors,node.durable.number);output(line);
 auto macLine=[&](const char* key,const Mac& mac,unsigned position){std::snprintf(line,sizeof(line),"{\"%s\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"position\":%u}\n",key,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],position);output(line);};
 macLine("coordinator",node.durable.committed.coordinator,0);
 for(uint8_t i=0;i<node.durable.committed.count;++i)macLine("member",node.durable.committed.order[i],i);
 for(uint8_t i=0;i<node.durable.pending.count;++i)macLine("selected",node.durable.pending.order[i],i);
 for(auto peer:node.peers)if(peer.used){std::snprintf(line,sizeof(line),"{\"peer\":\"%02x:%02x:%02x:%02x:%02x:%02x\",\"last_seen\":%u,\"boot\":%u}\n",peer.mac[0],peer.mac[1],peer.mac[2],peer.mac[3],peer.mac[4],peer.mac[5],peer.lastSeen,peer.boot);output(line);}
}
template<class Output> void eventLog(const Node& node,Output output){
 char line[128];uint32_t count=node.eventNext<node.events.size()?node.eventNext:uint32_t(node.events.size());
 for(uint32_t i=0;i<count;++i){auto e=node.events[(node.eventNext-count+i)%node.events.size()];std::snprintf(line,sizeof(line),"{\"time\":%u,\"event\":%u,\"value\":%u}\n",e.time,e.code,e.value);output(line);}
}
}
