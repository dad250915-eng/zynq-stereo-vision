#include "common.hpp"

// Independent sequential memory tasks keep AXI bursts independent of pixel logic.
static void d_read_color(const uint32_t *image,hls::stream<uint32_t>& pixels,
                         unsigned rows,unsigned cols,unsigned stride){
 for(unsigned y=0;y<rows;y++){
#pragma HLS LOOP_FLATTEN off
  for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
   pixels.write(image[y*stride+x]);
  }
 }
}
static void d_draw_box(hls::stream<uint32_t>& pixels,hls::stream<uint32_t>& boxed,
                       unsigned rows,unsigned cols,bool valid,
                       unsigned x0,unsigned y0,unsigned x1,unsigned y1){
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  uint32_t pixel=pixels.read();
  bool box=valid&&x>=x0&&x<x1&&y>=y0&&y<y1&&
           (x-x0<2||x1-1-x<2||y-y0<2||y1-1-y<2);
  boxed.write(box?0x00ff0000:pixel);
 }
}
static void d_write_color(hls::stream<uint32_t>& boxed,uint32_t *base_image,
                          unsigned rows,unsigned cols,unsigned stride){
 for(unsigned y=0;y<rows;y++){
#pragma HLS LOOP_FLATTEN off
  for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
   base_image[y*stride+x]=boxed.read();
  }
 }
}
static void d_read_mask(const uint8_t *selected_mask,hls::stream<uint8_t>& pixels,
                        unsigned count,bool valid){
 // Select once per frame: no conditional DDR access in the pixel loop.
 if(valid){
  for(unsigned p=0;p<count;p++){
#pragma HLS PIPELINE II=1
   pixels.write(selected_mask[p]);
  }
 }else{
  for(unsigned p=0;p<count;p++){
#pragma HLS PIPELINE II=1
   pixels.write(0);
  }
 }
}
static void d_write_mask(hls::stream<uint8_t>& pixels,uint8_t *mask,unsigned count){
 for(unsigned p=0;p<count;p++){
#pragma HLS PIPELINE II=1
  mask[p]=pixels.read();
 }
}
static void d_pipeline(const uint32_t *image,const uint8_t *selected_mask,
                       uint8_t *mask,uint32_t *base_image,unsigned rows,unsigned cols,
                       unsigned stride,bool valid,unsigned x0,unsigned y0,unsigned x1,unsigned y1){
#pragma HLS DATAFLOW
 hls::stream<uint32_t> pixels("d_pixels"),boxed("d_boxed");
 hls::stream<uint8_t> mask_pixels("d_mask_pixels");
#pragma HLS STREAM variable=pixels depth=128
#pragma HLS STREAM variable=boxed depth=128
#pragma HLS STREAM variable=mask_pixels depth=128
 d_read_color(image,pixels,rows,cols,stride);
 d_draw_box(pixels,boxed,rows,cols,valid,x0,y0,x1,y1);
 d_write_color(boxed,base_image,rows,cols,stride);
 d_read_mask(selected_mask,mask_pixels,rows*cols,valid);
 d_write_mask(mask_pixels,mask,rows*cols);
}

void roi_d_overlay(const uint32_t *image, const uint8_t *selected_mask, const uint32_t *meta, uint8_t *mask, uint32_t *base_image, unsigned rows, unsigned cols, unsigned color_stride_words) {
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=image_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=selected_mask offset=slave bundle=selected_mask_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=meta offset=slave bundle=meta_mem depth=16 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=mask offset=slave bundle=mask_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=base_image offset=slave bundle=base_image_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=image bundle=control
#pragma HLS INTERFACE s_axilite port=selected_mask bundle=control
#pragma HLS INTERFACE s_axilite port=meta bundle=control
#pragma HLS INTERFACE s_axilite port=mask bundle=control
#pragma HLS INTERFACE s_axilite port=base_image bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=color_stride_words bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 if(!rows||rows>H||!cols||cols>W||color_stride_words<cols)return;
 unsigned x0=meta[4],y0=meta[5],x1=meta[6],y1=meta[7];bool valid=meta[0]==0x54475431&&!meta[2]&&meta[3]&&x1<=cols&&y1<=rows;
 d_pipeline(image,selected_mask,mask,base_image,rows,cols,color_stride_words,
            valid,x0,y0,x1,y1);
}
