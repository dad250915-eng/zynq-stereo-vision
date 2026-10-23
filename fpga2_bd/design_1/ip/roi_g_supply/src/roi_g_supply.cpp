#include "common.hpp"

void roi_g_supply(const uint8_t *left_gray, const uint8_t *right_gray, hls::stream<GrayBeat>& reference, hls::stream<GrayBeat>& search, unsigned x0, unsigned y0, unsigned width, unsigned height, unsigned stride) {
#pragma HLS INTERFACE m_axi port=left_gray offset=slave bundle=left_gray_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=right_gray offset=slave bundle=right_gray_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE axis port=reference
#pragma HLS INTERFACE axis port=search
#pragma HLS INTERFACE s_axilite port=left_gray bundle=control
#pragma HLS INTERFACE s_axilite port=right_gray bundle=control
#pragma HLS INTERFACE s_axilite port=x0 bundle=control
#pragma HLS INTERFACE s_axilite port=y0 bundle=control
#pragma HLS INTERFACE s_axilite port=width bundle=control
#pragma HLS INTERFACE s_axilite port=height bundle=control
#pragma HLS INTERFACE s_axilite port=stride bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 if(!width||width>W||!height||height>H||stride<W||x0+width>W||y0+height>H)return;
 for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++){
#pragma HLS PIPELINE II=1
  unsigned p=(y0+y)*stride+x0+x;
  // RIGHT -> reference. LEFT -> search. Never interchange these ports.
  reference.write(graybeat(right_gray[p],x,y,width));search.write(graybeat(left_gray[p],x,y,width));
 }

}
