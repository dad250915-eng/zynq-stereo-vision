#include "roi_e_region_adaptive.hpp"
#ifdef __SYNTHESIS__
#include "ap_int.h"
typedef ap_uint<1> MaskBit;
#else
typedef uint8_t MaskBit;
#endif
static unsigned umin(unsigned a,unsigned b){return a<b?a:b;}
static unsigned umax(unsigned a,unsigned b){return a>b?a:b;}
static unsigned adiff(unsigned a,unsigned b){return a>b?a-b:b-a;}
static uint64_t sqdist(unsigned x,unsigned y,unsigned area,uint64_t sx,uint64_t sy){
 int64_t dx=(int64_t)x*area-(int64_t)sx,dy=(int64_t)y*area-(int64_t)sy;
 return (uint64_t)(dx*dx)+(uint64_t)(dy*dy);
}
void roi_e_region_adaptive(const uint32_t *meta,const uint8_t *reference_gray,
 const uint8_t *target_mask,uint8_t *compute_mask,uint32_t *descriptor,
 uint32_t *candidates,uint32_t *runs,unsigned rows,unsigned cols,
 unsigned stride,unsigned tier_override,unsigned run_capacity,
 unsigned texture_threshold,unsigned q25_threshold){
#pragma HLS INTERFACE m_axi port=meta offset=slave bundle=meta_mem depth=16
#pragma HLS INTERFACE m_axi port=reference_gray offset=slave bundle=reference_mem depth=921600
#pragma HLS INTERFACE m_axi port=target_mask offset=slave bundle=mask_mem depth=921600
#pragma HLS INTERFACE m_axi port=compute_mask offset=slave bundle=compute_mem depth=921600
#pragma HLS INTERFACE m_axi port=descriptor offset=slave bundle=descriptor_mem depth=32
#pragma HLS INTERFACE m_axi port=candidates offset=slave bundle=candidate_mem depth=36
#pragma HLS INTERFACE m_axi port=runs offset=slave bundle=run_mem depth=1382400
#pragma HLS INTERFACE s_axilite port=meta bundle=control
#pragma HLS INTERFACE s_axilite port=reference_gray bundle=control
#pragma HLS INTERFACE s_axilite port=target_mask bundle=control
#pragma HLS INTERFACE s_axilite port=compute_mask bundle=control
#pragma HLS INTERFACE s_axilite port=descriptor bundle=control
#pragma HLS INTERFACE s_axilite port=candidates bundle=control
#pragma HLS INTERFACE s_axilite port=runs bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=tier_override bundle=control
#pragma HLS INTERFACE s_axilite port=run_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=texture_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=q25_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
 static MaskBit mask[E_N],horizontal[E_N];
#pragma HLS BIND_STORAGE variable=mask type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=horizontal type=ram_2p impl=bram
 unsigned px[E_POOL],py[E_POOL],pid[E_POOL],tex[E_POOL],sorted[E_POOL];
 bool used[E_POOL];
 unsigned selected[9];
 uint32_t d[32]={0};
 d[0]=0x524f4931;d[1]=meta[1];d[7]=cols;d[8]=rows;d[9]=meta[1];
 d[10]=255;d[26]=0x454d0001;d[27]=stride;d[28]=texture_threshold;d[29]=q25_threshold;
 for(unsigned i=0;i<36;i++)candidates[i]=0;
 if(!rows||rows>E_H||!cols||cols>E_W||stride<cols||stride>E_W||tier_override>3||
    texture_threshold>28050||q25_threshold>28050){
  d[2]=E_INVALID;for(unsigned i=0;i<32;i++)descriptor[i]=d[i];return;
 }
 unsigned area=0,x0=cols,y0=rows,x1=0,y1=0;uint64_t sx=0,sy=0;
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  bool b=target_mask[y*cols+x]!=0;mask[y*cols+x]=b;
  if(b){area++;sx+=x;sy+=y;x0=umin(x0,x);y0=umin(y0,y);x1=umax(x1,x+1);y1=umax(y1,y+1);}
 }
 // B metadata belongs to exactly this mask. A stale descriptor is an error,
 // not a permission to select a different frame's target.
 uint64_t msx=((uint64_t)meta[9]<<32)|meta[8],msy=((uint64_t)meta[11]<<32)|meta[10];
 bool badmeta=meta[0]!=0x54475431||meta[2]||meta[3]!=area||meta[12]!=rows||meta[13]!=cols;
 if(area)badmeta=badmeta||meta[4]!=x0||meta[5]!=y0||meta[6]!=x1||meta[7]!=y1||msx!=sx||msy!=sy;
 d[17]=area;
 unsigned n=0,cells=0,median=0,q25=0,k=0,tier=E_MEDIUM,radius=0;
 if(area&&!badmeta){
  unsigned bw=x1-x0,bh=y1-y0;
  unsigned cx=(unsigned)((2*sx+area)/(2*(uint64_t)area));
  unsigned cy=(unsigned)((2*sy+area)/(2*(uint64_t)area));
  for(unsigned id=0;id<E_POOL;id++){
   unsigned x=id?x0+((id-1)%25)*(bw-1)/24:cx;
   unsigned y=id?y0+((id-1)/25)*(bh-1)/24:cy;
   bool legal=x>=5&&y>=5&&x+5<cols&&y+5<rows;
   bool duplicate=false;
   for(unsigned j=0;j<n;j++)if(px[j]==x&&py[j]==y)duplicate=true;
   if(legal&&!duplicate){
    bool full=true;unsigned sum=0;
    for(unsigned wy=0;wy<11;wy++)for(unsigned wx=0;wx<11;wx++){
     unsigned yy=y+wy-5,xx=x+wx-5;
     if(!mask[yy*cols+xx])full=false;
     if(wx<10)sum+=adiff(reference_gray[yy*stride+xx],reference_gray[yy*stride+xx+1]);
    }
    if(full){px[n]=x;py[n]=y;pid[n]=id;tex[n]=sum;used[n]=false;
     unsigned pos=n;
     for(unsigned j=0;j<n;j++){unsigned q=n-j;if(sorted[q-1]>sum){sorted[q]=sorted[q-1];pos=q-1;}else break;}
     sorted[pos]=sum;n++;
     unsigned cell=umin(2,3*(x-x0)/bw)+3*umin(2,3*(y-y0)/bh);cells|=1u<<cell;
    }
   }
  }
  if(n){
   median=sorted[(n-1)/2];q25=sorted[(n-1)/4];
   unsigned anchor=0;uint64_t bestdist=~(uint64_t)0;
   for(unsigned i=0;i<n;i++)if(tex[i]>=median){uint64_t dist=sqdist(px[i],py[i],area,sx,sy);
    if(dist<bestdist){anchor=i;bestdist=dist;}}
   selected[0]=anchor;used[anchor]=true;k=1;unsigned tmax=umax(sorted[n-1],1);
   while(k<umin(9,n)){
    unsigned best=0;uint64_t bestscore=0;bool have=false;
    for(unsigned i=0;i<n;i++)if(!used[i]){
     uint64_t separation=~(uint64_t)0;
     for(unsigned j=0;j<k;j++){unsigned s=selected[j];uint64_t v=(uint64_t)adiff(px[i],px[s])*bh+(uint64_t)adiff(py[i],py[s])*bw;
      if(v<separation)separation=v;}
     uint64_t score=separation*(tmax+3*tex[i]);
     if(!have||score>bestscore||(score==bestscore&&(tex[i]>tex[best]||(tex[i]==tex[best]&&pid[i]<pid[best])))){
      have=true;best=i;bestscore=score;}
    }
    selected[k++]=best;used[best]=true;
   }
  }
  unsigned occupied=0;for(unsigned i=0;i<9;i++)occupied+=(cells>>i)&1;
  d[20]=k;d[21]=median;d[22]=q25;d[23]=occupied;d[30]=n;
  bool admitted=k>=3&&n>=3&&occupied>=3&&q25>=q25_threshold;
  tier=tier_override?tier_override:((median>texture_threshold&&admitted)?E_SMALL:E_MEDIUM);
  d[31]=admitted;
  for(unsigned i=0;i<k;i++){unsigned s=selected[i];candidates[4*i]=px[s];candidates[4*i+1]=py[s];candidates[4*i+2]=pid[s];candidates[4*i+3]=tex[s];}
  unsigned shortside=umin(bw,bh);
  radius=tier==E_SMALL?0:(tier==E_MEDIUM?umax(8,umin(16,(shortside+15)/16)):umax(24,umin(48,(shortside+5)/6)));
 }
 if(!area||badmeta){d[2]=E_INVALID;if(badmeta)d[25]|=E_META_MISMATCH;}
 d[16]=tier;d[24]=radius;d[11]=radius;
 if(!d[2]&&tier!=E_SMALL){
  for(unsigned y=0;y<rows;y++){
   unsigned count=0;
   for(unsigned x=0;x<umin(cols,radius);x++)count+=mask[y*cols+x];
   for(unsigned x=0;x<cols;x++){
    if(x+radius<cols)count+=mask[y*cols+x+radius];
    horizontal[y*cols+x]=count!=0;
    if(x>=radius)count-=mask[y*cols+x-radius];
   }
  }
 }
 unsigned vertical[E_W]={0};
 if(!d[2]&&tier!=E_SMALL)for(unsigned y=0;y<umin(rows,radius);y++)for(unsigned x=0;x<cols;x++)vertical[x]+=horizontal[y*cols+x];
 unsigned pixels=0,nruns=0,bx0=cols,by0=rows,bx1=0,by1=0;
 for(unsigned y=0;y<rows;y++){
  bool inrun=false;unsigned begin=0;
  for(unsigned x=0;x<=cols;x++){
   bool b=false;
   if(x<cols){
    if(!d[2]){
     if(tier==E_SMALL){for(unsigned i=0;i<k;i++){unsigned s=selected[i];
      unsigned dx=adiff(x,px[s]),dy=adiff(y,py[s]);if((dx==0||dx==3)&&(dy==0||dy==3))b=true;}}
     else{if(y+radius<rows)vertical[x]+=horizontal[(y+radius)*cols+x];b=vertical[x]!=0;
      if(y>=radius)vertical[x]-=horizontal[(y-radius)*cols+x];}
    }
    compute_mask[y*cols+x]=b?1:0;
    if(b){pixels++;bx0=umin(bx0,x);by0=umin(by0,y);bx1=umax(bx1,x+1);by1=umax(by1,y+1);}
   }
   if(b&&!inrun){inrun=true;begin=x;}
   if(!b&&inrun){
    if(nruns<run_capacity){runs[3*nruns]=y;runs[3*nruns+1]=begin;runs[3*nruns+2]=x;}
    else d[25]|=E_RLE_OVERFLOW;
    nruns++;inrun=false;
   }
  }
 }
 if(pixels){d[3]=bx0;d[4]=by0;d[5]=bx1-bx0;d[6]=by1-by0;}
 d[18]=pixels;d[19]=nruns;
 for(unsigned i=0;i<32;i++)descriptor[i]=d[i];
}
