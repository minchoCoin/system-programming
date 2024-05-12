/*
학번 : 201924451
이름 : 김태훈
*/
#include "cachelab.h"
#include <stdbool.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

typedef unsigned long long int mem_addr;

typedef struct _cache_line{
    bool valid; //valid bit
    mem_addr tag; //tag
    unsigned long long lru; //lru

}Cache_line;

typedef Cache_line* Cache_set;
typedef Cache_set* Cache;



int verbosity=0; // if verbosity=1 then print trace
int s=0; //set index bits
int b=0; //block offset bits
int E=0; //number of lines per set

char* trace_file = NULL; // trace file name

int S; // number of sets 2^s
int B; // block size 2^b

int miss_count=0;
int hit_count=0;
int eviction_count=0;
unsigned long long lru_counter = 1; // lru가 높을 수록 최근에 사용한 것임

Cache cache; // 시뮬레이션에 사용할 캐시
mem_addr set_index_mask; // shift로 block bit를 없애고, tag + set bit에서 set bit만 추출

/* 
 * initCache - Allocate memory, write 0's for valid and tag and LRU
 * also computes the set_index_mask
 */
void initCache()
{
    cache = (Cache_set*) malloc(sizeof(Cache_set)*S);
    for(int i=0;i<S;++i){
        cache[i] = (Cache_line*) malloc(sizeof(Cache_line)*E);
    }
    for(int i=0;i<S;++i){
        for(int j=0;j<E;++j){
            cache[i][j].valid=false;
            cache[i][j].tag=0;
            cache[i][j].lru=0;
        }
    }
    set_index_mask = (mem_addr) (pow(2,s)-1);
}


/* 
 * freeCache - free allocated memory
 */
void freeCache()
{
    for(int i=0;i<S;++i) free(cache[i]);
    free(cache);
}


/* 
 * accessData - Access data at memory address addr.
 *   If it is already in cache, increast hit_count
 *   If it is not in cache, bring it in cache, increase miss count.
 *   Also increase eviction_count if a line is evicted.
 */
void accessData(mem_addr addr)
{
    mem_addr seti = (addr>>b) & set_index_mask;
    mem_addr tag = addr >>(s+b);

    for(int line=0;line<E;++line){
        if(cache[seti][line].valid && cache[seti][line].tag == tag){
            cache[seti][line].lru = lru_counter++;
            hit_count++;
            return;
        }
    }
    //이 라인까지 왔다는 것은 cache miss가 발생했다는 것이다.
    miss_count++;
    unsigned long long min_lru = ULLONG_MAX;

    int replace;
    
    bool find=false;

    //빈 캐시를 찾는다.
    for(int line=0;line<E;++line){
        if(cache[seti][line].valid == false){
            find=true;
            replace = line;
            break;
        }
    }

    //빈 캐시가 없으면 lru가 가장 작은 것을 방출하고 교체한다.
    if(!find){
        for(int line=0;line<E;++line){
            if(cache[seti][line].lru < min_lru){
                replace = line;
                min_lru = cache[seti][line].lru;
            }
        }
        eviction_count++;
    }

    cache[seti][replace].tag=tag;
    cache[seti][replace].lru=lru_counter++;
    cache[seti][replace].valid=true;
    
}

void replayTrace(char* trace_fn)
{
    char buf[1000];
    mem_addr addr=0;
    unsigned int len=0;
    FILE* trace_fp = fopen(trace_fn, "r");

    if(!trace_fp){
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);
    }

    while( fgets(buf, 1000, trace_fp) != NULL) {
        if(buf[1]=='S' || buf[1]=='L' || buf[1]=='M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if(verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

            accessData(addr);

            /* If the instruction is R/W then access again */
            if(buf[1]=='M')
                accessData(addr);
            
            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}

/*
 * printUsage - Print usage info
 */
void printUsage(char* argv[])
{
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of set index bits.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of block offset bits.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}

/*
 * main - Main routine 
 */
int main(int argc, char* argv[])
{
    char c;

    while( (c=getopt(argc,argv,"s:E:b:t:vh")) != -1){
        switch(c){
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        case 'v':
            verbosity = 1;
            break;
        case 'h':
            printUsage(argv);
            exit(0);
        default:
            printUsage(argv);
            exit(1);
        }
    }

    /* Make sure that all required command line args were specified */
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        printUsage(argv);
        exit(1);
    }

    /* Compute S, E and B from command line args */
    S = (unsigned int) pow(2, s);
    B = (unsigned int) pow(2, b);
 
    /* Initialize cache */
    initCache();

#ifdef DEBUG_ON
    printf("DEBUG: S:%u E:%u B:%u trace:%s\n", S, E, B, trace_file);
    printf("DEBUG: set_index_mask: %llu\n", set_index_mask);
#endif
 
    replayTrace(trace_file);

    /* Free allocated memory */
    freeCache();

    /* Output the hit and miss statistics for the autograder */
    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
