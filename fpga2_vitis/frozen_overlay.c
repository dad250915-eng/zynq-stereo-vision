#include "frozen_overlay.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

static const uint8_t digits[10][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}};
static uint8_t glyph(char c,unsigned r){
 static const char letters[]="ZMINVALD";
 static const uint8_t rows[][7]={
 {31,1,2,4,8,16,31},{17,27,21,21,17,17,17},
 {14,4,4,4,4,4,14},{17,25,25,21,19,19,17},
 {17,17,17,17,17,10,4},{14,17,17,31,17,17,17},
 {16,16,16,16,16,16,31},{30,17,17,17,17,17,30}};
 unsigned i;
 if(c>='0' && c<='9')return digits[c-'0'][r];
 if(c==':')return (r==2 || r==5)?4:0;
 if(c=='.')return r==6?4:0;
 for(i=0;i<sizeof(letters)-1;i++)if(c==letters[i])return rows[i][r];
 return 0;
}
static void pixel(uint8_t *dst,unsigned stride,unsigned x,unsigned y,uint32_t rgb){
 memcpy(dst+y*stride+4*(320+x),&rgb,4);
}
static void text(uint8_t *dst,unsigned stride,unsigned w,unsigned h,
 unsigned x,unsigned y,const char *s){
 unsigned i,xx,yy;const unsigned scale=3;
 for(i=0;s[i];i++)for(yy=0;yy<9*scale;yy++)for(xx=0;xx<6*scale;xx++){
  unsigned px=x+i*6*scale+xx,py=y+yy;
  unsigned on=(yy>=scale && yy<8*scale && xx<5*scale) &&
   (glyph(s[i],yy/scale-1)&(1U<<(4-xx/scale)));
  if(px<w && py<h)pixel(dst,stride,px,py,on?0x00ffffffU:0);
 }
}
static int overlaps(const void*a,size_t an,const void*b,size_t bn){
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return x<=y?y-x<an:x-y<bn;
}
int fo_render(const uint8_t *right,unsigned rs,const uint8_t *mask,unsigned ms,
 const uint32_t r[32],unsigned w,unsigned h,uint32_t frame,
 uint8_t *out,unsigned os){
 unsigned x,y,count=0,x0=w,y0=h,x1=0,y1=0;
 float z;char label[48];
 if(!right||!mask||!r||!out||!w||w>1280||!h||h>720||rs<w*4||rs>7680||
    ms<w||ms>1920||os<(w+640)*4||os>7680)return FO_BAD_ARGUMENT;
 if(overlaps(out,(size_t)os*h,right,(size_t)rs*h)||
    overlaps(out,(size_t)os*h,mask,(size_t)ms*h)||overlaps(out,(size_t)os*h,r,128))return FO_BAD_ARGUMENT;
 if(r[0]!=0x44505431U||r[1]!=1||r[20]!=h||r[21]!=w||r[15]>1||
    r[6]>r[5]||r[5]>w*h||(r[2]&~63U)|| (r[2]&3U))return FO_BAD_RESULT;
 if(r[3]!=frame||r[4]!=frame)return FO_WRONG_FRAME;
 if((r[2]==0)!=(r[15]==1))return FO_BAD_RESULT;
 memcpy(&z,&r[14],4);
 if(r[15] && (!isfinite(z)||z<=0||z>1000000.0f||r[6]==0))return FO_BAD_RESULT;
 for(y=0;y<h;y++)for(x=0;x<w;x++)if(mask[y*ms+x]){
  count++;if(x<x0)x0=x;if(y<y0)y0=y;if(x+1>x1)x1=x+1;if(y+1>y1)y1=y+1;
 }
 if(count!=r[5])return FO_MASK_MISMATCH;
 if(count && (r[16]!=x0||r[17]!=y0||r[18]!=x1||r[19]!=y1))return FO_MASK_MISMATCH;
 /* Validate everything before changing output. Preserve raw images/masks. */
 memset(out,0,(size_t)os*h);
 for(y=0;y<h;y++)memcpy(out+y*os+320*4,right+y*rs,w*4);
 for(y=0;y<h;y++)for(x=0;x<w;x++)if(mask[y*ms+x] &&
    (x==0||y==0||x+1==w||y+1==h||!mask[y*ms+x-1]||!mask[y*ms+x+1]||
     !mask[(y-1)*ms+x]||!mask[(y+1)*ms+x]))pixel(out,os,x,y,0x00ff0000U);
 if(count)for(y=y0;y<y1;y++)for(x=x0;x<x1;x++)
  if(x-x0<2||x1-x<=2||y-y0<2||y1-y<=2)pixel(out,os,x,y,0x00ff0000U);
 if(r[15]){
  unsigned tenths=(unsigned)(z*10.0f+0.5f);
  snprintf(label,sizeof(label),"Z: %u.%u MM",tenths/10,tenths%10);
 }else snprintf(label,sizeof(label),"Z: INVALID");
 /* Top-left panel always readable; repeat near target when it fits. */
 text(out,os,w,h,8,8,label);
 if(count && y0>=40) {
  unsigned tw=(unsigned)strlen(label)*18;
  unsigned tx=x0;
  if(tw<w && tx>w-tw)tx=w-tw;
  text(out,os,w,h,tx,y0-30,label);
 }
 return FO_OK;
}
