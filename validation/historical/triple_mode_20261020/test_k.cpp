#include "common.hpp"
#include <vector>
#include <stdio.h>
void roi_k_depth(hls::stream<DispBeat>&,hls::stream<DispBeat>&,uint16_t*,const uint16_t*,unsigned,unsigned,unsigned,unsigned&,const uint32_t*,unsigned,unsigned);
#define CHECK(v) do{if(!(v)){printf("FAIL line %d\n",__LINE__);return __LINE__;}}while(0)
static bool valid(unsigned q,unsigned x){unsigned d=q>>4;return !(q&15)&&d>=1&&d<=254&&d<=x;}
static unsigned expected(unsigned rgb,unsigned depth,unsigned a,bool ok){
 unsigned rr[3]={(rgb>>19)&31,(rgb>>10)&63,(rgb>>3)&31};
 unsigned dd[3]={(depth>>11)&31,(depth>>5)&63,depth&31};
 if(ok)for(unsigned c=0;c<3;c++)rr[c]=(rr[c]*(8-a)+dd[c]*a+4)/8;
 return (rr[0]<<11)|(rr[1]<<5)|rr[2];
}
extern "C" __declspec(dllexport) int run_k_tests(){
 uint16_t lut[256];for(unsigned d=0;d<256;d++)lut[d]=uint16_t(d*193+1);
 hls::stream<DispBeat> in,out;unsigned errors;
 unsigned sizes[][2]={{16,16},{33,57},{720,1280}};
 for(auto &size:sizes){
  unsigned rows=size[0],cols=size[1],n=rows*cols,stride=cols==1280?1920:cols+19;
  std::vector<uint16_t> image(n+2,0x5a5a),q(n);
  std::vector<uint32_t> rgb(rows*stride+2,0xdeadbeef);
  for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
   unsigned p=y*cols+x,d=(p*17)%256;q[p]=uint16_t((d<<4)|((p%31)==0?1:0));
   if(p%37==0)q[p]=0xffff; // invalid high Q4 must never index outside palette
   rgb[1+y*stride+x]=((x*7+y*3)&255)<<16|((x+y*11)&255)<<8|((x*5+y)&255);
  }
  for(unsigned mode: {2U,0U,1U,2U,2U,2U,0U}) {
   unsigned a=mode==2?((rows+cols)%9):1;
   // Repeat fusion for exact alpha endpoint/default checks on every dimension.
   for(unsigned alpha: {0U,1U,2U,8U}) {
    if(mode!=2&&alpha!=1)continue;
    a=alpha;
    for(unsigned p=0;p<n;p++){
     DispBeat b=dispbeat(q[p],p%cols,p/cols,cols);
     if(!mode){b.id=p&1;b.dest=(p>>1)&1;b.keep=p&3;b.strb=(p>>2)&3;}
     in.write(b);
    }
    roi_k_depth(in,out,image.data()+1,lut,mode,rows,cols,errors,mode==2?rgb.data()+1:nullptr,stride,a);
    CHECK(!errors&&in.empty());CHECK(image.front()==0x5a5a&&image.back()==0x5a5a);
    if(!mode){
     for(unsigned p=0;p<n;p++){
      DispBeat b=out.read();CHECK(b.data==q[p]&&b.id==(p&1)&&b.dest==((p>>1)&1));
      CHECK(b.keep==(p&3)&&b.strb==((p>>2)&3)&&bool(b.user)==(p==0)&&bool(b.last)==(p%cols==cols-1));
     }
    }else for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
     unsigned v=q[y*cols+x];bool ok=valid(v,x);unsigned color=ok?lut[v>>4]:0;
     unsigned want=mode==1?color:expected(rgb[1+y*stride+x],color,a,ok);
     CHECK(image[1+(rows-1-y)*cols+x]==want);
    }
    CHECK(out.empty());
   }
  }
  CHECK(rgb.front()==0xdeadbeef&&rgb.back()==0xdeadbeef);
  for(unsigned y=0;y<rows;y++)for(unsigned x=cols;x<stride;x++)CHECK(rgb[1+y*stride+x]==0xdeadbeef);
 }
 std::vector<uint16_t> im(258,0x5a5a);std::vector<uint32_t> rgb(256,0x123456);
 for(unsigned mode: {1U,2U}) {
  for(unsigned p=0;p<256;p++){DispBeat b=dispbeat(16,p%16,p/16,16);if(p==3){b.user=1;b.last=1;b.keep=0;}in.write(b);}
  roi_k_depth(in,out,im.data()+1,lut,mode,16,16,errors,rgb.data(),16,1);
  CHECK(errors==7&&in.empty()&&out.empty());
 }
 unsigned bad[][5]={{3,16,16,16,1},{2,16,16,15,1},{2,16,16,4097,1},{2,16,16,16,9},{1,0,16,16,1},{2,721,16,16,1},{2,16,1281,1920,1}};
 for(auto &c:bad){in.write(dispbeat(16,0,0,16));roi_k_depth(in,out,im.data()+1,lut,c[0],c[1],c[2],errors,rgb.data(),c[3],c[4]);CHECK(errors==8&&!in.empty()&&out.empty());in.read();}
 puts("PASS K: 1280x720, 1920-stride RGB, exact blend 0/1/2/8, invalid-depth RGB fallback, one flip, mode regression, AXIS flags, guards");return 0;
}
