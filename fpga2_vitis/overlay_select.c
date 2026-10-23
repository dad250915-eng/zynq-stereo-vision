#include "overlay_select.h"
static uint32_t flood(uint8_t *m,unsigned w,unsigned h,uint32_t seed,
                      uint8_t from,uint8_t to,uint32_t *q)
{
    uint32_t head=0,tail=1;q[0]=seed;m[seed]=to;
    while(head<tail) {
        uint32_t p=q[head++],x=p%w,y=p/w,n[4],k=0,i;
        if(x)n[k++]=p-1;
        if(x+1<w)n[k++]=p+1;
        if(y)n[k++]=p-w;
        if(y+1<h)n[k++]=p+w;
        for(i=0;i<k;i++)if(m[n[i]]==from){m[n[i]]=to;q[tail++]=n[i];}
    }
    return tail;
}
uint32_t overlay_select(uint8_t *m,unsigned w,unsigned h,uint32_t lo,uint32_t hi,uint32_t *q)
{
    uint32_t i,n=w*h,best=0,seed=0;
    if(!m||!q||!w||!h||w>1280||h>720||!lo||lo>hi)return 0;
    for(i=0;i<n;i++)m[i]=m[i]?1:0;
    for(i=0;i<n;i++)if(m[i]==1) {
        uint32_t count=flood(m,w,h,i,1,2,q);
        if(count>=lo&&count<=hi&&count>best){best=count;seed=i;}
    }
    if(best)flood(m,w,h,seed,2,255,q);
    for(i=0;i<n;i++)if(m[i]!=255)m[i]=0;
    return best;
}
