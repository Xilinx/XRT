#include <string.h>

#define MODE_READ       1
#define MODE_WRITE      2
#define MODE_NOSTRIDE   4

#define BURSTBUFFERSIZE 16384

// burst length size in 32-bit words
void globalbandwidth(int *a, int *b, unsigned int bursts, unsigned int burstlength, unsigned int mode)
{
#pragma HLS INTERFACE m_axi port=a offset=slave bundle=gmem
#pragma HLS INTERFACE m_axi port=b offset=slave bundle=gmem
#pragma HLS INTERFACE s_axilite port=a  bundle=control
#pragma HLS INTERFACE s_axilite port=b bundle=control
#pragma HLS INTERFACE s_axilite port=bursts bundle=control
#pragma HLS INTERFACE s_axilite port=burstlength bundle=control
#pragma HLS INTERFACE s_axilite port=mode bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control

  int burstbuffer[BURSTBUFFERSIZE];
  if(mode==0) return;
  if(burstlength>BURSTBUFFERSIZE) return;

  int *a_addr = a;
  int *b_addr = b;
  for(unsigned int burst=0;burst<bursts;burst++){
    if(mode & MODE_READ){
      memcpy(burstbuffer, a_addr, burstlength*sizeof(int));
    }
    if(mode & MODE_WRITE){
      memcpy(b_addr, burstbuffer, burstlength*sizeof(int));
    }
    if(!(mode & MODE_NOSTRIDE)){
      a_addr += burstlength;
      b_addr += burstlength;
    }
  }

 return;
}
