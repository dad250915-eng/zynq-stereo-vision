#include "../src/roi_e_region_adaptive.hpp"
#include <fstream>
#include <vector>
#include <iostream>
template<class T> void read(std::ifstream &f,T *v,size_t n){f.read(reinterpret_cast<char*>(v),n*sizeof(T));if(!f)throw "short vector";}
int main(){
 std::ifstream f("golden.bin",std::ios::binary);if(!f){std::cerr<<"golden.bin missing\n";return 1;}
 uint32_t count;read(f,&count,1);unsigned failed=0;
 for(unsigned c=0;c<count;c++){
  uint32_t header[5],meta[16],expected[32],ec[36],d[32],cand[36];
  read(f,header,5);unsigned rows=header[0],cols=header[1],stride=header[2],tier=header[3],capacity=header[4];
  std::vector<uint8_t> gray(rows*stride),mask(rows*cols),out(rows*cols),gold(rows*cols);
  std::vector<uint32_t> runs(3*capacity);
  read(f,meta,16);read(f,&gray[0],gray.size());read(f,&mask[0],mask.size());read(f,&gold[0],gold.size());read(f,expected,32);read(f,ec,36);
  roi_e_region_adaptive(meta,&gray[0],&mask[0],&out[0],d,cand,&runs[0],rows,cols,stride,tier,capacity,461,220);
  for(unsigned i=0;i<out.size();i++)if(out[i]!=gold[i]){failed++;break;}
  for(unsigned i=0;i<32;i++)if(d[i]!=expected[i]){std::cerr<<"descriptor case "<<c<<" word "<<i<<" "<<d[i]<<" != "<<expected[i]<<"\n";failed++;}
  for(unsigned i=0;i<36;i++)if(cand[i]!=ec[i])failed++;
  std::vector<uint8_t> recovered(rows*cols,0);
  for(unsigned i=0;i<d[19];i++)for(unsigned x=runs[3*i+1];x<runs[3*i+2];x++)recovered[runs[3*i]*cols+x]=1;
  if(recovered!=gold)failed++;
 }
 std::cout<<"golden cases="<<count<<" failures="<<failed<<"\n";return failed?1:0;
}
