#include <stdio.h>
#include <stdlib.h>

// Try to avoid endian issues with fwrite
void writeword(FILE* fp, unsigned int *buf)
{
  unsigned char c;
  int i;
  for (i = 0; i < 4; i++) {
    c = *buf & 0x000000FF;
    fwrite(&c, 1, 1, fp);
    *buf = *buf >> 8;
  }
}

int main(int argc, char * argv[])
{
  FILE *fp_in;
  FILE *fp_out;
  unsigned int buf = 0x00000000;
  unsigned char c = 0x00;
  int i;
  if (argc != 3)
  {
    fprintf(stderr, "Usage: %s <input bit> <output bin>\n", argv[0]);
    return 1;
  }

  fp_in = fopen(argv[1], "rb");
  if (!fp_in) {
    fprintf(stderr, "ERROR: Could not open %s\n", argv[1]);
    return 1;
  }
  fp_out = fopen(argv[2], "wb");
  if (!fp_out) {
    fprintf(stderr, "ERROR: Could not open %s\n", argv[2]);
    return 1;
  }
  // Find beginning of config
  while ((buf != 0xFFFFFFFF) &&
          (fread(&c, 1, 1, fp_in) == 1)) {
    buf = (buf << 8) | c;
  }
  while ((buf != 0x000000BB) &&
          (fread(&c, 1, 1, fp_in) == 1)) {
    buf = (buf << 8) | c;
  }
  // Dump config header
  buf = 0xFFFFFFFF;
  for (i = 0; i < 8; i++)
    fwrite(&buf, 1, 4, fp_out);
  buf = 0x000000BB;
  writeword(fp_out, &buf);

  i = 0;
  while (fread(&c, 1, 1, fp_in) == 1) {
    buf = (buf << 8) | c;
    if (++i == 4) {
      writeword(fp_out, &buf);
      i = 0;
    }
  }

  fclose(fp_in);
  fclose(fp_out);

  return 0;
}


