#include "common.hpp"

void roi_e_region(const uint32_t *meta, uint32_t *descriptor, unsigned max_disparity, unsigned context) {
#pragma HLS INTERFACE m_axi port=meta offset=slave bundle=meta_mem depth=16 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=descriptor offset=slave bundle=descriptor_mem depth=16 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=meta bundle=control
#pragma HLS INTERFACE s_axilite port=descriptor bundle=control
#pragma HLS INTERFACE s_axilite port=max_disparity bundle=control
#pragma HLS INTERFACE s_axilite port=context bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 uint32_t d[16]={0};d[0]=0x524f4931;d[1]=meta[1];d[7]=meta[13];d[8]=meta[12];d[9]=meta[1];
 unsigned cols=meta[13],rows=meta[12],x0=meta[4],y0=meta[5],x1=meta[6],y1=meta[7];
 if(meta[0]!=0x54475431||meta[2]||!meta[3]||!cols||cols>W||!rows||rows>H||x0>=x1||y0>=y1||x1>cols||y1>rows||max_disparity>255||context>1280)d[2]=1;
 else{
  unsigned halo=context+2,left=halo+max_disparity;
  unsigned a=x0>left?x0-left:0,b=y0>halo?y0-halo:0;
  unsigned c=x1+halo<cols?x1+halo:cols,e=y1+halo<rows?y1+halo:rows;
  // Pad tiny rectangles to the supported minimum; both cameras share this origin.
  if(c-a<16){if(a+16<=cols)c=a+16;else a=c>16?c-16:0;}
  if(e-b<16){if(b+16<=rows)e=b+16;else b=e>16?e-16:0;}
  d[3]=a;d[4]=b;d[5]=c-a;d[6]=e-b;d[10]=max_disparity;d[11]=context;
 }
 for(unsigned i=0;i<16;i++)descriptor[i]=d[i];

}
