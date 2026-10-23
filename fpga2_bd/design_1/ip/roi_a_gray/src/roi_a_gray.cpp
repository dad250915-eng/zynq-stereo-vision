#include "common.hpp"

void roi_a_gray(const uint32_t *left, const uint32_t *right, uint8_t *left_gray, uint8_t *right_gray, unsigned rows, unsigned cols, unsigned color_stride_words) {
#pragma HLS INTERFACE m_axi port=left offset=slave bundle=left_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=right offset=slave bundle=right_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=left_gray offset=slave bundle=left_gray_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=right_gray offset=slave bundle=right_gray_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=left bundle=control
#pragma HLS INTERFACE s_axilite port=right bundle=control
#pragma HLS INTERFACE s_axilite port=left_gray bundle=control
#pragma HLS INTERFACE s_axilite port=right_gray bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=color_stride_words bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 if(!rows||rows>H||!cols||cols>W||color_stride_words<cols)return;
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  uint32_t l=left[y*color_stride_words+x],r=right[y*color_stride_words+x];
  left_gray[y*cols+x]=(((l>>16)&255)+2*((l>>8)&255)+(l&255))>>2;
  right_gray[y*cols+x]=(((r>>16)&255)+2*((r>>8)&255)+(r&255))>>2;
 }

}
