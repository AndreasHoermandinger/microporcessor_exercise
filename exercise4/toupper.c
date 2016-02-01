#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include "options.h"
#include <arm_neon.h>



int debug = 0;
double *results;
double *ratios;
unsigned long   *sizes;
unsigned current_size;

int no_sz = 1, no_ratio =1, no_version=1;

#define ARRAY_SIZE (sizeof((__128)/sizeof(int)))
#define NUM_THREADS 4



static inline
double gettime(void) {
    struct timeval currenttime;
    gettimeofday(&currenttime, NULL);
    return ((double)currenttime.tv_sec*1000000 + (double)currenttime.tv_usec);
}


static void toupper_simple(char * text) {
    char *c = text;
    for(c = text; *c != '\0';c++)
        if(*c >= 0x61) *c = ((char)*c - 0x20);
}

static void toupper_optimised_neon64(char *text) {

    uint8x8_t cmp_v = vdup_n_u8(0x60);		// Create an 64bit vector (8 x int8) and fill it with scalar.
    uint8x8_t and_v = vdup_n_u8(0x20);
    uint8x8_t str_v;
    uint8x8_t tmp_v;

    int length = strlen(text);
    int modulus = length % 8;	// needed to check if can fill the last vector

    int i, j;
    for (i = 0; i < length - modulus; i += 8) {
      // load chunks of 8 characters into the vector
      str_v = vld1_u8(&text[i]);

      tmp_v = vcgt_u8(str_v, cmp_v);
      tmp_v = vand_u8(tmp_v, and_v);
      str_v = vsub_u8(str_v, tmp_v);

      // store chunks of 8 characters back to the text array
      vst1_u8(&text[i], str_v);
    }

    // finally check the remaining text for lower case letters
    // TODO
    if (modulus != 0) {
      toupper_simple(&text[length - modulus]);
    }
}

static void toupper_optimised_neon128(char *text) {

    uint8x16_t cmp_v = vdupq_n_u8(0x60);                // Create an 128bit vector (16 x int8) and fill it with scalar.
    uint8x16_t and_v = vdupq_n_u8(0x20);
    uint8x16_t str_v;
    uint8x16_t tmp_v;

    int length = strlen(text);
    int modulus = length % 16; 

    int i, j;
    for (i = 0; i < length - modulus; i += 16) {
      // load chunks of 16 characters of the text into the vector register
      str_v = vld1q_u8(&text[i]);

      tmp_v = vcgtq_u8(str_v, cmp_v);
      tmp_v = vandq_u8(tmp_v, and_v);
      str_v = vsubq_u8(str_v, tmp_v);

      // store chunks of 16 characters back to the text array
      vst1q_u8(&text[i], str_v);
    }

    // finally check the remaining text for lower case letters
    // TODO
    if (modulus != 0) {
      toupper_simple(&text[length - modulus]);
    }
}

/*static void toupper_optimised(char * text) {


    __m64 cmp_vector = _mm_set1_pi8(0x60);
    __m64 and_vector = _mm_set1_pi8(0x20);
    __m64 tmp_vector;
    char *c;

    int length = strlen(text);

    for(c = text; c < text + length; c+=8)
    {
        __m64 *cv = (__m64*)c;

        tmp_vector = _mm_cmpgt_pi8(*cv,cmp_vector);
        tmp_vector = _m_pand(tmp_vector,and_vector);
        *cv = _mm_subs_pu8(*cv, tmp_vector);


    }
    //if(debug)printf("%s",text);
}
*/

/*static void toupper_optimised_sse(char * text) {

    __m128i text_vector, cmp_vector, tmp_vector, and_vector;
    cmp_vector = _mm_set1_epi8(0x60);
    and_vector = _mm_set1_epi8(0x20);
    char *c;

    int length = strlen(text);

    for(c = text; c < text + length; c+=16)
    {

        __m128i cv =_mm_set_epi8(c[15],c[14],c[13],c[12],c[11],c[10],c[9],c[8],c[7],c[6],c[5],c[4],c[3],c[2],c[1],c[0]);

        tmp_vector = _mm_cmpgt_epi8(cv,cmp_vector);
        tmp_vector = _mm_and_si128(tmp_vector,and_vector);
        tmp_vector = _mm_subs_epu8(cv, tmp_vector);
        _mm_storeu_si128((__m128i*)c, tmp_vector);


    }
    //if(debug)printf("%s",text);
}

*/

/*****************************************************************/


// align at 16byte boundaries
void* mymalloc(unsigned long int size)
{
     void* addr = malloc(size+32);
     return (void*)((unsigned long int)addr /16*16+16);
}

char createChar(int ratio){
	char isLower = rand()%100;

	// upper case=0, lower case=1
	if(isLower < ratio)
		isLower =0;
	else
		isLower = 1;

	char letter = rand()%26+1; // a,A=1; b,B=2; ...

	return 0x40 + isLower*0x20 + letter;

}

char * init(unsigned long int sz, int ratio){
    int i=0;
    char *text = (char *) mymalloc(sz+1);
    srand(1);// ensures that all strings are identical
    for(i=0;i<sz;i++){
			char c = createChar(ratio);
			text[i]=c;
	  }
    text[i] = '\0';
    return text;
}



/*
 * ******************* Run the different versions **************
 */

typedef void (*toupperfunc)(char *text);

void run_toupper(int size, int ratio, int version, toupperfunc f, const char* name)
{
   double start, stop;
		int index;

		index =  ratio;
		index += size*no_ratio;
		index += version*no_sz*no_ratio;

    char *text = init(sizes[size], ratios[ratio]);
    current_size = sizes[size];


    if(debug) printf("Before: %.40s...\n",text);

    start = gettime();
    (*f)(text);
    stop = gettime();
    results[index] = stop-start;

    if(debug) printf("After:  %.40s...\n",text);
}

struct _toupperversion {
    const char* name;
    toupperfunc func;
} toupperversion[] = {
    { "simple",    toupper_simple },
//    { "optimised_mmx", toupper_optimised },
//    { "optimised_sse", toupper_optimised_sse },
//    { "optimised_parallel", toupper_optimised_parallel },  
    { "optimized_neon64", toupper_optimised_neon64 },
    { "optimized_neon128", toupper_optimised_neon128 },
    { 0,0 }
};


void run(int size, int ratio)
{
	int v;
	for(v=0; toupperversion[v].func !=0; v++) {
		run_toupper(size, ratio, v, toupperversion[v].func, toupperversion[v].name);
	}

}

void printresults(){
	int i,j,k,index;
	printf("%s\n", OPTS);

	for(j=0;j<no_sz;j++){
		for(k=0;k<no_ratio;k++){
			printf("Size: %ld \tRatio: %f \tRunning time:", sizes[j], ratios[k]);
			for(i=0;i<no_version;i++){
				index =  k;
				index += j*no_ratio;
				index += i*no_sz*no_ratio;
				printf("\t%s: %f", toupperversion[i].name, results[index]);
			}
			printf("\n");
		}
	}
}

int main(int argc, char* argv[])
{
    unsigned long int min_sz=800000, max_sz = 0, step_sz = 10000;
		int min_ratio=50, max_ratio = 0, step_ratio = 1;
		int arg,i,j,v;
		int no_exp;

		for(arg = 1;arg<argc;arg++){
			if(0==strcmp("-d",argv[arg])){
				debug = 1;
			}
			if(0==strcmp("-l",argv[arg])){
					min_sz = atoi(argv[arg+1]);
					if(arg+2>=argc) break;
					if(0==strcmp("-r",argv[arg+2])) break;
					if(0==strcmp("-d",argv[arg+2])) break;
					max_sz = atoi(argv[arg+2]);
					step_sz = atoi(argv[arg+3]);
			}
			if(0==strcmp("-r",argv[arg])){
					min_ratio = atoi(argv[arg+1]);
					if(arg+2>=argc) break;
					if(0==strcmp("-l",argv[arg+2])) break;
					if(0==strcmp("-d",argv[arg+2])) break;
					max_ratio = atoi(argv[arg+2]);
					step_ratio = atoi(argv[arg+3]);
			}

		}
    for(v=0; toupperversion[v].func !=0; v++)
		no_version=v+1;
		if(0==max_sz)  no_sz =1;
		else no_sz = (max_sz-min_sz)/step_sz+1;
		if(0==max_ratio)  no_ratio =1;
		else no_ratio = (max_ratio-min_ratio)/step_ratio+1;
		no_exp = v*no_sz*no_ratio;
		results = (double *)malloc(sizeof(double[no_exp]));
		ratios = (double *)malloc(sizeof(double[no_ratio]));
		sizes = (long *)malloc(sizeof(long[no_sz]));

		for(i=0;i<no_sz;i++)
			sizes[i] = min_sz + i*step_sz;
		for(i=0;i<no_ratio;i++)
			ratios[i] = min_ratio + i*step_ratio;

		for(i=0;i<no_sz;i++)
			for(j=0;j<no_ratio;j++)
				run(i,j);

		printresults();
    return 0;
}
