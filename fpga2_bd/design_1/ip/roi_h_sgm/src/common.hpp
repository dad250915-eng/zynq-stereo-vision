#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "ap_axi_sdata.h"
#include "hls_stream.h"
typedef ap_axiu<8,1,1,1> GrayBeat;
typedef ap_axiu<16,1,1,1> DispBeat;
static const unsigned W=1280,H=720,N=W*H;
// B metadata: magic,frame,status,area,x0,y0,x1,y1,sumx lo/hi,sumy lo/hi,rows,cols.
// E descriptor: magic,frame,status,x0,y0,width,height,full_width,full_height,mask_frame.
static uint32_t fbits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
static GrayBeat graybeat(unsigned v,unsigned x,unsigned y,unsigned width){
 GrayBeat b;b.data=v;b.keep=1;b.strb=1;b.user=(x==0&&y==0);b.last=(x==width-1);b.id=0;b.dest=0;return b;
}
static DispBeat dispbeat(unsigned v,unsigned x,unsigned y,unsigned width){
 DispBeat b;b.data=v;b.keep=3;b.strb=3;b.user=(x==0&&y==0);b.last=(x==width-1);b.id=0;b.dest=0;return b;
}
