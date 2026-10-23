#include "common.hpp"

static void c_read(const uint8_t *mask,hls::stream<uint8_t>& pixels,unsigned count){
 for(unsigned p=0;p<count;p++){
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=921600
  pixels.write(mask[p]);
 }
}
static void c_edges(hls::stream<uint8_t>& pixels,hls::stream<uint8_t>& edges,unsigned rows,unsigned cols){
 uint8_t above[W],middle[W],window[3][3];
#pragma HLS BIND_STORAGE variable=above type=ram_t2p impl=bram
#pragma HLS BIND_STORAGE variable=middle type=ram_t2p impl=bram
#pragma HLS ARRAY_PARTITION variable=window complete dim=0
 for(unsigned y=0;y<=rows;y++){
#pragma HLS LOOP_FLATTEN off
#pragma HLS LOOP_TRIPCOUNT min=2 max=721
  for(unsigned x=0;x<=cols;x++){
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=2 max=1281
#pragma HLS DEPENDENCE variable=above inter false
#pragma HLS DEPENDENCE variable=middle inter false
   uint8_t a=0,b=0,c=0;
   if(x<cols){
    if(y>=2)a=above[x];
    if(y>=1)b=middle[x];
    if(y<rows)c=pixels.read();
    above[x]=b;middle[x]=c;
   }
   for(unsigned k=0;k<3;k++){
#pragma HLS UNROLL
    if(x==0){window[k][0]=0;window[k][1]=0;}
    else{window[k][0]=window[k][1];window[k][1]=window[k][2];}
   }
   window[0][2]=a;window[1][2]=b;window[2][2]=c;
   if(y>=1 && x>=1){
    bool edge=window[1][1] && (!window[0][1] || !window[2][1] || !window[1][0] || !window[1][2]);
    edges.write(edge?255:0);
   }
  }
 }
}
static void c_write(hls::stream<uint8_t>& edges,uint8_t *contour,unsigned count){
 for(unsigned p=0;p<count;p++){
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=921600
  contour[p]=edges.read();
 }
}
static void c_pipeline(const uint8_t *mask,uint8_t *contour,unsigned rows,unsigned cols){
#pragma HLS DATAFLOW
 hls::stream<uint8_t> pixels("c_pixels"),edges("c_edges");
#pragma HLS STREAM variable=pixels depth=64
#pragma HLS STREAM variable=edges depth=64
 c_read(mask,pixels,rows*cols);
 c_edges(pixels,edges,rows,cols);
 c_write(edges,contour,rows*cols);
}
void roi_c_contour(const uint8_t *mask,uint8_t *contour,unsigned rows,unsigned cols){
#pragma HLS INTERFACE m_axi port=mask offset=slave bundle=mask_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=contour offset=slave bundle=contour_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=mask bundle=control
#pragma HLS INTERFACE s_axilite port=contour bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
 if(!rows || rows>H || !cols || cols>W)return;
 c_pipeline(mask,contour,rows,cols);
}
