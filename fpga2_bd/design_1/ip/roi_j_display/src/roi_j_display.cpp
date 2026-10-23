#include "common.hpp"

// 5 columns x 6 rows, top row first; bit 4 of each row is the left pixel.
// Keep rows explicit so orientation can be reviewed without decoding hex words.
#define J_GLYPH(a,b,c,d,e,f) (((a)<<25)|((b)<<20)|((c)<<15)|((d)<<10)|((e)<<5)|(f))
static unsigned j_glyph(unsigned char c){
 switch(c){
 case '0':return J_GLYPH(0b01110,0b10001,0b10001,0b10001,0b10001,0b01110);
 case '1':return J_GLYPH(0b00100,0b01100,0b00100,0b00100,0b00100,0b01110);
 case '2':return J_GLYPH(0b01110,0b10001,0b00010,0b00100,0b01000,0b11111);
 case '3':return J_GLYPH(0b11110,0b00001,0b00110,0b00001,0b10001,0b01110);
 case '4':return J_GLYPH(0b00010,0b00110,0b01010,0b10010,0b11111,0b00010);
 case '5':return J_GLYPH(0b11111,0b10000,0b11110,0b00001,0b10001,0b01110);
 case '6':return J_GLYPH(0b01110,0b10000,0b11110,0b10001,0b10001,0b01110);
 case '7':return J_GLYPH(0b11111,0b00001,0b00010,0b00100,0b01000,0b01000);
 case '8':return J_GLYPH(0b01110,0b10001,0b01110,0b10001,0b10001,0b01110);
 case '9':return J_GLYPH(0b01110,0b10001,0b10001,0b01111,0b00001,0b01110);
 case 'X':return J_GLYPH(0b10001,0b01010,0b00100,0b00100,0b01010,0b10001);
 case 'Y':return J_GLYPH(0b10001,0b10001,0b01010,0b00100,0b00100,0b00100);
 case 'Z':return J_GLYPH(0b11111,0b00001,0b00010,0b00100,0b01000,0b11111);
 case '-':return J_GLYPH(0b00000,0b00000,0b11111,0b00000,0b00000,0b00000);
 case 'I':return J_GLYPH(0b01110,0b00100,0b00100,0b00100,0b00100,0b01110);
 case 'N':return J_GLYPH(0b10001,0b11001,0b11001,0b10101,0b10011,0b10001);
 case 'V':return J_GLYPH(0b10001,0b10001,0b10001,0b10001,0b01010,0b00100);
 case 'A':return J_GLYPH(0b01110,0b10001,0b10001,0b11111,0b10001,0b10001);
 case 'L':return J_GLYPH(0b10000,0b10000,0b10000,0b10000,0b10000,0b11111);
 case 'D':return J_GLYPH(0b11110,0b10001,0b10001,0b10001,0b10001,0b11110);
 default:return 0;
 }
}
#undef J_GLYPH
static unsigned j_number(char text[64],unsigned n,int32_t v){
 if(v<0){text[n++]='-';v=-v;}char b[10];unsigned k=0;do{b[k++]='0'+v%10;v/=10;}while(v&&k<10);while(k)text[n++]=b[--k];return n;
}

void roi_j_display(const uint32_t *base_image, const uint8_t *contour, const uint32_t *result, uint16_t *rgb565, unsigned rows, unsigned cols, unsigned color_stride_words, unsigned frame) {
#pragma HLS INTERFACE m_axi port=base_image offset=slave bundle=base_image_mem depth=2073600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=contour offset=slave bundle=contour_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=result offset=slave bundle=result_mem depth=66 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE m_axi port=rgb565 offset=slave bundle=rgb565_mem depth=921600 max_read_burst_length=64 max_write_burst_length=64
#pragma HLS INTERFACE s_axilite port=base_image bundle=control
#pragma HLS INTERFACE s_axilite port=contour bundle=control
#pragma HLS INTERFACE s_axilite port=result bundle=control
#pragma HLS INTERFACE s_axilite port=rgb565 bundle=control
#pragma HLS INTERFACE s_axilite port=rows bundle=control
#pragma HLS INTERFACE s_axilite port=cols bundle=control
#pragma HLS INTERFACE s_axilite port=color_stride_words bundle=control
#pragma HLS INTERFACE s_axilite port=frame bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

 if(!rows||rows>H||!cols||cols>W||color_stride_words<cols)return;
 char text[64];unsigned n=0;bool valid=result[0]==0x44505431&&result[3]==frame&&result[15]&&result[65]==frame;
 text[n++]='Z';text[n++]=' ';
 if(valid){uint32_t bits=result[14];float z;memcpy(&z,&bits,4);n=j_number(text,n,(int32_t)(z+0.5f));}
 else{const char invalid[7]={'I','N','V','A','L','I','D'};for(unsigned k=0;k<7;k++)text[n++]=invalid[k];}
 char xyztext[64];unsigned xn=0;
 if(valid&&result[32]==0x58595a31&&result[37]){
  const char labels[3]={'X','Y','Z'};
  for(unsigned k=0;k<3;k++){xyztext[xn++]=labels[k];xyztext[xn++]=' ';char tmp[64];unsigned z=j_number(tmp,0,(int32_t)result[48+k]/1000);for(unsigned t=0;t<z;t++)xyztext[xn++]=tmp[t];xyztext[xn++]=' ';}
 }
 // Flip only the display layers; measurement buffers and coordinates stay native.
 for(unsigned y=0;y<rows;y++){
#pragma HLS LOOP_FLATTEN off
  const unsigned source_y=rows-1-y;
  for(unsigned x=0;x<cols;x++){
#pragma HLS PIPELINE II=1
  uint32_t p=base_image[source_y*color_stride_words+x];if(contour[source_y*cols+x])p=0x00ff0000;
  bool ink=false,back=false;
  if(x>=8&&y>=8&&y<36&&x<8+n*24){unsigned tx=(x-8)/4,ty=(y-8)/4;unsigned ch=tx/6,cx=tx%6;back=true;if(cx<5&&ty<6)ink=(j_glyph(text[ch])>>(29-(ty*5+cx)))&1;}
  if(x>=8&&y>=48&&y<76&&x<8+xn*24){unsigned tx=(x-8)/4,ty=(y-48)/4;unsigned ch=tx/6,cx=tx%6;back=true;if(cx<5&&ty<6)ink=(j_glyph(xyztext[ch])>>(29-(ty*5+cx)))&1;}
  if(back)p=ink?0x00ffffff:0;
  rgb565[y*cols+x]=(((p>>19)&31)<<11)|(((p>>10)&63)<<5)|((p>>3)&31);
  }
 }

}
