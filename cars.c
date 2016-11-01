#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void* detectDeadlockThread(void* arg);
void* detectEmptyThread(void* arg);
void* northThread(void* seq);
void* southThread(void* seq);
void* westThread(void* seq);
void* eastThread(void* seq);

typedef struct node_struct node;
struct node_struct{
    int n;
    node* next;
    node* last;
};
typedef struct seqQueue_struct seqQueue;
struct seqQueue_struct{
    node* head;
    node* tail;
};

seqQueue* initQueue(){
    seqQueue* s = (seqQueue*)malloc(sizeof(seqQueue));
    s->head = (node*)malloc(sizeof(node));
    s->tail = (node*)malloc(sizeof(node));
    s->head->last = NULL;
    s->head->next = s->tail;
    s->tail->last = s->head;
    s->tail->next = NULL;
    return s;
}
void push(seqQueue* s, int n){
    node* newNode = (node*)malloc(sizeof(node));
    newNode->n = n;
    newNode->last = s->head;
    newNode->next = s->head->next;
    s->head->next = newNode;
    newNode->next->last = newNode;
}
int pop(seqQueue* s){
    node* oldNode = s->tail->last;
    int res;
    if(oldNode == s->head)
        return -1;
    else{
        res = oldNode->n;
        oldNode->last->next = s->tail;
        oldNode->next->last = oldNode->last;
        free(oldNode);
        return res;
    }
}

pthread_t north_t, south_t, west_t, east_t, dead_t, empty_t;
pthread_cond_t north_cond = PTHREAD_COND_INITIALIZER,
               south_cond = PTHREAD_COND_INITIALIZER,
               west_cond = PTHREAD_COND_INITIALIZER,
               east_cond = PTHREAD_COND_INITIALIZER,
               north_dead_cond = PTHREAD_COND_INITIALIZER,
               south_dead_cond = PTHREAD_COND_INITIALIZER,
               west_dead_cond = PTHREAD_COND_INITIALIZER,
               east_dead_cond = PTHREAD_COND_INITIALIZER,
               north_empty_cond = PTHREAD_COND_INITIALIZER,
               south_empty_cond = PTHREAD_COND_INITIALIZER,
               west_empty_cond = PTHREAD_COND_INITIALIZER,
               east_empty_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t car_lock = PTHREAD_MUTEX_INITIALIZER,
                nocar_lock = PTHREAD_MUTEX_INITIALIZER,
                north_dead_lock = PTHREAD_MUTEX_INITIALIZER,
                south_dead_lock = PTHREAD_MUTEX_INITIALIZER,
                west_dead_lock = PTHREAD_MUTEX_INITIALIZER,
                east_dead_lock = PTHREAD_MUTEX_INITIALIZER,
                north_empty_lock = PTHREAD_MUTEX_INITIALIZER,
                south_empty_lock = PTHREAD_MUTEX_INITIALIZER,
                west_empty_lock = PTHREAD_MUTEX_INITIALIZER,
                east_empty_lock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char* argv[]){
    if(argc == 1){
        printf("Usage: %s <sequence>\n", argv[0]);
        return 1;
    }

    seqQueue *north_s = initQueue(),
             *south_s = initQueue(),
             *west_s = initQueue(),
             *east_s = initQueue();

    char* s = argv[1];
    int i = 0;
    char c = s[i];

    while(c != '\0'){
        switch(c){
            case 'n':
                push(north_s, i+1);
                break;
            case 's':
                push(south_s, i+1);
                break;
            case 'w':
                push(west_s, i+1);
                break;
            case 'e':
                push(east_s, i+1);
                break;
            default:
                printf("Unrecognized symbol '%c'\n", c);
                return 1;
        }
        c = s[++i];
    }

    pthread_create(&north_t, NULL, northThread, (void*)north_s);
    pthread_create(&south_t, NULL, southThread, (void*)south_s);
    pthread_create(&east_t, NULL, eastThread, (void*)east_s);
    pthread_create(&west_t, NULL, westThread, (void*)west_s);
    pthread_create(&dead_t, NULL, detectDeadlockThread, NULL);
    pthread_create(&empty_t, NULL, detectEmptyThread, NULL);

    pthread_join(north_t, NULL);
    pthread_join(south_t, NULL);
    pthread_join(east_t, NULL);
    pthread_join(west_t, NULL);
    pthread_join(dead_t, NULL);
    pthread_join(empty_t, NULL);

    return 0;
}

void* detectDeadlockThread(void* arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    pthread_mutex_lock(&north_dead_lock);
    pthread_mutex_lock(&south_dead_lock);
    pthread_mutex_lock(&west_dead_lock);
    pthread_mutex_lock(&east_dead_lock);

    pthread_cond_wait(&north_dead_cond, &north_dead_lock);
    pthread_cond_wait(&south_dead_cond, &south_dead_lock);
    pthread_cond_wait(&west_dead_cond, &west_dead_lock);
    pthread_cond_wait(&east_dead_cond, &east_dead_lock);
    printf("DEADLOCK: car jam detected, signalling North to go\n");
    pthread_cond_signal(&north_cond);

    pthread_mutex_unlock(&north_dead_lock);
    pthread_mutex_unlock(&south_dead_lock);
    pthread_mutex_unlock(&west_dead_lock);
    pthread_mutex_unlock(&east_dead_lock);

    return arg;
}
void* detectEmptyThread(void* arg){
    pthread_mutex_lock(&north_empty_lock);
    pthread_mutex_lock(&south_empty_lock);
    pthread_mutex_lock(&west_empty_lock);
    pthread_mutex_lock(&east_empty_lock);

    pthread_cond_wait(&north_empty_cond, &north_empty_lock);
    pthread_cond_wait(&south_empty_cond, &south_empty_lock);
    pthread_cond_wait(&west_empty_cond, &west_empty_lock);
    pthread_cond_wait(&east_empty_cond, &east_empty_lock);

    /*
    if(pthread_cancel(dead_t))
        printf("cancel dead failed\n");
    else printf("cancel dead succeed\n");
    if(pthread_cancel(north_t))
        printf("cancel north failed\n");
    else printf("cancel north succeed\n");
    if(pthread_cancel(south_t))
        printf("cancel south failed\n");
    else printf("cancel south succeed\n");
    if(pthread_cancel(west_t))
        printf("cancel west failed\n");
    else printf("cancel west succeed\n");
    if(pthread_cancel(east_t))
        printf("cancel east failed\n");
    else printf("cancel east succeed\n");
    */

    pthread_mutex_unlock(&north_empty_lock);
    pthread_mutex_unlock(&south_empty_lock);
    pthread_mutex_unlock(&west_empty_lock);
    pthread_mutex_unlock(&east_empty_lock);

    return arg;
}

void* northThread(void* seq){
    sleep(1);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    int car = pop(carSeq);
    while(car != -1){
        printf("car %d from North arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        pthread_cond_signal(&north_dead_cond);
        pthread_cond_wait(&north_cond, &car_lock);
        printf("car %d from North leaving crossing\n", car);
        pthread_cond_signal(&east_cond);
        pthread_mutex_unlock(&car_lock);

        car = pop(carSeq);
    }

    while(car == -1){
        pthread_mutex_lock(&nocar_lock);
        pthread_cond_signal(&north_empty_cond);
        pthread_cond_signal(&east_cond);
        pthread_mutex_unlock(&nocar_lock);
    }

    return NULL;
}
void* southThread(void* seq){
    sleep(2);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    int car = pop(carSeq);
    while(car != -1){
        printf("car %d from South arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        pthread_cond_signal(&south_dead_cond);
        pthread_cond_wait(&south_cond, &car_lock);
        printf("car %d from South leaving crossing\n", car);
        pthread_cond_signal(&west_cond);
        pthread_mutex_unlock(&car_lock);

        car = pop(carSeq);
    }

    while(car == -1){
        pthread_mutex_lock(&nocar_lock);
        pthread_cond_signal(&south_empty_cond);
        pthread_cond_signal(&west_cond);
        pthread_mutex_unlock(&nocar_lock);
    }

    return NULL;
}
void* westThread(void* seq){
    sleep(3);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    int car = pop(carSeq);
    while(car != -1){
        printf("car %d from West arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        pthread_cond_signal(&west_dead_cond);
        pthread_cond_wait(&west_cond, &car_lock);
        printf("car %d from West leaving crossing\n", car);
        pthread_cond_signal(&north_cond);
        pthread_mutex_unlock(&car_lock);

        car = pop(carSeq);
    }

    while(car == -1){
        pthread_mutex_lock(&nocar_lock);
        pthread_cond_signal(&west_empty_cond);
        pthread_cond_signal(&north_cond);
        pthread_mutex_unlock(&nocar_lock);
    }

    return NULL;
}
void* eastThread(void* seq){
    sleep(4);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    int car = pop(carSeq);
    while(car != -1){
        printf("car %d from East arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        pthread_cond_signal(&east_dead_cond);
        pthread_cond_wait(&east_cond, &car_lock);
        printf("car %d from East leaving crossing\n", car);
        pthread_cond_signal(&south_cond);
        pthread_mutex_unlock(&car_lock);

        car = pop(carSeq);
    }

    while(car == -1){
        pthread_mutex_lock(&nocar_lock);
        pthread_cond_signal(&east_empty_cond);
        pthread_cond_signal(&south_cond);
        pthread_mutex_unlock(&nocar_lock);
    }

    return NULL;
}
