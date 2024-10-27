#include "sender.h"

void send(message_t message, mailbox_t* mailbox_ptr) {
    if (mailbox_ptr->flag == 1) { // Message passing
        // Send message using POSIX message queues
        mq_send(mailbox_ptr->storage.mqd, message.text, sizeof(message.text), 0);
    } else if (mailbox_ptr->flag == 2) { // Shared memory
        // Send message using shared memory
        strcpy(mailbox_ptr->storage.shm_addr, message.text);
    }
}

int main(int argc, char* argv[]) {
    mailbox_t mailbox;
    message_t message;
    struct timespec start_time, end_time;
    double total_time = 0.0;
    mailbox.flag = atoi(argv[1]);

    sem_t* sem_sender = sem_open("sem_sender", O_CREAT, 0666, 1);
    sem_t* sem_receiver = sem_open("sem_receiver", O_CREAT, 0666, 0);

    // Create mailbox or shared memory based on mechanism
    if (mailbox.flag == 1) {
        printf("\033[36mMessage Passing\n\033[0m");
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 10;
        attr.mq_msgsize = 256;
        attr.mq_curmsgs = 0;
        mailbox.storage.mqd = mq_open("/mymq", O_CREAT | O_WRONLY, 0666, &attr);
        if (mailbox.storage.mqd == (mqd_t) -1) {
            perror("mq_open failed");
            return 1;
        }
    } else if (mailbox.flag == 2) {
        printf("\033[36mShared Memory\n\033[0m");
        key_t key = ftok("input.txt", 14);
        mailbox.storage.mqd = shmget(key, sizeof(message_t), 0666 | IPC_CREAT);
        if (mailbox.storage.mqd == -1) {
            perror("shmget failed");
            return 1;
        }
        mailbox.storage.shm_addr = shmat(mailbox.storage.mqd, NULL, 0);
    }

    char* input_file = argv[2];
    // Open input file
    FILE* fp = fopen(input_file, "r");
    if (fp == NULL) {
        perror("fopen");
        return 1;
    }

    // Read messages from input file and send them
    while (fgets(message.text, sizeof(message.text), fp) != NULL) {
        sem_wait(sem_sender);
        clock_gettime(CLOCK_MONOTONIC, &start_time);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end_time);
        total_time += (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_nsec - start_time.tv_nsec) * 1e-9;
        printf("\033[36mSending message: \033[0m%s", message.text);
        sem_post(sem_receiver);
    }

    // Send exit message
    printf("\033[31m\nEnd of input file! exit!\033[0m");
    strcpy(message.text, "EXIT");
    sem_wait(sem_sender);
    send(message, &mailbox);
    sem_post(sem_receiver);

    printf("\nTotal time taken in sending msg: %.6f s\n", total_time);

    // Close input file and destroy mailbox or shared memory
    if (mailbox.flag == 1) {
        mq_close(mailbox.storage.mqd);// Close the message queue
        mq_unlink("/my_mq");// Remove the message queue
    }else if (mailbox.flag == 2) { // Shared memory
        shmdt(mailbox.storage.shm_addr); // Detach the shared memory segment
        shmctl(mailbox.storage.mqd, IPC_RMID, NULL); // Remove the shared memory segment
    }
    fclose(fp);
    sem_close(sem_sender);
    sem_close(sem_receiver);
    sem_unlink("sem_sender");
    sem_unlink("sem_receiver");

    return 0;
}