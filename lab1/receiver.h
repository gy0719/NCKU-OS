#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <time.h>
#include <mqueue.h>

typedef struct {
    int flag;      // 1 for message passing, 2 for shared memory
    union {
        mqd_t mqd;  
        char* shm_addr;
    } storage;
} mailbox_t;



typedef struct {
    char text[100];
} message_t;

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr);