#include "xyz_diagnostic.h"
#include <string.h>
#include <math.h>

/* Verified against current P1/P2 and both packed rectification maps, 2026-09-23.
 * YAML SHA256 bfc4b0ca3044e74e8e9bb1ae964b97b697f0fc5fc6532229eebcbf3c1bf235d3.
 * d = uRIGHT-uLEFT. Origin RIGHT optical centre; rectified axes x-right,y-down,z-forward. */
#define XYZ_FX 1102.6462220650792
#define XYZ_FY 1102.6462220650792
#define XYZ_CX 612.1809387207031
#define XYZ_CY 354.54823303222656
#define XYZ_NUMERATOR 44268.252199026785
#define XYZ_PRINCIPAL_DELTA 0.0
#define XYZ_GRID 25U
#define XYZ_SIDE (2U*XYZ_RADIUS+1U)
#define XYZ_PATCH (XYZ_SIDE*XYZ_SIDE)

const char *xyz_status_name(uint32_t s)
{
    static const char *const names[]={"OK","BAD_METADATA","WRONG_FRAME",
        "GLOBAL_INVALID","MASK_MISMATCH","NO_INTERIOR","LOW_SUPPORT",
        "SPREAD","BAD_GEOMETRY","PIPELINE_STOP","BUSY"};
    return s<=XYZ_BUSY?names[s]:"UNKNOWN";
}
static unsigned sample(const uint16_t *d,unsigned stride,unsigned x,unsigned y)
{
    unsigned q=d[y*(stride/2)+x],v=q>>4;
    /* Actual producer is integer disparity packed as Q4. */
    return !(q&15U)&&v>=1&&v<=254&&v<=x?v:0;
}
static unsigned quantile(const uint16_t hist[256],unsigned rank)
{
    unsigned i,total=0;
    for(i=1;i<=254;i++){total+=hist[i];if(total>=rank)return i;}
    return 0;
}
static int32_t milli(double value)
{return (int32_t)(value*1000.0+(value>=0?0.5:-0.5));}

void xyz_compute(const uint16_t *d,unsigned ds,const uint8_t *m,unsigned ms,
                 const uint32_t r[32],unsigned w,unsigned h,uint32_t frame,
                 uint32_t out[XYZ_WORDS])
{
    unsigned x,y,n=0,x0=w,y0=h,x1=0,y1=0,i,have=0,best_status=XYZ_NO_INTERIOR;
    uint64_t sx=0,sy=0;
    double cx,cy,best_distance=1e30;
    uint32_t best[10]={0};
    if(!out)return;
    memset(out,0,XYZ_WORDS*sizeof(*out));
    out[0]=XYZ_MAGIC;out[1]=1;out[3]=frame;out[4]=XYZ_BAD_METADATA;
    out[8]=XYZ_RADIUS;out[27]=1;out[28]=1000;
    out[29]=XYZ_MIN_SUPPORT;out[30]=XYZ_MAX_IQR_PX*16;
    if(!d||!m||!r||!w||!h||w>1280||h>720||ds<w*2||ds>7680||
       (ds&1)||ms<w||ms>1920)return;
    out[19]=r[2];out[20]=r[15];out[21]=r[5];
    if(r[0]!=0x44505431U||r[1]!=1||r[20]!=h||r[21]!=w||r[15]>1||
       r[6]>r[5]||r[5]>w*h||(r[2]&~63U)||((r[2]==0)!=(r[15]==1)))return;
    if(r[3]!=frame||r[4]!=frame){out[4]=XYZ_WRONG_FRAME;return;}
    if(r[2]||!r[15]){out[4]=XYZ_GLOBAL_INVALID;return;}
    for(y=0;y<h;y++)for(x=0;x<w;x++){
        unsigned b=m[y*ms+x];
        if(b!=0&&b!=255){out[4]=XYZ_MASK_MISMATCH;return;}
        if(b){
            n++;sx+=x;sy+=y;
            if(x<x0)x0=x;
            if(y<y0)y0=y;
            if(x+1>x1)x1=x+1;
            if(y+1>y1)y1=y+1;
        }
    }
    if(n!=r[5]||!n||x0!=r[16]||y0!=r[17]||x1!=r[18]||y1!=r[19]){
        out[4]=XYZ_MASK_MISMATCH;return;
    }
    cx=(double)sx/n;cy=(double)sy/n;
    /* Centroid plus a bounded 25x25 grid in the selected bounding box.
     * Scan all candidates, prefer passing patch nearest the mask centroid.
     * A grid may miss a narrow feature; never fabricate a fallback coordinate. */
    for(i=0;i<=XYZ_GRID*XYZ_GRID;i++){
        uint16_t hist[256]={0};
        unsigned xx,yy,count=0,inside=1,a,b,c,centre;
        double dist;
        if(i==0){x=(unsigned)(cx+0.5);y=(unsigned)(cy+0.5);}
        else {x=x0+((i-1)%XYZ_GRID)*(x1-x0-1)/(XYZ_GRID-1);
              y=y0+((i-1)/XYZ_GRID)*(y1-y0-1)/(XYZ_GRID-1);}
        if(x<XYZ_RADIUS||y<XYZ_RADIUS||x+XYZ_RADIUS>=w||y+XYZ_RADIUS>=h)continue;
        for(yy=y-XYZ_RADIUS;yy<=y+XYZ_RADIUS&&inside;yy++)
            for(xx=x-XYZ_RADIUS;xx<=x+XYZ_RADIUS;xx++)
                if(m[yy*ms+xx]!=255){inside=0;break;}
        if(!inside)continue;
        out[22]++;if(best_status<XYZ_LOW_SUPPORT)best_status=XYZ_LOW_SUPPORT;
        for(yy=y-XYZ_RADIUS;yy<=y+XYZ_RADIUS;yy++)
            for(xx=x-XYZ_RADIUS;xx<=x+XYZ_RADIUS;xx++){
                unsigned v=sample(d,ds,xx,yy);if(v){hist[v]++;count++;}
            }
        centre=sample(d,ds,x,y);
        if(!centre||count*1000U<XYZ_MIN_SUPPORT*XYZ_PATCH)continue;
        a=quantile(hist,(count+3)/4);b=quantile(hist,(count+1)/2);
        c=quantile(hist,(3*count+3)/4);
        best_status=XYZ_SPREAD;
        if(c-a>XYZ_MAX_IQR_PX||centre+2<b||centre>b+2)continue;
        out[23]++;
        dist=(x-cx)*(x-cx)+(y-cy)*(y-cy);
        if(!have||dist<best_distance){
            have=1;best_distance=dist;
            best[0]=x;best[1]=y;best[2]=XYZ_RADIUS;best[3]=XYZ_PATCH;
            best[4]=count;best[5]=a*16;best[6]=b*16;best[7]=c*16;
            best[8]=(c-a)*16;best[9]=count*1000/XYZ_PATCH;
        }
    }
    if(!have){out[4]=best_status;return;}
    for(i=0;i<10;i++)out[6+i]=best[i];
    {
        double den=best[6]/16.0-XYZ_PRINCIPAL_DELTA;
        double z=XYZ_NUMERATOR/den;
        double px=(best[0]-XYZ_CX)*z/XYZ_FX,py=(best[1]-XYZ_CY)*z/XYZ_FY;
        if(den<=0||!isfinite(z)||!isfinite(px)||!isfinite(py)||z>1000000||
           fabs(px)>1000000||fabs(py)>1000000){out[4]=XYZ_BAD_GEOMETRY;return;}
        out[16]=(uint32_t)milli(px);out[17]=(uint32_t)milli(py);out[18]=(uint32_t)milli(z);
        out[4]=XYZ_OK;out[5]=1;
    }
}
