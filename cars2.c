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
int isEmpty(seqQueue* s){
    if(s->head->next == s->tail)
        return 1;
    else
        return 0;
}

pthread_t north_t, south_t, west_t, east_t, dead_t, empty_t;
pthread_cond_t north_cond = PTHREAD_COND_INITIALIZER,
               south_cond = PTHREAD_COND_INITIALIZER,
               west_cond = PTHREAD_COND_INITIALIZER,
               east_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t car_lock = PTHREAD_MUTEX_INITIALIZER;
int north_car_flag = 0,
    south_car_flag = 0,
    west_car_flag = 0,
    east_car_flag = 0,
    north_empty_flag = 0,
    south_empty_flag = 0,
    west_empty_flag = 0,
    east_empty_flag = 0,
    north_go = 0,
    south_go = 0,
    west_go = 0,
    east_go = 0;

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

    while(1){
        pthread_mutex_lock(&car_lock);
        if(north_car_flag && south_car_flag && west_car_flag && east_car_flag == 1){
            printf("DEADLOCK: car jam detected, signalling North to go\n");
            pthread_cond_signal(&north_cond);
            north_go = 1;
            pthread_mutex_unlock(&car_lock);
            break;
        }
        else
            pthread_mutex_unlock(&car_lock);
    }

    return arg;
}
void* detectEmptyThread(void* arg){
    while(1){
        if(north_empty_flag && south_empty_flag && west_empty_flag && east_empty_flag == 1){
            pthread_cancel(dead_t);
            pthread_cancel(north_t);
            pthread_cancel(south_t);
            pthread_cancel(west_t);
            pthread_cancel(east_t);
            break;
        }
    }

    return arg;
}

void* northThread(void* seq){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    if(isEmpty(carSeq) != 1){
        int car = pop(carSeq);
        printf("car %d from North arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        north_car_flag = 1;
        while(north_go == 0)
            pthread_cond_wait(&north_cond, &car_lock);
        printf("car %d from North leaving crossing\n", car);
        north_go = 0;
        pthread_cond_signal(&east_cond);
        east_go = 1;
        pthread_mutex_unlock(&car_lock);

        pthread_create(&north_t, NULL, northThread, (void*)carSeq);
        pthread_join(north_t, NULL);
    }
    else{
        while(1){
            pthread_mutex_lock(&car_lock);
            north_empty_flag = 1;
            pthread_cond_signal(&east_cond);
            east_go = 1;
            pthread_mutex_unlock(&car_lock);
        }
    }

    return NULL;
}
void* southThread(void* seq){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    if(isEmpty(carSeq) != 1){
        int car = pop(carSeq);
        printf("car %d from South arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        south_car_flag = 1;
        while(south_go == 0)
            pthread_cond_wait(&south_cond, &car_lock);
        printf("car %d from South leaving crossing\n", car);
        south_go = 0;
        pthread_cond_signal(&west_cond);
        west_go = 1;
        pthread_mutex_unlock(&car_lock);

        pthread_create(&south_t, NULL, southThread, (void*)carSeq);
        pthread_join(south_t, NULL);
    }
    else{
        while(1){
            pthread_mutex_lock(&car_lock);
            south_empty_flag = 1;
            pthread_cond_signal(&west_cond);
            west_go = 1;
            pthread_mutex_unlock(&car_lock);
        }
    }

    return NULL;
}
void* westThread(void* seq){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    if(isEmpty(carSeq) != 1){
        int car = pop(carSeq);
        printf("car %d from West arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        west_car_flag = 1;
        while(west_go == 0)
            pthread_cond_wait(&west_cond, &car_lock);
        printf("car %d from West leaving crossing\n", car);
        west_go = 0;
        pthread_cond_signal(&north_cond);
        north_go = 1;
        pthread_mutex_unlock(&car_lock);

        pthread_create(&west_t, NULL, westThread, (void*)carSeq);
        pthread_join(west_t, NULL);
    }

    else{
        while(1){
            pthread_mutex_lock(&car_lock);
            west_empty_flag = 1;
            pthread_cond_signal(&north_cond);
            north_go = 1;
            pthread_mutex_unlock(&car_lock);
        }
    }

    return NULL;
}
void* eastThread(void* seq){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    if(isEmpty(carSeq) != 1){
        int car = pop(carSeq);
        printf("car %d from East arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        east_car_flag = 1;
        while(east_go == 0)
            pthread_cond_wait(&east_cond, &car_lock);
        printf("car %d from East leaving crossing\n", car);
        east_go = 0;
        pthread_cond_signal(&south_cond);
        south_go = 1;
        pthread_mutex_unlock(&car_lock);

        pthread_create(&east_t, NULL, eastThread, (void*)carSeq);
        pthread_join(east_t, NULL);
    }

    else{
        while(1){
            pthread_mutex_lock(&car_lock);
            east_empty_flag = 1;
            pthread_cond_signal(&south_cond);
            south_go = 1;
            pthread_mutex_unlock(&car_lock);
        }
    }

    return NULL;
}
