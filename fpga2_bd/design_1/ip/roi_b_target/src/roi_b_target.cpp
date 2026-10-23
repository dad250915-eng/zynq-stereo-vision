#include "common.hpp"
#define COLOR_ROWS 720
#define COLOR_COLS 1280
// Plain internal stream payloads; AXIS protocol structs are only legal on top ports.
struct color_axis {ap_uint<32> data;ap_uint<4> keep,strb;ap_uint<1> user,last,id,dest;};
struct mask_axis {ap_uint<8> data;ap_uint<1> keep,strb,user,last,id,dest;};
#include "imgproc/xf_bgr2hsv.hpp"
#include "imgproc/xf_colorthresholding.hpp"
#include "imgproc/xf_erosion.hpp"
#include "imgproc/xf_dilation.hpp"

typedef xf::cv::Mat<XF_8UC3,COLOR_ROWS,COLOR_COLS,XF_NPPC1,2> ColorMat;
typedef xf::cv::Mat<XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_NPPC1,2> MaskMat;

static void read_frame(hls::stream<color_axis>& input, ColorMat& out,
                       unsigned rows, unsigned cols, unsigned& errors) {
    unsigned err=0;
    for(unsigned y=0;y<rows;y++) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=720
        for(unsigned x=0;x<cols;x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=1280
            color_axis p=input.read();
            if(bool(p.user)!=(x==0 && y==0)) err |= 1;
            if(bool(p.last)!=(x==cols-1)) err |= 2;
            if(p.keep!=15 || p.strb!=15) err |= 4;
            out.write(y*cols+x,p.data.range(23,0));
        }
    }
    errors=err;
}

static void threshold(ColorMat& src,MaskMat& dst,unsigned low,unsigned high) {
    unsigned char lo[6],hi[6];
#pragma HLS ARRAY_PARTITION variable=lo complete
#pragma HLS ARRAY_PARTITION variable=hi complete
    unsigned char hl=low&255,hh=high&255;
    const bool wrap=hl>hh;
    lo[0]=hl;hi[0]=wrap ? 179 : hh;
    lo[3]=wrap ? 0 : hl;hi[3]=hh;
    lo[1]=lo[4]=(low>>8)&255;hi[1]=hi[4]=(high>>8)&255;
    lo[2]=lo[5]=(low>>16)&255;hi[2]=hi[5]=(high>>16)&255;
    xf::cv::colorthresholding<XF_8UC3,XF_8UC1,2,COLOR_ROWS,COLOR_COLS,XF_NPPC1,2,2>(src,dst,lo,hi);
}

static void write_frame(MaskMat& in,hls::stream<mask_axis>& output,
                        unsigned rows,unsigned cols,unsigned& pixels) {
    for(unsigned y=0;y<rows;y++) {
#pragma HLS LOOP_TRIPCOUNT min=16 max=720
        for(unsigned x=0;x<cols;x++) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=16 max=1280
            mask_axis p;
            p.data=in.read(y*cols+x);p.keep=1;p.strb=1;
            p.user=(x==0 && y==0);p.last=(x==cols-1);p.id=0;p.dest=0;
            output.write(p);
        }
    }
    pixels=rows*cols;
}

static void pipeline(hls::stream<color_axis>& input,hls::stream<mask_axis>& output,
                     unsigned rows,unsigned cols,unsigned low,unsigned high,
                     unsigned& status,unsigned& pixels) {
#pragma HLS DATAFLOW
    ColorMat bgr(rows,cols),hsv(rows,cols);
    MaskMat binary(rows,cols),e3(rows,cols),d3(rows,cols),d5(rows,cols),e5(rows,cols);
    unsigned char k3[9]={1,1,1,1,1,1,1,1,1};
    unsigned char k5[25]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
    read_frame(input,bgr,rows,cols,status);
    xf::cv::bgr2hsv<XF_8UC3,COLOR_ROWS,COLOR_COLS,XF_NPPC1,2,2>(bgr,hsv);
    threshold(hsv,binary,low,high);
    xf::cv::erode<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,3,3,1,XF_NPPC1,2,2>(binary,e3,k3);
    xf::cv::dilate<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,3,3,1,XF_NPPC1,2,2>(e3,d3,k3);
    xf::cv::dilate<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,5,5,1,XF_NPPC1,2,2>(d3,d5,k5);
    xf::cv::erode<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,5,5,1,XF_NPPC1,2,2>(d5,e5,k5);
    write_frame(e5,output,rows,cols,pixels);
}

static void b_read(const uint32_t *image,hls::stream<color_axis>& rgb,unsigned rows,unsigned cols,unsigned stride){
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  color_axis p;p.data=image[y*stride+x];p.keep=15;p.strb=15;p.user=(x==0&&y==0);p.last=(x==cols-1);p.id=0;p.dest=0;rgb.write(p);
 }
}
static void b_store(hls::stream<mask_axis>& stream,uint8_t *mask,unsigned rows,unsigned cols){
 for(unsigned i=0;i<rows*cols;i++){
#pragma HLS PIPELINE II=1
  mask[i]=stream.read().data?1:0;
 }
}

static void b_read_mat(const uint32_t *image,ColorMat &bgr,unsigned rows,unsigned cols,unsigned stride){
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  bgr.write(y*cols+x,ap_uint<24>(image[y*stride+x]));
 }
}
static void b_store_mat(MaskMat &binary,ap_uint<32> *mask,unsigned rows,unsigned cols){
 ap_uint<32> word=0;
 for(unsigned i=0;i<rows*cols;i++){
#pragma HLS PIPELINE II=1
  unsigned shift=(i&15)*2;
  word.range(shift+1,shift)=binary.read(i)?1:0;
  if((i&15)==15 || i+1==rows*cols){mask[i>>4]=word;word=0;}
 }
}
static void b_color(const uint32_t *image,ap_uint<32> *mask,unsigned rows,unsigned cols,unsigned stride,unsigned low,unsigned high,unsigned &status){
#pragma HLS DATAFLOW
 ColorMat bgr(rows,cols),hsv(rows,cols);
 MaskMat binary(rows,cols),e3(rows,cols),d3(rows,cols),d5(rows,cols),e5(rows,cols);
 unsigned char k3[9]={1,1,1,1,1,1,1,1,1};
 unsigned char k5[25]={1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};
 b_read_mat(image,bgr,rows,cols,stride);
 xf::cv::bgr2hsv<XF_8UC3,COLOR_ROWS,COLOR_COLS,XF_NPPC1,2,2>(bgr,hsv);
 threshold(hsv,binary,low,high);
 xf::cv::erode<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,3,3,1,XF_NPPC1,2,2>(binary,e3,k3);
 xf::cv::dilate<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,3,3,1,XF_NPPC1,2,2>(e3,d3,k3);
 xf::cv::dilate<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,5,5,1,XF_NPPC1,2,2>(d3,d5,k5);
 xf::cv::erode<XF_BORDER_CONSTANT,XF_8UC1,COLOR_ROWS,COLOR_COLS,XF_SHAPE_RECT,5,5,1,XF_NPPC1,2,2>(d5,e5,k5);
 b_store_mat(e5,mask,rows,cols);
 status=0;
}
// Two bits per pixel: background=0, unvisited=1, visited=2, selected=3.
// DDR queue is private scratch. Entries encode x[10:0],y[20:11], avoiding
// runtime division/modulo at every flood pixel. External mask remains 0/255.
static unsigned b_get(const ap_uint<32> *mask,unsigned p){
#pragma HLS INLINE
 return (mask[p>>4]>>((p&15)*2))&3;
}
static void b_set(ap_uint<32> *mask,unsigned p,unsigned value){
#pragma HLS INLINE
 ap_uint<32> word=mask[p>>4];unsigned shift=(p&15)*2;
 word.range(shift+1,shift)=value;mask[p>>4]=word;
}
static unsigned b_xy(unsigned x,unsigned y){
#pragma HLS INLINE
 return x|(y<<11);
}
static void b_flush(uint32_t *queue,const uint32_t *writes,unsigned base,unsigned count){
#pragma HLS INLINE off
 for(unsigned i=0;i<count;i++){
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_TRIPCOUNT min=1 max=64
  queue[base+i]=writes[i];
 }
}
static void b_load(const uint32_t *queue,uint32_t *reads,unsigned base){
#pragma HLS INLINE off
 for(unsigned i=0;i<64;i++){
#pragma HLS PIPELINE II=1
  reads[i]=queue[base+i];
 }
}
static unsigned b_flood(ap_uint<32> *mask,uint32_t *queue,unsigned cols,unsigned rows,unsigned seed_xy,unsigned from,unsigned to){
 uint32_t writes[64],reads[64];
#pragma HLS BIND_STORAGE variable=writes type=ram_2p impl=bram
#pragma HLS BIND_STORAGE variable=reads type=ram_2p impl=bram
 unsigned head=0,tail=1,write_base=0,read_base=0xffffffff;
 writes[0]=seed_xy;b_set(mask,(seed_xy>>11)*cols+(seed_xy&2047),to);
 while(head<tail){
#pragma HLS LOOP_TRIPCOUNT min=1 max=921600
  unsigned xy;
  if(head>=write_base)xy=writes[head&63];
  else{
   unsigned block=head&~63u;
   if(block!=read_base){b_load(queue,reads,block);read_base=block;}
   xy=reads[head&63];
  }
  head++;
  unsigned x=xy&2047,y=xy>>11,p=y*cols+x;
  for(unsigned dir=0;dir<4;dir++){
   bool valid;unsigned n,nxy;
   if(dir==0){valid=x!=0;n=p-1;nxy=b_xy(x-1,y);}
   else if(dir==1){valid=x+1<cols;n=p+1;nxy=b_xy(x+1,y);}
   else if(dir==2){valid=y!=0;n=p-cols;nxy=b_xy(x,y-1);}
   else{valid=y+1<rows;n=p+cols;nxy=b_xy(x,y+1);}
   if(valid && b_get(mask,n)==from){
    b_set(mask,n,to);writes[tail&63]=nxy;tail++;
    if((tail&63)==0){b_flush(queue,writes,write_base,64);write_base=tail;}
   }
  }
 }
 // Only complete flushed blocks are ever read from DDR. The live partial
 // block is forwarded from writes; thus small frontiers cannot read stale DDR.
 if(tail>write_base)b_flush(queue,writes,write_base,tail-write_base);
 return tail;
}

void roi_b_target(const uint32_t *image, uint8_t *work_mask, uint32_t *queue, uint32_t *meta, unsigned rows, unsigned cols, unsigned color_stride_words, unsigned low, unsigned high, unsigned min_area, unsigned max_area, unsigned frame) {
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=image_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=work_mask offset=slave bundle=work_mask_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=queue offset=slave bundle=queue_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=meta offset=slave bundle=meta_mem depth=16 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=image bundle=control
#pragma HLS INTERFACE s_axilite port=work_mask bundle=control
#pragma HLS INTERFACE s_axilite port=queue bundle=control
#pragma HLS INTERFACE s_axilite port=meta bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=color_stride_words bundle=control
#pragma HLS INTERFACE s_axilite port=low bundle=control
#pragma HLS INTERFACE s_axilite port=high bundle=control
#pragma HLS INTERFACE s_axilite port=min_area bundle=control
#pragma HLS INTERFACE s_axilite port=max_area bundle=control
#pragma HLS INTERFACE s_axilite port=frame bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 uint32_t out[16]={0};out[0]=0x54475431;out[1]=frame;out[12]=rows;out[13]=cols;
 bool bad=rows<16||rows>H||cols<16||cols>W||color_stride_words<cols||!min_area||min_area>max_area||(low&255)>179||(high&255)>179||((low>>8)&255)>((high>>8)&255)||((low>>16)&255)>((high>>16)&255)||(low>>24)||(high>>24);
 if(bad){out[2]=1;}else{
  static ap_uint<32> state[(N+15)/16];
#pragma HLS BIND_STORAGE variable=state type=ram_t2p impl=bram
  unsigned status=0;b_color(image,state,rows,cols,color_stride_words,low,high,status);
  unsigned best=0,seed=0;
  for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++)if(b_get(state,y*cols+x)==1){
   unsigned xy=b_xy(x,y);
   unsigned count=b_flood(state,queue,cols,rows,xy,1,2);
   if(count>=min_area&&count<=max_area&&count>best){best=count;seed=xy;}
  }
  if(best)b_flood(state,queue,cols,rows,seed,2,3);
  unsigned x0=cols,y0=rows,x1=0,y1=0;uint64_t sx=0,sy=0;
  for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
   unsigned i=y*cols+x;
   bool selected=b_get(state,i)==3;work_mask[i]=selected?255:0;
   if(selected){sx+=x;sy+=y;if(x<x0)x0=x;if(y<y0)y0=y;if(x+1>x1)x1=x+1;if(y+1>y1)y1=y+1;}
  }
  out[2]=status?2:(!best?4:0);out[3]=best;
  if(best){out[4]=x0;out[5]=y0;out[6]=x1;out[7]=y1;}
  out[8]=sx;out[9]=sx>>32;out[10]=sy;out[11]=sy>>32;
 }
 for(unsigned i=0;i<16;i++)meta[i]=out[i];

}
