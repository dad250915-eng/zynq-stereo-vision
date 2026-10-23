#include "common.hpp"

static unsigned i_quantile(const uint16_t hist[256],unsigned rank){unsigned n=0;for(unsigned d=1;d<=254;d++){n+=hist[d];if(n>=rank)return d;}return 0;}
static unsigned i_sample(uint16_t q,unsigned x){unsigned d=q>>4;return !(q&15)&&d>=1&&d<=254&&d<=x?d:0;}
static int32_t i_milli(double x){return (int32_t)(x*1000.0+(x>=0?0.5:-0.5));}

static void dense_measure(hls::stream<DispBeat>& disparity, const uint8_t *mask, const uint32_t *meta, uint32_t *result, unsigned x0, unsigned y0, unsigned width, unsigned height, unsigned frame, unsigned mask_frame, unsigned min_disparity, unsigned max_disparity, unsigned min_points, unsigned min_support, unsigned max_iqr, float numerator, float delta) {

 uint32_t r[32]={0},xyz[32]={0};
 r[0]=0x44505431;r[1]=1;r[3]=frame;r[4]=mask_frame;r[20]=H;r[21]=W;
 xyz[0]=0x58595a31;xyz[1]=1;xyz[3]=frame;xyz[4]=1;xyz[8]=5;xyz[27]=1;xyz[28]=1000;xyz[29]=800;xyz[30]=64;
 unsigned status=0,protocol=0;
 bool dimensions=width>=1&&width<=W&&height>=1&&height<=H&&meta[12]<=H&&meta[13]<=W&&x0+width<=meta[13]&&y0+height<=meta[12];
 if(!dimensions||min_disparity>max_disparity||max_disparity>255||!min_points||min_points>N||min_support>1000||max_iqr>255||!(numerator>0&&numerator<=1e9f)||!(delta>=-4096&&delta<=4096)||!((float)min_disparity-delta>=0.001f))status|=1;
 if(frame!=mask_frame||meta[1]!=frame)status|=2;
 uint32_t hist[256];
#pragma HLS BIND_STORAGE variable=hist type=ram_2p impl=bram
 uint16_t line_d[11][1280];uint8_t line_m[11][1280];
#pragma HLS BIND_STORAGE variable=line_d type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=line_m type=ram_2p impl=bram
 for(unsigned d=0;d<256;d++)hist[d]=0;
 unsigned selected=0,valid=0,bad=0,range=0,outside=0,bx0=W,by0=H,bx1=0,by1=0;
 uint64_t sx=uint64_t(meta[8])|(uint64_t(meta[9])<<32),sy=uint64_t(meta[10])|(uint64_t(meta[11])<<32);
 double cx=meta[3]?(double)sx/meta[3]:0,cy=meta[3]?(double)sy/meta[3]:0;
 unsigned cu[626],cv[626];
#pragma HLS BIND_STORAGE variable=cu type=ram_1p impl=bram
#pragma HLS BIND_STORAGE variable=cv type=ram_1p impl=bram
 for(unsigned i=0;i<626;i++){
  cu[i]=i?meta[4]+((i-1)%25)*(meta[6]-meta[4]-1)/24:(unsigned)(cx+0.5);
  cv[i]=i?meta[5]+((i-1)/25)*(meta[7]-meta[5]-1)/24:(unsigned)(cy+0.5);
 }
 bool have=false;double best_distance=1e30;unsigned best_id=626,best_status=5,best[10]={0};
 if(dimensions)for(unsigned y=0;y<height;y++){
  unsigned bank=y%11,gy=y0+y;
  for(unsigned x=0;x<width;x++){
#pragma HLS PIPELINE II=3
   DispBeat beat=disparity.read();uint64_t payload=beat.data;unsigned q=unsigned(payload&65535),d=q>>4,gx=x0+x;uint8_t m=mask[gy*meta[13]+gx];
   if(((payload>>16)&2047)!=gx||((payload>>27)&1023)!=gy||(m&&!(payload&(uint64_t(1)<<37))))protocol|=16;
   line_d[bank][x]=q;line_m[bank][x]=m;
   if(bool(beat.user)!=(x==0&&y==0))protocol|=1;if(bool(beat.last)!=(x==width-1))protocol|=2;if(beat.keep!=255||beat.strb!=255)protocol|=4;
   if(m!=0&&m!=255)protocol|=8;
   if(m){selected++;if(gx<bx0)bx0=gx;if(gy<by0)by0=gy;if(gx+1>bx1)bx1=gx+1;if(gy+1>by1)by1=gy+1;
    if((q&15)||d>255)bad++;else if(d<min_disparity||d>max_disparity)range++;else if(d>gx)outside++;else{hist[d]++;valid++;}
   }
  }
  if(y>=10)for(unsigned i=0;i<626;i++){
   unsigned u=cu[i],v=cv[i];
   if(v+5!=gy||u<5||v<5||u+5>=W||v+5>=H||u<x0+5||u+5>=x0+width||v<y0+5)continue;
   uint16_t ph[256];
#pragma HLS BIND_STORAGE variable=ph type=ram_2p impl=bram
   for(unsigned d=0;d<256;d++)ph[d]=0;
   bool inside=true;unsigned count=0;
   for(unsigned yy=0;yy<11;yy++)for(unsigned xx=0;xx<11;xx++){
    unsigned lx=u-x0-5+xx,ly=v-y0-5+yy,b=ly%11;
    if(line_m[b][lx]!=255)inside=false;
    unsigned d=i_sample(line_d[b][lx],u-5+xx);if(d){ph[d]++;count++;}
   }
   if(!inside)continue;xyz[22]++;if(best_status<6)best_status=6;
   unsigned centre=i_sample(line_d[(v-y0)%11][u-x0],u);
   if(!centre||count*1000<800*121)continue;
   unsigned a=i_quantile(ph,(count+3)/4),b=i_quantile(ph,(count+1)/2),c=i_quantile(ph,(3*count+3)/4);
   best_status=7;if(c-a>4||centre+2<b||centre>b+2)continue;xyz[23]++;
   double dist=(u-cx)*(u-cx)+(v-cy)*(v-cy);
   if(!have||dist<best_distance||(dist==best_distance&&i<best_id)){
    have=true;best_distance=dist;best_id=i;best[0]=u;best[1]=v;best[2]=5;best[3]=121;best[4]=count;best[5]=a*16;best[6]=b*16;best[7]=c*16;best[8]=(c-a)*16;best[9]=count*1000/121;
   }
  }
 }
 r[5]=selected;r[6]=valid;r[7]=bad;r[8]=range;r[9]=outside;
 if(selected){r[16]=bx0;r[17]=by0;r[18]=bx1;r[19]=by1;}else status|=4;
 if(valid<min_points)status|=8;if(selected&&uint64_t(valid)*1000<uint64_t(selected)*min_support)status|=16;
 if(valid){unsigned ranks[3]={(valid-1)/4,(valid-1)/2,3*(valid-1)/4},values[3]={0},cum=0;bool found[3]={false,false,false};
  for(unsigned d=0;d<256;d++){cum+=hist[d];for(unsigned k=0;k<3;k++)if(!found[k]&&cum>ranks[k]){values[k]=d;found[k]=true;}}
  r[10]=values[0]*16;r[11]=values[1]*16;r[12]=values[2]*16;r[13]=values[2]-values[0];if(r[13]>max_iqr)status|=32;
  if(!status){r[14]=fbits(numerator/((float)values[1]-delta));r[15]=1;}
 }
 if(protocol){status|=1;r[15]=0;r[14]=0;}r[2]=status;
 xyz[19]=status;xyz[20]=r[15];xyz[21]=selected;
 bool metadata=meta[0]==0x54475431&&!meta[2]&&selected==meta[3]&&bx0==meta[4]&&by0==meta[5]&&bx1==meta[6]&&by1==meta[7];
 // The legacy XYZ routine returns before testing patches for invalid global
 // depth, stale frames, or a mask mismatch. Preserve those diagnostic counts.
 if(status||!r[15]||!metadata){xyz[22]=0;xyz[23]=0;}
 if(frame!=mask_frame)xyz[4]=2;
 else if(status||!r[15])xyz[4]=3;
 else if(!metadata)xyz[4]=4;
 else if(!have)xyz[4]=best_status;
 else{
  for(unsigned i=0;i<10;i++)xyz[6+i]=best[i];
  // Keep calibrated RIGHT_RECT local-sample geometry and original double constants.
  double den=best[6]/16.0,z=44268.252199026785/den;
  double x=(best[0]-612.1809387207031)*z/1102.6462220650792,y=(best[1]-354.54823303222656)*z/1102.6462220650792;
  if(den<=0||!isfinite(z)||!isfinite(x)||!isfinite(y)||z>1000000||fabs(x)>1000000||fabs(y)>1000000)xyz[4]=8;
  else{xyz[4]=0;xyz[5]=1;xyz[16]=(uint32_t)i_milli(x);xyz[17]=(uint32_t)i_milli(y);xyz[18]=(uint32_t)i_milli(z);}
 }
 for(unsigned i=0;i<32;i++){result[i]=r[i];result[32+i]=xyz[i];}result[64]=protocol;result[65]=frame;

}
