#include <stdio.h>
#include <stdlib.h>
#include <stdfix.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <math.h>

typedef uint8_t vect[4]; //32 bit size block

vect iter_key[32]; //iter key and replacement table
static unsigned char Pi[8][16]=
{
  {1,7,14,13,0,5,8,3,4,15,10,6,9,12,11,2},
  {8,14,2,5,6,9,1,12,15,4,11,0,13,10,3,7},
  {5,13,15,6,9,2,12,10,11,7,8,1,4,3,14,0},
  {7,15,5,10,8,1,6,13,0,9,3,14,11,4,2,12},
  {12,8,2,1,13,4,15,6,7,0,10,5,3,14,9,11},
  {11,3,5,8,2,15,10,13,14,1,7,4,12,9,6,0},
  {6,8,2,3,9,10,5,12,1,14,4,7,11,13,0,15},
  {12,4,6,2,10,5,11,9,14,8,13,7,0,3,15,1}
};

static void
Blk_4_Print(uint8_t *key) //Key print after gen
{
    int i;
    for (i = 0; i < 4; i++){
        printf("%02x", key[i]);
    }
    printf("\n");
}

//first non-linear transformation
static void NBT(const uint8_t *in_data, uint8_t *out_data)
{
  uint8_t first_part_byte, sec_part_byte;
  int i;
  for (i = 0; i < 4; i++)
  {
    // Here we are getting first 4 bits of a byte
    first_part_byte = (in_data[i] & 0xf0) >> 4;
    // second 4 bits
    sec_part_byte = (in_data[i] & 0x0f);
    // Transformation according to replacement table
    first_part_byte = Pi[i * 2][first_part_byte];
    sec_part_byte = Pi[i * 2 + 1][sec_part_byte];
    // Get two parts of a byte together
    out_data[i] = (first_part_byte << 4) | sec_part_byte;
  }
}
/*Every first vector byte XORs with the needed second vector byte and goes out to the third vector:
*/
static void XOR(const uint8_t *first_vector, const uint8_t *second_vector, uint8_t *out_vector)
{
  int i;
  for (i = 0; i < 4; i++)
   out_vector[i] = first_vector[i]^second_vector[i];
}

//Adding in a ring? mod 2 deg 32
static void XOR_32(const uint8_t *first_vector, const uint8_t *second_vector, uint8_t *out_vector)
{
  int i;
  unsigned int internal = 0;
  for (i = 3; i >= 0; i--)
  {
    internal = first_vector[i] + second_vector[i] + (internal >> 8);
    out_vector[i] = internal & 0xff;
  }
}
/*This is adding of right block half with iter key (mod 32)
+ non-linear transformation and left move to 11 ranks
*/
static void conversions_g(const uint8_t *key, const uint8_t *blk, uint8_t *out_data)
{
  uint8_t internal[4];
  uint32_t out_data_32;
  // Add mod 32 the right half of the block with the iteration key
  XOR_32(blk, key, internal);
  // Non-linear transf of res
  NBT(internal, internal);
  //Transf 4-byte vect into 32-bit number
  out_data_32 = internal[0];
  out_data_32 = (out_data_32 << 8) + internal[1];
  out_data_32 = (out_data_32 << 8) + internal[2];
  out_data_32 = (out_data_32 << 8) + internal[3];
  // Cycle move left to 11 ranks
  out_data_32 = (out_data_32 << 11)|(out_data_32 >> 21);
  // Transf 32 bit res back into 4-byte vect
  out_data[3] = out_data_32;
  out_data[2] = out_data_32 >> 8;
  out_data[1] = out_data_32 >> 16;
  out_data[0] = out_data_32 >> 24;
}
/*addition mod 2 of the result of the transformation g with the right half
 block and exchange of content between the right and left parts of the block*/
static void conversions_G(const uint8_t *key, const uint8_t *blk, uint8_t *out_data) // ???‚?µ???°?†???? ?????µ???±???°?·?????°??????
{
  uint8_t blk_r[4]; // Right block half
  uint8_t blk_l[4]; // Left block half
  uint8_t G[4];

  int i;
  //Divide 64-bit block into two parts
  for(i = 0; i < 4; i++)
  {
    blk_r[i] = blk[4 + i];
    blk_l[i] = blk[i];
  }

  // g transf
  conversions_g(key, blk_r, G);
  // XOR g with the left half
  XOR(blk_l, G, G);

  for(i = 0; i < 4; i++)
  {
    // Write into left half def from right
    blk_l[i] = blk_r[i];
    // XOR res into right block
    blk_r[i] = G[i];
  }

  // Get right and left half of block together
  for(i = 0; i < 4; i++)
  {
    out_data[i] = blk_l[i];
    out_data[4 + i] = blk_r[i];
  }

}

static void conversions_G_Fin(const uint8_t *key, const uint8_t *blk, uint8_t *out_data)
{
  uint8_t blk_r[4]; // right block half
  uint8_t blk_l[4]; // left block half
  uint8_t G[4];

  int i;
  // Divide 64 bit block into two parts
  for(i = 0; i < 4; i++)
  {
    blk_r[i] = blk[4 + i];
    blk_l[i] = blk[i];
  }

  // g transformation
  conversions_g(key, blk_r, G);
  // XOR g res with left block half
  XOR(blk_l, G, G);
  // Write XOR res into left block half
  for(i = 0; i < 4; i++)
    blk_l[i] = G[i];

  // Get left and right half together
  for(i = 0; i < 4; i++)
  {
    out_data[i] = blk_l[i];
    out_data[4 + i] = blk_r[i];
  }
}



void Expand_Key(const uint8_t *key) // Keys generation
{
    memcpy(iter_key[0], key, 4);
	memcpy(iter_key[1], key + 4, 4);
	memcpy(iter_key[2], key + 8, 4);
	memcpy(iter_key[3], key + 12, 4);
	memcpy(iter_key[4], key + 16, 4);
	memcpy(iter_key[5], key + 20, 4);
	memcpy(iter_key[6], key + 24, 4);
	memcpy(iter_key[7], key + 28, 4);
	memcpy(iter_key[8], key, 4);
	memcpy(iter_key[9], key + 4, 4);
	memcpy(iter_key[10], key + 8, 4);
	memcpy(iter_key[11], key + 12, 4);
	memcpy(iter_key[12], key + 16, 4);
	memcpy(iter_key[13], key + 20, 4);
	memcpy(iter_key[14], key + 24, 4);
	memcpy(iter_key[15], key + 28, 4);
	memcpy(iter_key[16], key, 4);
	memcpy(iter_key[17], key + 4, 4);
	memcpy(iter_key[18], key + 8, 4);
	memcpy(iter_key[19], key + 12, 4);
	memcpy(iter_key[20], key + 16, 4);
	memcpy(iter_key[21], key + 20, 4);
	memcpy(iter_key[22], key + 24, 4);
	memcpy(iter_key[23], key + 28, 4);
	memcpy(iter_key[24], key + 28, 4);
  	memcpy(iter_key[25], key + 24, 4);
  	memcpy(iter_key[26], key + 20, 4);
  	memcpy(iter_key[27], key + 16, 4);
  	memcpy(iter_key[28], key + 12, 4);
  	memcpy(iter_key[29], key + 8, 4);
  	memcpy(iter_key[30], key + 4, 4);
  	memcpy(iter_key[31], key, 4);

    printf("Iteration cipher keys:\n");
    int i;
    for (i = 0; i < 32; i++)
    {
        printf("K%d=", i+1);
        Blk_4_Print(iter_key[i]);
    }
}
/*encryption is performed through thirty-two iterations, from the first to the thirty-first, using
transformation G and thirty-second with the final G transformation:*/
void Encrypt(const uint8_t *blk, uint8_t *out_blk) //encryption
{
    int i;
    conversions_G(iter_key[0], blk, out_blk);// first transformation
    for(i = 1; i < 31; i++)
        conversions_G(iter_key[i], out_blk, out_blk); // 2-30 transformation
    conversions_G_Fin(iter_key[31], out_blk, out_blk); // last transformation


      printf("encrypted text:\n");
    for (i = 0; i < 8; i++)
        printf("%02x", out_blk[i]);
    printf("\n");
}
void Decript(const uint8_t *blk, uint8_t *out_blk)
{
  int i;
    printf("Decripted text:\n");
    for (i = 0; i < 8; i++)
    printf("%02x", blk[i]);
    printf("\n");
  conversions_G(iter_key[31], blk, out_blk);
  for(i = 30; i > 0; i--)
    conversions_G(iter_key[i], out_blk, out_blk);
  conversions_G_Fin(iter_key[0], out_blk, out_blk);
    printf("opened text:\n");
    for (i = 0; i < 8; i++)
        printf("%02x", out_blk[i]);
    printf("\n");
}
//block finishing
void finish_writing_txt(char name_file[]){
    char c;
    int t = 0,u = 0;
    const char writ = '1';
    FILE* file_r = fopen(name_file,"r");
    while(!feof(file_r)){
        c = fgetc(file_r);
        t++;
    }
    t--;
    fclose(file_r);
    if(t%16 != 0){
    t = 16 - (t % 16);
    FILE* file = fopen(name_file,"a");
    while(u < t){
        fputc((int)writ,file);
        u++;
    }
    fclose (file);
    }
}
// CBC MODE.
// XOR each element with init vect
void CBC(uint8_t *input, uint8_t *iv ){
    uint8_t i;
    for(i = 0; i < 8; i++){
        input[i] ^= iv[i];
    }
    Encrypt(input, input);
    memcpy(input, iv, 8);
}

void scan_blk_CBC(uint8_t *out_blk){ // CBC block scan
    static unsigned char encrypt_test_string[8];

    int j = 0,k = 0;
    char name_file[10];
    char *vect_in;
    char c;
    vect_in = (char*) malloc (sizeof(char));
    printf("enter the name of the file that contains the plaintext\n name_txt -");
    scanf("%s",&name_file);
    printf("\n");
    finish_writing_txt(name_file);
    FILE* text = fopen(name_file,"r");
    if(text == NULL){
            printf("the file with the text did not open\n");
            exit(1);
    }else printf("the file with the text is open\n");

    printf("enter the name of the file that contains the plaintext\n name_vect.txt -");
    scanf("%s",&name_file);
    printf("\n");
    FILE* vect = fopen(name_file,"r");
    if(vect == NULL){
            printf("the file with the text did not open\n");
            exit(2);
    }else printf("the file with the text is open\n");
        while(!feof(vect)){
            fscanf(vect,"%02x",&vect_in[j]);
            j++;
        }
    fclose(vect);

    k++;
   while(!feof(text)){
        for(j = 0; j < 8; j++){
            fscanf(text,"%02x",&encrypt_test_string[j]);
        }
        CBC(encrypt_test_string, vect_in);
    }fclose(text);

}
int scan_blk_ECB_Decript(uint8_t *out_blk){// ECB block scan
    static unsigned char decrypt_test_string[8];

    int j = 0;
    char name_file[10];
    printf("enter the file that you want to decrypt\n name_txt -");
    scanf("%s",&name_file);
    printf("\n");
    finish_writing_txt(name_file);
    FILE* text = fopen(name_file,"r");
    if(text == NULL){
            printf("the file with the text did not open\n");
            exit(5);
    }else printf("the file with the text is open\n");

    while(!feof(text)){
        for(j = 0; j < 8; j++){
            if(text != EOF){
            fscanf(text,"%02x",&decrypt_test_string[j]);
            }
        }
         if(!feof(text))(Decript(decrypt_test_string, out_blk));

    }
    fclose(text);

}
int scan_blk_ECB(uint8_t *out_blk){// ECB block scan
    static unsigned char encrypt_test_string[8];

    int j = 0;
    char name_file[10];
    printf("enter the name of the file that contains the plaintext\n name_txt.txt -");
    scanf("%s",&name_file);
    printf("\n");
    finish_writing_txt(name_file);
    FILE* text = fopen(name_file,"r");
    if(text == NULL){
            printf("the file with the text did not open\n");
            exit(3);
    }else printf("the file with the text is open\n");

    while(!feof(text)){
        for(j = 0; j < 8; j++){
            if(text != EOF){
            fscanf(text,"%02x",&encrypt_test_string[j]);
            }
        }
         if(!feof(text)){
            printf("opned text:\n");
            for (j = 0; j < 8; j++)
                printf("%02x", encrypt_test_string[j]);
            printf("\n");
            (Encrypt(encrypt_test_string, out_blk));
         }

    }
    fclose(text);

}
void scan_key(){//Key scan
    static unsigned char sc_key[32];
    int j;
    char name_file[10];
    printf("enter the name of the file that contains the key\n name_key.txt-");
    scanf("%s",&name_file);
    printf("\n");
    FILE* open_txt = fopen(name_file,"r");
    if(open_txt == NULL){
            printf("the file with the key did not open\n");
            exit(4);
    }else printf("the file with the key is open\n");
    for(j = 0; j < 32; j++){
        fscanf(open_txt,"%02x",&sc_key[j]);
    }

    fclose(open_txt);
    Expand_Key(sc_key);
}
int main(){
    printf("-       -     -     -------  -       -     -\n");
    printf("--     --   -   -   -        --     --   -   -\n");
    printf("- -   - -  -     -  -        - -   - -  -     -\n");
    printf("-  - -  -  -------  -   ---  -  - -  -  -------\n");
    printf("-   -   -  -     -  -     -  -   -   -  -     -\n");
    printf("-       -  -     -  -------  -       -  -     -\n");
    printf("\n");
    printf("Select a mode \nECB -> 1 \nCBC -> 2\n");
    printf("decrypt ECB ->3\nmode -> ");
    int mode;
    scanf("%d",&mode);
    printf("\n");
    uint8_t out_blk[4];

    scan_key();
    switch(mode){
        case(1):
            scan_blk_ECB(out_blk);
        break;
        case(2):
            scan_blk_CBC(out_blk);
        break;
        case(3):
            scan_blk_ECB_Decript(out_blk);
        break;
    }
    return 0;
}
