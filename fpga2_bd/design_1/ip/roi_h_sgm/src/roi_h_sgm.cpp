#include "common.hpp"

#define SemiGlobalBM SemiGlobalBMOptimized
#define xFSGBMoptimization xFSGBMoptimizationOptimized
#include "../../../../reference/xf_sgbm_banked.hpp"
#undef SemiGlobalBM
#undef xFSGBMoptimization
typedef xf::cv::Mat<XF_8UC1,720,1280,XF_NPPC1,2> RoiGrayMat;
static void h_unpack(hls::stream<GrayBeat>& in,RoiGrayMat& mat,unsigned rows,unsigned cols,unsigned &err){
 unsigned flags=0;
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  GrayBeat b=in.read();if(bool(b.user)!=(x==0&&y==0))flags|=1;if(bool(b.last)!=(x==cols-1))flags|=2;if(b.keep!=1||b.strb!=1)flags|=4;
  mat.write(y*cols+x,b.data);
 }err=flags;
}
static void h_pack(RoiGrayMat& mat,hls::stream<DispBeat>& out,unsigned rows,unsigned cols){
 for(unsigned y=0;y<rows;y++)for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  out.write(dispbeat(unsigned(mat.read(y*cols+x))<<4,x,y,cols));
 }
}
static void h_pipeline(hls::stream<GrayBeat>& reference,hls::stream<GrayBeat>& search,hls::stream<DispBeat>& disparity,unsigned rows,unsigned cols,unsigned &reference_errors,unsigned &search_errors){
#pragma HLS DATAFLOW
 RoiGrayMat a(rows,cols),b(rows,cols),d(rows,cols);
 h_unpack(reference,a,rows,cols,reference_errors);h_unpack(search,b,rows,cols,search_errors);
 xf::cv::SemiGlobalBMOptimized<XF_BORDER_CONSTANT,5,256,32,4,XF_8UC1,XF_8UC1,720,1280,XF_NPPC1,2,2,2>(a,b,d,20,40);
 h_pack(d,disparity,rows,cols);
}

void roi_h_sgm(hls::stream<GrayBeat>& reference, hls::stream<GrayBeat>& search, hls::stream<DispBeat>& disparity, unsigned rows, unsigned cols, unsigned &reference_errors, unsigned &search_errors) {
#pragma HLS INTERFACE axis port=reference
#pragma HLS INTERFACE axis port=search
#pragma HLS INTERFACE axis port=disparity
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=reference_errors bundle=control
#pragma HLS INTERFACE s_axilite port=search_errors bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 if(rows<16||rows>H||cols<16||cols>W){reference_errors=8;search_errors=8;return;}
 h_pipeline(reference,search,disparity,rows,cols,reference_errors,search_errors);

}
