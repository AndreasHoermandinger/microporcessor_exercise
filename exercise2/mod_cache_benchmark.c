.]'s'#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/hardirq.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <linux/proc_fs.h>


#define DEVICE_NAME "cache_benchmark"


/* cache line length test parameters */

#define STRIDE_MAX 512
#define TEST_SIZE 1024*4096

/* cache size test parameters */

#define STEP 64 / sizeof(uint64_t)
#define MAX_CACHE 512 * 1024 / sizeof(uint64_t)
#define STRIDE 4160
#define NUM_ACCESS 1000000


struct proc_dir_entry *proc_entry;
unsigned long flags;


int test_stride = 1;
int test_cache_size = STEP;
int sep = 0;


#if defined(__i386__)
static __inline__ unsigned long long rdtsc1(void)
{
    unsigned long long int x;
    __asm__ volatile(".byte 0x0f, 0x31" : "=A" (x));
    return x;
}
#elif defined(__x86_64__)
static __inline__ uint64_t rdtsc1(void)
{
    unsigned a, d;

    __asm__ volatile(
        "cpuid\n\t"
        "rdtsc" : "=a" (a), "=d" (d));

    return ((uint64_t)a) | (((uint64_t)d) << 32);
}
#endif

int gcd(int n1, int n2)
{
    while (n1!=n2) if (n1>=n2) n1=n1-n2; else n2=n2-n1;
    return n1;
}

uint64_t cache_size_benchmark(int n)
{
    uint64_t *test_vector;
    uint64_t begin = 0, end = 0, total_time = 0, next_address = 0, value = 0;
    int stride = 4160, i = 0;

    test_vector = kmalloc(n * sizeof(uint64_t), GFP_KERNEL);

    /* fill the test array with pointer values */

    for(i=0; i<n; i++)
        test_vector[i] = ((uint64_t)(test_vector + i * sizeof(uint64_t) + stride)) % n;

    /* run the benchmark */

    next_address = test_vector[0];

    preempt_disable();
    raw_local_irq_save(flags);

    /* load test_vector into cache in order to avoid cold cache misses */
    for(i=0; i<n; i++) value += test_vector[i];
    value = 0;

    for(i=0; i<NUM_ACCESS; i++)
    {
        begin = rdtsc1();
        next_address = test_vector[next_address];
        end = rdtsc1();
        value += next_address;
        total_time += (end-begin);
    }
    raw_local_irq_restore(flags);
    preempt_enable();

    /* clean up and return the result */

    kfree(test_vector);
    return total_time;
}

uint64_t cache_line_length_benchmark(int stride)
{
    unsigned char *test_vector;
    unsigned char seed = 0;
    int i, index = 0, sum = 0, num_access = TEST_SIZE / STRIDE_MAX;
    uint64_t begin = 0, end = 0, total_time = 0;

    /* allocate the test array and fill it with random values */

    test_vector = kmalloc(TEST_SIZE * sizeof(char), GFP_KERNEL);
    seed = rdtsc1() >> (sizeof(uint64_t) - 1);
    for(i=0; i<TEST_SIZE; i++) test_vector[i] = seed + i*stride;

    /* run the benchmark */

    preempt_disable();
    raw_local_irq_save(flags);
    for (i=0; i<num_access; i++)
    {
        begin = rdtsc1();
        sum += test_vector[index];
        end = rdtsc1();

        total_time += (end-begin);
        index += stride;
    }
    raw_local_irq_restore(flags);
    preempt_enable();

    /* clean up and print results */

    kfree(test_vector);
    return total_time;
}

static ssize_t benchmark_read(struct file *flip, char __user *buffer, size_t buffer_size, loff_t *offp)
{
    uint64_t cycles = 0;
    char *result_str = kmalloc(100*sizeof(char), GFP_KERNEL);
    char *pos;
    int len = 0, max_line_len = 30;
    size_t ret_val = 0;
    char *sep_msg = "\nsize test begin\n\n";

    if(test_cache_size >= MAX_CACHE)
    {
        test_stride = 1;
        test_cache_size = STEP;
        sep = 0;
        return 0;
    }

    pos = buffer;

    /* cache line length test */

    //test_stride = STRIDE_MAX;
    while(pos + max_line_len <= buffer + buffer_size && test_stride < STRIDE_MAX)
    {
        cycles = cache_line_length_benchmark(test_stride);

        sprintf(result_str, "%d,%llu\n", test_stride, cycles);
        len = strlen(result_str);
        copy_to_user(pos, result_str, len);
        pos += len;
        ret_val += len;
        test_stride++;
    }

    if(pos + strlen(sep_msg) <= buffer + buffer_size && !sep)
    {
        sep = 1;
        len = strlen(sep_msg);
        copy_to_user(pos, sep_msg, len);
        pos += len;
        ret_val += len;
    }

    /* cache size test */
    //test_cache_size = MAX_CACHE;
    while(pos + max_line_len <= buffer + buffer_size && test_cache_size < MAX_CACHE)
    {
        // avoid common divisors
        int j = test_cache_size;
        while(gcd(j, STRIDE) > 1 && j < test_cache_size+STEP) j++;

        cycles = cache_size_benchmark(j);

        sprintf(result_str, "%d,%llu\n", j, cycles);
        len = strlen(result_str);
        copy_to_user(pos, result_str, len);
        pos += len;
        ret_val += len;
        test_cache_size += STEP;
    }

    return ret_val;
}

struct file_operations proc_fops = {
read: benchmark_read
};

static int __init benchmod_init(void)
{
    proc_entry = proc_create_data(DEVICE_NAME, 0, NULL, &proc_fops, NULL);
    return 0;
}

static void __exit benchmod_exit(void)
{
    remove_proc_entry(DEVICE_NAME, NULL);
}

module_init(benchmod_init);
module_exit(benchmod_exit);
