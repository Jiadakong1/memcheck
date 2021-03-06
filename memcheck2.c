/* 针对64位宽的DDR,32位宽的可能有问题 */

#include <stdio.h>
#include <stdlib.h>

#define rand32() ((unsigned int) rand() | ( (unsigned int) rand() << 16))
#define rand64() (((unsigned long) rand32()) << 32 | ((unsigned long) rand32()))
#define rand_ul() rand64()


unsigned long value = 0x0;

const static unsigned long long pattern[] = {
	0xaaaaaaaaaaaaaaaaULL,
	0xccccccccccccccccULL,
	0xf0f0f0f0f0f0f0f0ULL,
	0xff00ff00ff00ff00ULL,
	0xffff0000ffff0000ULL,
	0xffffffff00000000ULL,
	0x00000000ffffffffULL,
	0x0000ffff0000ffffULL,
	0x00ff00ff00ff00ffULL,
	0x0f0f0f0f0f0f0f0fULL,
	0x3333333333333333ULL,
	0x5555555555555555ULL
};
const static unsigned long long otherpattern = 0x0123456789abcdefULL;

static int memcheck_help(const char* str)
{
    const char *s = str;
    unsigned long len;
    int c;

    do {                //得到第一个字符，跳过空格
        c = *s++;
    } while (c == ' ');

    if(c == '-')
    {
        if((*s) == 'h')
        {
            printf( "Usage:\n"
                    "   memcheck [size(MB)] [loop]\n"
                    "   -size default : 100(MB)\n"
                    "   -loop default : 1\n");
            return 1;
        }

    }
    return 0;
}

static unsigned long str10_to_u32(const char* str)
{
    const char *s = str;
    unsigned long len;
    int c;

    do {                //得到第一个字符，跳过空格
        c = *s++;
    } while (c == ' ');

    for (len = 0; (c >= '0') && (c <= '9'); c = *s++) {
        c -= '0';
        len *= 10;
        len += c;
    }
    return len;
}


static unsigned long str16_to_u32(const char* str)
{
    const char *s = str;
    unsigned long len;
    int c;

    do {                //得到第一个字符，跳过空格
        c = *s++;
    } while (c == ' ');
    if(c == '0')
    {
        c = *s++;
    }
    else
    {
        printf("wrong number!\n");
        return -1;
    }


    if((c == 'x') || (c == 'X'))
    {
        c = *s++;
    }
    else
    {
        printf("wrong number!\n");
        return -1;
    }


    for (len = 0; ((c >= '0') && (c <= '9')) || ((c >= 'a') && (c <= 'f')) || ((c >= 'A') && (c <= 'F'))  ; c = *s++) {
        if(((c >= '0') && (c <= '9')) )
            c = c - '0';
        else if((c >= 'a') && (c <= 'f'))
            c = c - 'a' + 10;
        else if((c >= 'A') && (c <= 'F'))
            c = c - 'A' + 10;
        len *= 16;
        len += c;
    }
    return len;
}


static void move64(const unsigned long long *src, unsigned long long *dest)
{
	*dest = *src;
}

static int memory_post_dataline(unsigned long long * pmem)
{
	unsigned long long temp64 = 0;
	int num_patterns = 12; //ARRAY_SIZE(pattern);
	int i;
	unsigned int hi, lo, pathi, patlo;
	int ret = 0;
	//printf("memory_post_dataline\n");
	for ( i = 0; i < num_patterns; i++) {
		move64(&(pattern[i]), pmem++);
		/*
		 * Put a different pattern on the data lines: otherwise they
		 * may float long enough to read back what we wrote.
		 */
		move64(&otherpattern, pmem--);
		move64(pmem, &temp64);

		if (temp64 != pattern[i]){
			pathi = (pattern[i]>>32) & 0xffffffff;
			patlo = pattern[i] & 0xffffffff;

			hi = (temp64>>32) & 0xffffffff;
			lo = temp64 & 0xffffffff;
			printf("Memory (date line) error at %p, wrote %08x%08x, read %08x%08x !\n", (void *)pmem, pathi, patlo, hi, lo);
			ret = -1;
		}
	}
	return ret;
}

static int memory_post_addrline(unsigned long *testaddr, unsigned long *base, unsigned long size)
{
	unsigned long *target;
	unsigned long *end;
	unsigned long readback;
	unsigned long xor;
	int   ret = 0;
	//printf("memory_post_addrline\n");
	end = (unsigned long *)((unsigned long)base + size);	/* pointer arith! */
	xor = 0;
	for(xor = sizeof(unsigned long); xor > 0; xor <<= 1) {
		target = (unsigned long *)((unsigned long)testaddr ^ xor);
		if((target >= base) && (target < end)) {
			*testaddr = ~*target;
			readback  = *target;

			if(readback == *testaddr) {
				printf("Memory (address line) error at %p<->%p, XOR value %lx !\n", (void *)testaddr, (void *)target, xor);
				ret = -1;
			}
		}
	}
	return ret;
}


static int memory_post_test1(unsigned long *start,
			      unsigned long size,
			      unsigned long val)
{
	unsigned long i;
	unsigned long *mem = (unsigned long *) start;
	unsigned long readback;
	int ret = 0;
	printf("Pattern = %08lX\n" , val);
	for (i = 0; i < size / sizeof (unsigned long); i++) {
		mem[i] = val;
	}

	for (i = 0; i < size / sizeof (unsigned long) && !ret; i++) {
		readback = mem[i];
		if (readback != val) {
			printf("Memory error at%p wrote %lx, read %lx !\n",(void *)(mem + i), val, readback);
			ret = -1;
			break;
		}
	}
	return ret;
}


static int memory_post_test2(unsigned long *start, unsigned long size)
{
	unsigned long i;
	unsigned long *mem = (unsigned long *) start;
	unsigned long readback;
	int ret = 0;
	printf("Shift\n");
	for (i = 0; i < size / sizeof (unsigned long); i++) {
		mem[i] = 1 << (i % 32);
	}

	for (i = 0; i < size / sizeof (unsigned long) && !ret; i++) {
		readback = mem[i];
		if (readback != (1 << (i % 32))) {
			printf("Memory error at %p, wrote %d, read %lx !\n",(void *)(mem + i), 1 << (i % 32), readback);
			ret = -1;
			break;
		}
	}
	return ret;
}

static int memory_post_test3(unsigned long *start, unsigned long size)
{
	unsigned long i;
	unsigned long *mem = (unsigned long *) start;
	unsigned long readback;
	int ret = 0;
	printf("Sequence(positive)\n");
	for (i = 0; i < size / sizeof (unsigned long); i++) {
		mem[i] = i;
	}

	for (i = 0; i < size / sizeof (unsigned long) && !ret; i++) {
		readback = mem[i];
		if (readback != i) {
			printf("Memory error at %p, wrote %lx, read %lx !\n", (void *)(mem + i), i, readback);

			ret = -1;
			break;
		}
	}

	return ret;
}

static int memory_post_test4(unsigned long *start, unsigned long size)
{
	unsigned long i;
	unsigned long *mem = (unsigned long *) start;
	unsigned long readback;
	int ret = 0;
	printf("Sequence(negative)\n");
	for (i = 0; i < size / sizeof (unsigned long); i++) {
		mem[i] = ~i;
	}

	for (i = 0; i < size / sizeof (unsigned long) && !ret; i++) {
		readback = mem[i];
		if (readback != ~i) {
			printf("Memory error at %p, wrote %lx, read %lx !\n",(void *)(mem + i), ~i, readback);
			ret = -1;
			break;
		}
	}
	return ret;
}



static int memory_post_test_lines(unsigned long *start, unsigned long size)
{
	int ret = 0;

	ret = memory_post_dataline((unsigned long long *)start);

	if (!ret)
		ret = memory_post_addrline((unsigned long *)start, (unsigned long *)start, size);

	if (!ret)
		ret = memory_post_addrline((unsigned long *)(start+size-8),	(unsigned long *)start, size);

	return ret;
}

static int memory_post_test_patterns(unsigned long *start, unsigned long size)
{
	int ret = 0;

	ret = memory_post_test1(start, size, 0x00000000);

	if (!ret)
		ret = memory_post_test1(start, size, 0xffffffff);

    if (!ret)
		ret = memory_post_test1(start, size, 0x12345678);

    if (!ret)
        ret = memory_post_test1(start, size, 0x87654321);

	if (!ret)
		ret = memory_post_test1(start, size, 0x55555555);

	if (!ret)
		ret = memory_post_test1(start, size, 0xaaaaaaaa);

	if (!ret)
		ret = memory_post_test2(start, size);

	if (!ret)
		ret = memory_post_test3(start, size);

	if (!ret)
		ret = memory_post_test4(start, size);

	return ret;
}


int compare_regions(unsigned long *bufa, unsigned long *bufb, size_t count) {
    int r = 0;
    size_t i;
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    off_t physaddr;

    for (i = 0; i < count/sizeof (unsigned long); i++, p1++, p2++) {
        if (*p1 != *p2)
		{
        	printf("FAILURE: 0x%08lx != 0x%08lx at offset 0x%08lx.\n",
                    (unsigned long) *p1, (unsigned long) *p2, (unsigned long) (i * sizeof(unsigned long)));
			r = -1;
        }
    }
    return r;
}



int test_xor_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
        *p1++ ^= q;
		*p2 = value;
        *p2++ ^= q;
    }
    return compare_regions(bufa, bufb, count);
}


int test_sub_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        *p1++ -= q;
        *p2++ -= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_mul_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        *p1++ *= q;
        *p2++ *= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_div_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        if (!q) {
            q++;
        }
        *p1++ /= q;
        *p2++ /= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_or_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        *p1++ |= q;
        *p2++ |= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_and_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        *p1++ &= q;
        *p2++ &= q;
    }
    return compare_regions(bufa, bufb, count);
}

int test_seqinc_comparison(unsigned long *bufa, unsigned long *bufb, size_t count) {
    unsigned long *p1 = bufa;
    unsigned long *p2 = bufb;
    size_t i;
    unsigned long q = rand_ul();

    for (i = 0; i < count/sizeof (unsigned long); i++) {
		*p1 = value;
		*p2 = value;
        *p1++ = *p2++ = (i + q);
    }
    return compare_regions(bufa, bufb, count);
}


int main(int argc, char const *argv[]) {
    unsigned int loop = 1;
    unsigned int i = 0;
    unsigned long * pstart = NULL;
	unsigned long * pstart2 = NULL;
    unsigned long memsize = 100;

    if(argc > 1)
    {
        if(memcheck_help(argv[1]) == 1)
            return 0;
        memsize = (unsigned int)str10_to_u32(argv[1]);
    }


    if(argc > 2)
        loop = (unsigned int)str10_to_u32(argv[2]);

    printf("memsize = %luMB   loop = %u\n", memsize, loop);
    memsize = memsize*1024*1024;

	for(i=0; i<loop; i++)
    {
        //printf("loop %d :\n", i+1);
        pstart = (unsigned long*)malloc(memsize);
		printf("start_address = %p   memsize = %luMB   loop = %u\n", pstart, memsize/1024/1024, i+1);

        if(pstart == NULL)
        {
            printf("Error: Cannot allocate memory.\n");
            return -1;
        }

        memory_post_test_lines(pstart, memsize);

        memory_post_test_patterns(pstart, memsize);

		free(pstart);
		pstart = NULL;
        printf("\n");
    }

	memsize = memsize/2;
    for(i=0; i<loop; i++)
    {
        //printf("loop %d :\n", i+1);
        pstart = (unsigned long*)malloc(memsize);
		pstart2 = (unsigned long*)malloc(memsize);
		printf("start_address1 = %p  start_address2 = %p memsize = %luMB   loop = %u\n", pstart, pstart2, memsize/1024/1024, i+1);

        if((pstart == NULL) || (pstart2 == NULL)  )
        {
            printf("Error: Cannot allocate memory.\n");
            return -1;
        }

		printf("Compare XOR\n");
		test_xor_comparison(pstart, pstart2, memsize);

		printf("Compare SUB\n");
		test_sub_comparison(pstart, pstart2, memsize);

		printf("Compare MUL\n");
		test_mul_comparison(pstart, pstart2, memsize);

		printf("Compare DIV\n");
		test_div_comparison(pstart, pstart2, memsize);

		printf("Compare OR\n");
		test_or_comparison(pstart, pstart2, memsize);

		printf("Compare AND\n");
		test_and_comparison(pstart, pstart2, memsize);

		printf("Sequential Increment\n");
		test_seqinc_comparison(pstart, pstart2, memsize);

		free(pstart);
		pstart = NULL;

		free(pstart2);
		pstart2 = NULL;
        printf("\n");
    }

    return 0;
}
