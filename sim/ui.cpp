#include <algorithm>
#include <iomanip>
#include "table.hpp"
struct InputAction {uint32_t at;size_t puck;bool down;};
static void jsonString(const std::string& text){
 std::cout<<'"';for(unsigned char c:text){switch(c){case '"':std::cout<<"\\\"";break;case '\\':std::cout<<"\\\\";break;case '\n':std::cout<<"\\n";break;case '\r':std::cout<<"\\r";break;case '\t':std::cout<<"\\t";break;default:if(c<32)std::cout<<'?';else std::cout<<c;}}std::cout<<'"';
}
int main(){
 auto table=std::make_unique<Table>(3,42);std::vector<InputAction> actions;
 std::string line;
 while(std::getline(std::cin,line)){
  std::istringstream input(line);std::string command,error;input>>command;
  try{
   auto index=[&](){size_t i;if(!(input>>i)||i>=table->pucks.size())throw std::runtime_error("Invalid puck");return i;};
   auto cancel=[&](size_t i){actions.erase(std::remove_if(actions.begin(),actions.end(),[&](const auto& a){return a.puck==i;}),actions.end());};
   if(command=="reset"){
    unsigned count;uint64_t seed;if(!(input>>count>>seed)||count<2||count>8||seed>UINT32_MAX)throw std::runtime_error("Use 2–8 pucks and a 32-bit seed");
    table=std::make_unique<Table>(count,uint32_t(seed));actions.clear();
   }else if(command=="advance"){
    unsigned duration;if(!(input>>duration)||duration>10000||duration%10)throw std::runtime_error("Advance must be 0–10000 ms in 10 ms ticks");
    for(unsigned elapsed=0;elapsed<duration;elapsed+=10){
     for(auto it=actions.begin();it!=actions.end();)if(reached(table->time,it->at)){if(table->pucks[it->puck].powered)table->pucks[it->puck].down=it->down;it=actions.erase(it);}else ++it;
     table->run(10);
    }
   }else if(command=="button"){
    auto i=index();unsigned down;if(!(input>>down)||down>1)throw std::runtime_error("Invalid button state");
    cancel(i);if(table->pucks[i].powered)table->pucks[i].down=down;
    table->trace<<"button "<<table->time<<" puck="<<i<<" down="<<down<<'\n';
   }else if(command=="gesture"){
    auto i=index();std::string gesture;input>>gesture;
    if(!table->pucks[i].powered)throw std::runtime_error("Power on the puck first");
    if(table->pucks[i].down||std::any_of(actions.begin(),actions.end(),[&](const auto& a){return a.puck==i;}))throw std::runtime_error("Puck already has an input in progress");
    std::vector<std::pair<uint32_t,bool>> sequence;
    if(gesture=="tap")sequence={{0,true},{100,false}};
    else if(gesture=="hold")sequence={{0,true},{3100,false}};
    else if(gesture=="enroll")sequence={{0,true},{3100,false},{3300,true},{3400,false},{3500,true},{3600,false}};
    else throw std::runtime_error("Unknown gesture");
    for(auto entry:sequence)actions.push_back({table->time+entry.first,i,entry.second});
    table->trace<<"gesture "<<table->time<<" puck="<<i<<" "<<gesture<<'\n';
   }else if(command=="power"){
    auto i=index();unsigned on;if(!(input>>on)||on>1)throw std::runtime_error("Invalid power state");
    cancel(i);auto& p=table->pucks[i];if(on&&!p.powered)p.restart();p.powered=on;p.down=false;table->trace<<"power "<<table->time<<" puck="<<i<<" on="<<on<<'\n';
   }else if(command=="restart"){
    auto i=index();cancel(i);table->reboot(i);table->trace<<"restart "<<table->time<<" puck="<<i<<'\n';
   }else if(command=="storage"){
    auto i=index();unsigned failing;if(!(input>>failing)||failing>1)throw std::runtime_error("Invalid storage fault");table->pucks[i].disk.failing=failing;
   }else if(command=="isolate"){
    auto i=index();unsigned blocked;if(!(input>>blocked)||blocked>1)throw std::runtime_error("Invalid isolation state");
    for(size_t j=0;j<table->pucks.size();++j)if(i!=j)table->blocked[i][j]=table->blocked[j][i]=blocked;
   }else if(command=="link"){
    auto from=index(),to=index();unsigned blocked;if(!(input>>blocked)||blocked>1||from==to)throw std::runtime_error("Choose two different pucks");table->blocked[from][to]=blocked;
   }else if(command=="faults"){
    unsigned on;if(!(input>>on)||on>1)throw std::runtime_error("Invalid radio fault setting");table->faults=on;
   }else if(command!="state")throw std::runtime_error("Unknown command");
  }catch(const std::exception& e){error=e.what();}
  std::cout<<"{\"time\":"<<table->time<<",\"seed\":"<<table->seed<<",\"faults\":"<<(table->faults?"true":"false")<<",\"queued\":"<<table->queue.size()<<",\"dropped\":"<<table->dropped<<",\"error\":";jsonString(error);
  std::cout<<",\"pucks\":[";
  for(size_t i=0;i<table->pucks.size();++i){if(i)std::cout<<',';auto& p=table->pucks[i];auto& n=*p.core;
   std::cout<<"{\"id\":"<<i<<",\"powered\":"<<(p.powered?"true":"false")<<",\"down\":"<<(p.down?"true":"false")<<",\"led\":"<<(p.powered&&n.led(p.clock(table->time))?"true":"false")<<",\"status\":"<<unsigned(n.status)<<",\"membership\":\""<<std::hex<<n.durable.committed.id<<std::dec<<"\",\"enrollment\":\""<<std::hex<<n.durable.reservation<<std::dec<<"\",\"session\":"<<n.currentSession()<<",\"revision\":"<<n.currentRevision()<<",\"active\":"<<unsigned(n.activePosition())<<",\"writes\":"<<p.disk.writes<<",\"storageFault\":"<<(p.disk.failing?"true":"false")<<",\"retries\":"<<n.retries<<",\"offset\":"<<p.offset<<",\"drift\":"<<p.ppm<<",\"coordinator\":";
   int coordinator=-1;for(size_t j=0;j<table->pucks.size();++j)if(table->pucks[j].mac==n.durable.committed.coordinator)coordinator=int(j);std::cout<<coordinator;
   auto order=[&](const char* name,const Membership& member){std::cout<<",\""<<name<<"\":[";for(uint8_t k=0;k<member.count;++k){if(k)std::cout<<',';int device=-1;for(size_t j=0;j<table->pucks.size();++j)if(table->pucks[j].mac==member.order[k])device=int(j);std::cout<<device;}std::cout<<']';};
   order("order",n.durable.committed);order("selected",n.durable.pending);std::cout<<'}';
  }
  std::cout<<"],\"links\":[";for(size_t i=0;i<table->pucks.size();++i){if(i)std::cout<<',';std::cout<<'[';for(size_t j=0;j<table->pucks.size();++j){if(j)std::cout<<',';std::cout<<(table->blocked[i][j]?"true":"false");}std::cout<<']';}
  std::cout<<"],\"trace\":";jsonString(table->trace.str());std::cout<<"}\n"<<std::flush;
  table->trace.str("");table->trace.clear();
 }
}
