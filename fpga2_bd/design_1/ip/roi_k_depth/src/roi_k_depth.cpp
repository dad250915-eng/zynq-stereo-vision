#include "common.hpp"

// Shared downstream of the SAME H: 0=measurement passthrough, 1=depth,
// 2=RIGHT_RECT RGB + depth palette. No frame-sized on-chip storage.
static bool k_valid(unsigned q,unsigned x) {
 unsigned d=q>>4;
 return !(q&15)&&d>=1&&d<=254&&d<=x;
}
static unsigned k_flags(const DispBeat &b,unsigned x,unsigned y,unsigned cols) {
 unsigned f=0;
 if(bool(b.user)!=(x==0&&y==0))f|=1;
 if(bool(b.last)!=(x+1==cols))f|=2;
 if(b.keep!=3||b.strb!=3)f|=4;
 return f;
}
static void k_read_rgb(const uint32_t *image_r,hls::stream<uint32_t>& colors,
                      unsigned rows,unsigned cols,unsigned color_stride_words,bool fusion) {
 if(!fusion)return;
 for(unsigned y=0;y<rows;y++) {
  unsigned base=y*color_stride_words;
  for(unsigned x=0;x<cols;x++) {
#pragma HLS PIPELINE II=1
   colors.write(image_r[base+x]);
  }
 }
}
static uint16_t k_blend(uint32_t rgb,uint16_t depth,unsigned alpha,bool valid) {
#pragma HLS INLINE
 ap_uint<4> a=alpha,inv=8-a;
 unsigned r=(rgb>>19)&31,g=(rgb>>10)&63,b=(rgb>>3)&31;
 if(valid) {
  r=(inv*ap_uint<5>(r)+a*ap_uint<5>((depth>>11)&31)+4U)>>3;
  g=(inv*ap_uint<6>(g)+a*ap_uint<6>((depth>>5)&63)+4U)>>3;
  b=(inv*ap_uint<5>(b)+a*ap_uint<5>(depth&31)+4U)>>3;
 }
 return uint16_t((r<<11)|(g<<5)|b);
}
static void k_mix(hls::stream<DispBeat>& disparity,hls::stream<uint32_t>& colors,
                  hls::stream<uint16_t>& pixels,const uint16_t lut[256],
                  unsigned rows,unsigned cols,unsigned blend_eighths,bool fusion,unsigned &errors) {
 unsigned flags=0;
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++) {
#pragma HLS PIPELINE II=1
  DispBeat beat=disparity.read();unsigned q=beat.data;
  flags|=k_flags(beat,x,y,cols);
  bool valid=k_valid(q,x);
  uint16_t depth=valid?lut[q>>4]:uint16_t(0);
  if(fusion) pixels.write(k_blend(colors.read(),depth,blend_eighths,valid));
  else pixels.write(depth);
 }
 errors=flags;
}
static void k_write(hls::stream<uint16_t>& pixels,uint16_t *rgb565,unsigned rows,unsigned cols) {
 for(unsigned y=0;y<rows;y++) {
  // Both RGB and disparity use original RIGHT_RECT coordinates. Flip ONCE here.
  unsigned base=(rows-1-y)*cols;
  for(unsigned x=0;x<cols;x++) {
#pragma HLS PIPELINE II=1
   rgb565[base+x]=pixels.read();
  }
 }
}
static void k_display(hls::stream<DispBeat>& disparity,uint16_t *rgb565,
                     const uint32_t *image_r,const uint16_t lut[256],
                     unsigned rows,unsigned cols,unsigned stride,unsigned alpha,bool fusion,unsigned &errors) {
#pragma HLS DATAFLOW
 hls::stream<uint32_t> colors;
 hls::stream<uint16_t> pixels;
#pragma HLS STREAM variable=colors depth=128
#pragma HLS STREAM variable=pixels depth=128
 k_read_rgb(image_r,colors,rows,cols,stride,fusion);
 k_mix(disparity,colors,pixels,lut,rows,cols,alpha,fusion,errors);
 k_write(pixels,rgb565,rows,cols);
}
void roi_k_depth(hls::stream<DispBeat>& disparity,hls::stream<DispBeat>& measurement,
                 uint16_t *rgb565,const uint16_t *palette,unsigned mode,
                 unsigned rows,unsigned cols,unsigned &errors,const uint32_t *image_r,
                 unsigned color_stride_words,unsigned blend_eighths) {
#pragma HLS INTERFACE axis port=disparity
#pragma HLS INTERFACE axis port=measurement
#pragma HLS INTERFACE m_axi port=rgb565 offset=slave bundle=video depth=921600 max_write_burst_length=64 num_write_outstanding=2
#pragma HLS INTERFACE m_axi port=palette offset=slave bundle=video depth=256 max_read_burst_length=64 num_read_outstanding=2
#pragma HLS INTERFACE m_axi port=image_r offset=slave bundle=video depth=1382400 max_read_burst_length=64 num_read_outstanding=2
#pragma HLS INTERFACE s_axilite port=rgb565 bundle=control
#pragma HLS INTERFACE s_axilite port=palette bundle=control
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=errors bundle=control
#pragma HLS INTERFACE s_axilite port=image_r bundle=control
#pragma HLS INTERFACE s_axilite port=color_stride_words bundle=control
#pragma HLS INTERFACE s_axilite port=blend_eighths bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
 errors=0;
 if(rows<16||rows>H||cols<16||cols>W||mode>2||
    (mode==2&&(color_stride_words<cols||color_stride_words>4096||blend_eighths>8))) {
  errors=8;return;
 }
 if(!mode) {
  for(unsigned p=0;p<rows*cols;p++) {
#pragma HLS PIPELINE II=1
   measurement.write(disparity.read());
  }
 } else {
  uint16_t lut[256];
#pragma HLS BIND_STORAGE variable=lut type=ram_1p impl=lutram
  for(unsigned i=0;i<256;i++) {
#pragma HLS PIPELINE II=1
   lut[i]=palette[i];
  }
  k_display(disparity,rgb565,image_r,lut,rows,cols,color_stride_words,blend_eighths,mode==2,errors);
 }
}
