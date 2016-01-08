#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>

#define TEST_FREQ 8192
#define TEST_SIZE 1024*4096
#define NUM_ACCES 1024 


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


void cache_line_size_benchmark(int n, int stride) {

    unsigned char test_vector[n];
    struct timeval current_time;
    int i;
    int sum = 0;
    int avg_time = 0;
    unsigned long long int begin = 0, end = 0;

    /* fill the test array with random values */

    gettimeofday(&current_time, NULL);
    srand(current_time.tv_usec);

    for(i=0; i<n; i+=sizeof(int)) {
        int rand_value = rand();
        memcpy(test_vector + i, &rand_value, sizeof(int));
    }

    /* run the benchmark */

    begin = rdtsc();
    for (i=0; i<NUM_ACCES; i++) sum += test_vector[i*stride];
    end = rdtsc();

    /* print results */

    printf("%d %llu\n", stride, (end-begin)/NUM_ACCES);
}

int main() {

    int i;
    for (i=2; i<TEST_FREQ; i*=2) cache_line_size_benchmark(TEST_SIZE, i);
    return 1;
}

static inline
double gettime(void) {
    struct timeval currenttime;
    gettimeofday(&currenttime, NULL);
    return ((double)currenttime.tv_sec*1000000 + (double)currenttime.tv_usec);
}


