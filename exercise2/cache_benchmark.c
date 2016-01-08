#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define STEP 256
#define MAX_CACHE 512 * 1024 // 6 Mb
#define TEST_FREQ 250
#define TEST_SIZE 1024*4096
#define NUM_ACCES_LENGTH 1024 
#define STRIDE    4160
#define NUM_ACCESS_SIZE 1000000


#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
    unsigned long long int x;
    __asm__ volatile(".byte 0x0f, 0x31" : "=A" (x));
    return x;
}

#elif defined(__x86_64__)

static __inline__ unsigned long long int rdtsc(void)
{
    unsigned long long int x;
    unsigned a, d;
    __asm__ volatile("rdtsc" : "=a" (a), "=d" (d));
    return ((unsigned long long)a) | (((unsigned long long)d) << 32);
}

#endif

int gcd(int n1, int n2)
{
    while(n1 != n2)
        if(n1>=n2) n1=n1-n2;
        else n2=n2-n1;

    return n1;
}

void cache_size_benchmark(int n, int num_access)
{
    unsigned long long int *test_vector;
    unsigned long long int begin = 0, end = 0;
    int stride = 4160;
    int i = 0;

    test_vector = malloc(n * sizeof(long long int));

    // avoid common divisors

    if(!(stride % n)) stride++;

    /* fill the test array with pointer values */

    for(i=0; i<n; i++) 
        test_vector[i] = ((unsigned long long int)(test_vector + i*sizeof(long long int) + stride)) % n;

    /* avoid cold cache misses */

    int value = 0;
    for(i=0; i<n; i++) value = test_vector[i];

    /* run the benchmark */

    long long int next_address = test_vector[0];
    begin = rdtsc();
    for(i=0; i<num_access; i++) next_address = test_vector[next_address];
    end = rdtsc();

    free(test_vector);

    printf("%d %lld\n", n, (end-begin)/num_access);
}


void cache_line_length_benchmark(int n, int stride) {

    unsigned char *test_vector;
    struct timeval current_time;
    int i;
    int sum = 0;
    int avg_time = 0;
    unsigned long long int begin = 0, end = 0;
    int num_access = n / stride;

    test_vector = malloc(n*sizeof(char));

    /* fill the test array with random values */

    gettimeofday(&current_time, NULL);
    srand(current_time.tv_usec);

    for(i=0; i<n; i+=sizeof(char)) {
        int rand_value = rand();
        memcpy(test_vector + i, &rand_value, sizeof(char));
    }

    /* run the benchmark */

    begin = rdtsc();
    for (i=0; i<n; i+=stride) sum += test_vector[i];
    end = rdtsc();

    free(test_vector);

    /* print results */

    printf("%d %llu\n", stride, (end-begin)/num_access);
}

int main() {

    long long int i = 0;

    // cache line length test
    // for (i=2; i<TEST_FREQ; i+=2) cache_line_length_benchmark(TEST_SIZE, i);

    // cache size test
     for(i=2; i<MAX_CACHE; i+=STEP) {

         // avoid common divisors
         int j = i;
         while(gcd(j, STRIDE) > 1 && j < i+STEP) j++;

         // perform test
         cache_size_benchmark(j, NUM_ACCESS_SIZE);
     }

    return 1;
}

static inline
double gettime(void) {
    struct timeval currenttime;
    gettimeofday(&currenttime, NULL);
    return ((double)currenttime.tv_sec*1000000 + (double)currenttime.tv_usec);
}


