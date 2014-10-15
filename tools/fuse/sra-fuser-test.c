#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define MAX_THREADS 20
#define CHUNK_SZ 4200

int fin = 0;
int fout = 0;
off_t file_sz;
off_t step;

typedef struct thread_data_struct {
    int id;
} thread_data;

void* thread_func(void *v)
{
    char buf[CHUNK_SZ];
    thread_data* data = v;
    off_t pos = (data->id - 1) * CHUNK_SZ;

    printf("Thread %i started\n", data->id);

    while( pos < file_sz ) {
        ssize_t i = pread(fin, buf, CHUNK_SZ, pos);
        if( i < 0 ) {
            printf("Thread %i %lu:%lu: %s\n", data->id, pos, pos + CHUNK_SZ - 1, strerror(errno));
            break;
        }
        pwrite(fout, buf, i, pos);
        pos += step;
    }
    printf("Thread %i finished\n", data->id);
    return v;
}


int main (int argc, char* argv[])
{
    pthread_t threads[MAX_THREADS];
    thread_data threads_data[MAX_THREADS];
    int i, threads_qty = 0;
    struct stat stat;

    if( argc < 4 ) {
        printf("\nUsage:\n\ttest num_threads input_file output_file\n\n");
        return 1;
    }
    threads_qty = atoi(argv[1]);
    if( threads_qty < 2 || threads_qty > MAX_THREADS ) {
        printf("num_threads out of range: [2:%i]\n", MAX_THREADS);
        return 2;
    }
    if( (fin = open(argv[2], O_RDONLY, 0)) < 0) {
        printf("Cannot read file '%s': %s\n", argv[2], strerror(errno));
        return 3;
    }
    if( fstat(fin, &stat) != 0 ) {
        printf("Cannot stat '%s': %s\n", argv[2], strerror(errno));
        return 3;
    }
    printf("File size: %lu bytes\n", stat.st_size);

    if( stat.st_size / threads_qty / CHUNK_SZ < 10 ) {
        printf("Need bigger file or less threads to test \n");
        return 5;
    }
    if( (fout = creat(argv[3], 0)) < 0 ) {
        printf("Cannot write file '%s': %s\n", argv[3], strerror(errno));
        return 4;
    }
    file_sz = stat.st_size;
    step = threads_qty * CHUNK_SZ;

    for(i = 0; i < threads_qty; i++) {
        threads_data[i].id = i + 1;
        if( pthread_create(&threads[i], NULL, thread_func, &threads_data[i]) != 0 ) {
            printf("Cannot pthread_create %i: %s\n", i + 1, strerror(errno));
            return 14;
        }
    }

    for(i = 0; i < threads_qty; i++) {
        thread_data* data;
        if( pthread_join(threads[i], (void**)&data) != 0 ) {
            printf("Cannot pthread_join %i: %s\n", i + 1, strerror(errno));
        }
    }

    close(fin);
    close(fout);
    return 0;
}
