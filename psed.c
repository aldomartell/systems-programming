#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <pthread.h>
#include<semaphore.h>
#include <fcntl.h> //open
#include <sys/mman.h>//needed for mmap
//#include <sys/sysinfo.h>//needed for get_nprocs()
#include <sys/stat.h>//needed for stat structure and fstat
#include <unistd.h>//sysconf

#define DEFAULT_CHUNK_SZ (1 << 20)//1MB if the user doesnt pass -s we just use that 
#ifdef __linux__
#include <sys/sysinfo.h>

#define num_cpus() get_nprocs() //Ask how many processes are online
#else
#define num_cpus() ((int)sysconf(_SC_NPROCESSORS_ONLN))
#endif



#define Q_CAP    64 //queue capacity 
#define MAX_THR  64//max threads, thread count

void print_help() {//prints instructions if user asks for help same as wsed.c first project
    printf("Usage: psed [-m mode] [initial] [final] [file]\n");
    printf("-s chunk_size\n");
    printf("\tSize of each file chunk processed by a thread. \n");
    printf("-m  mode\n");

    printf("\t\"substitution\" or \"translation\". Default: substitution.\n");
    
    printf("initial & final\n");
    printf("\t in substitution, all occurrences of initial in [file] are replaced with final\n");
    printf("\t in translation, each character in initial is replaced with the corresponding character from final in [file]\n");

}

typedef struct {//stores one chunk of file data
    char *in;//pointer to input chunk from nmap
    size_t in_len;
    char *out;
    size_t out_len;
    size_t out_cap;
    int idx;
}chunk;

void buf_append(chunk *c, const char *data, size_t len) {//we append the data into output buffer
    if (c->out_len +len >c->out_cap){//if output buffer is full, grow it
        c->out_cap = (c->out_len + len) * 2;
        c->out = realloc(c->out, c->out_cap);//resize the memory
    }
    memcpy(c->out + c->out_len, data, len);//copy the new data into output
    c->out_len += len;
}

void substitution(chunk *c, const char *pattern, const char *replacement){//we find every occurrence of pattern and replace with the replacement
    size_t pat_len= strlen(pattern);//length of pattern
    size_t rep_len  =strlen(replacement);//replacement length
    char *pos = c->in;//current position while scanning through chunk
    char *end = c->in + c->in_len;
    char *match;
 
    while (pos <end && (match = (char *)memmem(pos, end-pos, pattern, pat_len))!= NULL) {//we keep searching until the end of the chunk, we use memmem instead of strstr because the chunk isnt null
        buf_append(c, pos, match - pos);//copy everything before the match
        buf_append(c, replacement, rep_len);//insert replacement text
        pos = match + pat_len;//move past matched pattern
    }
    buf_append(c, pos, end-pos);//we copy whatever is left after the last match
}


void translation(chunk *c, const char *source, const char *dest){//for each character, swap it with the character
    size_t dest_len = strlen(dest);
    for (size_t i = 0; i <c->in_len; i++) {
        char ch = c->in[i];
        const char *found = strchr(source, ch);//checks if current character exists in the string
        if (found != NULL) {
            size_t idx = found-source; //now we have to figure out which matching position it has, for example if source abc, char is b so index is 1 so replace with dest[1]
            if (idx < dest_len)
                ch = dest[idx];//swap with the matching character in dest
        }
        buf_append(c, &ch, 1);
    }
}

chunk   q_buf[Q_CAP];//shared queue for producer consumer 
int q_head =0; //where next chunk is removed
int q_tail =0; //where next chunk is inserted
int q_size =0; 
int q_done =0;//set it to 1 when producer is done adding chunks
pthread_mutex_t q_mu   = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  q_full   = PTHREAD_COND_INITIALIZER;//signal when queue has space
pthread_cond_t  q_empty  =PTHREAD_COND_INITIALIZER;//signal when queue has something 
 
void enq(chunk c) {//producer adds chunk into queue
    pthread_mutex_lock(&q_mu);//lock queue so thread changes it 
    while (q_size == Q_CAP)
        pthread_cond_wait(&q_full, &q_mu);//if queue is full, wait until worker removes something/consume
    q_buf[q_tail] = c;
    q_tail = (q_tail + 1) % Q_CAP;
    q_size++;
    pthread_cond_signal(&q_empty);//wake up a waiting worker
    pthread_mutex_unlock(&q_mu);
}
 
int deq(chunk *out) {//workers call this one to grab next chunk 
    pthread_mutex_lock(&q_mu);
    while (q_size == 0 && !q_done)
        pthread_cond_wait(&q_empty, &q_mu);//wait for producer to add something
    if (q_size == 0){ 
        pthread_mutex_unlock(&q_mu); 
        return -1; 
    }
    *out = q_buf[q_head];
    q_head = (q_head + 1)% Q_CAP;
    q_size--;
    pthread_cond_signal(&q_full);//we tell theres space now
    pthread_mutex_unlock(&q_mu);
    return 0;
}
 
void seal_q() {//tells everyone that producer is finished
    pthread_mutex_lock(&q_mu);
    q_done = 1;
    pthread_cond_broadcast(&q_empty);//broadcast, wake everybody up
    pthread_mutex_unlock(&q_mu);
}chunk *done_chunks;//stores completed chunks
int nchunks = 0;
pthread_mutex_t done_mu = PTHREAD_MUTEX_INITIALIZER;

typedef struct {//info passed to each thread
    int   tid;
    int   mode;    //0 = substitution, 1 = translation   
    char *val1;
    char *val2;
} worker_arg;

void *run_worker(void *vp) {//keeps pulling chunks from the queue and process them until theres nothing left 
    worker_arg *arg = (worker_arg *)vp;
    chunk c;
 
    while (deq(&c) == 0){//keep processing until queue is empty and sealed
        c.out = malloc(c.in_len);
        c.out_len= 0;
        c.out_cap =c.in_len;
 
        if (arg->mode == 0)
            substitution(&c, arg->val1, arg->val2);
        else
            translation(&c, arg->val1, arg->val2);
 
        munmap(c.in, c.in_len);//release mapped memory after processing
 
        pthread_mutex_lock(&done_mu);
        done_chunks[c.idx]= c;//only one thread can do this/save result in the correct slot
        pthread_mutex_unlock(&done_mu);
    }
    return NULL;
}


int main(int argc, char *argv[]) {
    long CHUNK_SZ = DEFAULT_CHUNK_SZ;
    char mode[20] = "substitution";
    int opt;
    int count = 0;
 
    while ((opt = getopt(argc, argv, "m:s:h-:")) != -1){
        switch (opt) {
            case 'm':
                strncpy(mode, optarg, sizeof(mode) - 1);
                mode[sizeof(mode) -1] = '\0';
                count++;
                break;
            case '-':
                if (strcmp(optarg, "help")== 0) { 
                    print_help(); 
                    return 0; 
                }
                else{ 
                    printf("psed: [option] [val1] [val2] [file]\n"); 
                    return 1; 
                }
            case '?':
                printf("psed: [option] [val1] [val2] [file]\n"); 
                return 1;
            
            case 's':
                CHUNK_SZ = atol(optarg);
                if (CHUNK_SZ <= 0 ){
                    printf("psed: invalid chunk size\n");
                    return 1;
                    
                }
                break;
            case 'h':
                print_help(); 
                return 0;
        }
    }
 
    if (count >1) { 
        printf("psed: [option] [val1] [val2] [file]\n"); 
        return 1; 
    }
 
    int remaining = argc - optind;
    if (remaining!= 3) { 
        printf("psed: [option] [val1] [val2] [file]\n"); 
        return 1; 
    }
 
    const char *val1 = argv[optind];
    const char *val2 = argv[optind + 1];
    const char *filename = argv[optind + 2];
 
    if (strcmp(mode, "substitution") != 0 && strcmp(mode, "translation") != 0) {
        printf("psed: unknown mode detected\n");
        printf("mode must be \"substitution\" or \"translation\"\n");
        return 1;
    }
 
    if (strcmp(mode, "translation") == 0 && strlen(val1) != strlen(val2)) {
        printf("psed: Translation strings not equal length\n");
        return 1;
    }
 
    int fd = open(filename, O_RDONLY);//now open input file for reading mode
    if (fd < 0){ 
        printf("psed: cannot open file\n"); 
        return 1; 
    }
 
    struct stat sb;
    fstat(fd, &sb);//file size so we know how many chunks to create
    off_t fsz = sb.st_size;
 
    int nthreads = num_cpus();
    if (nthreads< 1)       
        nthreads = 1;
    if (nthreads >MAX_THR) 
        nthreads = MAX_THR;
 
    //figure out max chunks so we can allocate done_chunks
    int max_chunks = (int)(fsz /CHUNK_SZ) +2;
    done_chunks = calloc(max_chunks, sizeof(chunk));
 
    pthread_t  threads[MAX_THR];
    worker_arg args[MAX_THR];
    int  modenum = (strcmp(mode, "translation") == 0) ? 1 : 0;
 
    for (int i = 0; i < nthreads; i++) {
        args[i].tid  = i;
        args[i].mode = modenum;
        args[i].val1 = (char *)val1;
        args[i].val2 = (char *)val2;
        pthread_create(&threads[i], NULL, run_worker, &args[i]);
    }
 
    //producer, slice the file and push chunks
    long page  =sysconf(_SC_PAGE_SIZE);
    long csize = (CHUNK_SZ / page) * page;
    if (csize <= 0) csize = page;
 
    for (off_t off= 0; off <fsz; off += csize) {//split file into chunks and send each to queue
        size_t mlen = (size_t)((fsz -off< csize) ? (fsz - off) : csize);
        char *ptr= mmap(NULL, mlen, PROT_READ, MAP_PRIVATE, fd, off);//map this chunk directly into memory
        if (ptr ==MAP_FAILED){ 
            perror("mmap"); 
            return 1; 
        }
        madvise(ptr, mlen, MADV_SEQUENTIAL);//we will read it sequentially
 
        chunk c = {0};
        c.in =ptr;
        c.in_len  =mlen;
        c.idx= nchunks++;
        enq(c);
    }
 
    close(fd);
    seal_q();
 
    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);//wait for all threads to finish
 
    for (int i = 0; i < nchunks; i++) {
        fwrite(done_chunks[i].out, 1, done_chunks[i].out_len, stdout);//print processed chunks in order
        free(done_chunks[i].out);
    }
 
    free(done_chunks);
    return 0;
}//getting the producer consumer pattern right was the most challenging,  as well as when to call the cond wait and broadcast, and of course i learned how to use mmap  