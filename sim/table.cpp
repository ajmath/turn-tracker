#include "table.hpp"
int main(int argc,char** argv){
 const std::string mode=argc>=3?argv[2]:"faults";
 uint32_t seed=argc>1?uint32_t(std::strtoull(argv[1],nullptr,10)):42;
 const std::string replay="build/table "+std::to_string(seed)+(mode=="clean"?" clean":"");
 for(size_t count=2;count<=8;++count){
  Table table(count,seed);table.faults=argc<3||std::string(argv[2])!="clean";
  try {table.scenario();std::cout<<"PASS n="<<count<<" seed="<<seed<<" drops="<<table.dropped<<'\n';}
  catch(const std::exception& e){std::ofstream out("build/failure.trace");out<<table.trace.str();std::cerr<<"FAIL n="<<count<<" seed="<<seed<<" "<<e.what()<<"; replay: "<<replay<<" trace=build/failure.trace\n";return 1;}
  if(count==3){
   std::ofstream out("build/example.trace");out<<table.trace.str();
   Table edges(3,seed);edges.faults=table.faults;
   try{edges.edgeScenarios();Table conflict(3,seed);conflict.faults=table.faults;try{conflict.conflictScenario();conflict.randomized();Table abandoned(3,seed);abandoned.faults=table.faults;try{abandoned.abandoned();}catch(...){conflict.trace<<abandoned.trace.str();throw;}}catch(...){edges.trace<<conflict.trace.str();throw;}std::cout<<"PASS persistence/replacement/escape seed="<<seed<<'\n';}
   catch(const std::exception& e){std::ofstream failure("build/failure.trace");failure<<edges.trace.str();std::cerr<<"FAIL edges seed="<<seed<<" "<<e.what()<<" replay: "<<replay<<'\n';return 1;}
  }
 }
 Table wrap(2,seed);wrap.time=0xffff0000;for(auto& puck:wrap.pucks)puck.ppm=0;
 try{wrap.scenario();std::cout<<"PASS clock wrap seed="<<seed<<'\n';}catch(const std::exception& e){std::ofstream failure("build/failure.trace");failure<<wrap.trace.str();std::cerr<<"wrap seed="<<seed<<" "<<e.what()<<"; replay: "<<replay<<" trace=build/failure.trace\n";return 1;}
 for(size_t target=0;target<3;++target)for(unsigned boundary=1;boundary<=(target==0?6u:2u);++boundary)for(bool after:{false,true}){
  Table cut(3,seed);cut.pucks[target].disk.cutAt=boundary;cut.pucks[target].disk.afterWrite=after;
  try{cut.persistenceBoundary();}catch(const std::exception& e){std::ofstream failure("build/failure.trace");failure<<cut.trace.str();std::cerr<<"persistence seed="<<seed<<" target="<<target<<" boundary="<<boundary<<" after="<<after<<" "<<e.what()<<"; replay: "<<replay<<" trace=build/failure.trace\n";return 1;}
 }
 std::cout<<"PASS persistence boundary sweep seed="<<seed<<'\n';
 Table capacity(9,seed);try{capacity.edgeScenarios();std::cout<<"PASS capacity seed="<<seed<<'\n';}catch(const std::exception& e){std::ofstream failure("build/failure.trace");failure<<capacity.trace.str();std::cerr<<"capacity seed="<<seed<<" "<<e.what()<<"; replay: "<<replay<<" trace=build/failure.trace\n";return 1;}
}
