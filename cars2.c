#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

/*
 * 主要函数声明，具体信息在后文阐述
 */
void* detectDeadlockThread(void* arg);
void* exitThread(void* arg);
void* northThread(void* seq);
void* southThread(void* seq);
void* westThread(void* seq);
void* eastThread(void* seq);

/*
 * 存储汽车编号使用的队列数据结构，节点存int型的编号，
 * 为双向链表
 */
typedef struct node_struct node; /*链表节点*/
struct node_struct{
    int n;
    node* next;
    node* last;
};
typedef struct seqQueue_struct seqQueue; /*链表结构*/
struct seqQueue_struct{
    node* head;
    node* tail;
};

/*
 *  队列的实现，包括4个方法：
 *  initQueue()创建一个空的没有节点的队列，返回队列地址；
 *  push()往队列s头部加入n；
 *  pup()返回最后一个节点的n，并删除最后一个节点；
 *  isEmpty()判断队列是否为空
 */
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

/*
 * 变量声明及初始化
 */
pthread_t north_t, south_t, west_t, east_t, dead_t, empty_t;
pthread_cond_t north_cond = PTHREAD_COND_INITIALIZER,
               south_cond = PTHREAD_COND_INITIALIZER,
               west_cond = PTHREAD_COND_INITIALIZER,
               east_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t car_lock = PTHREAD_MUTEX_INITIALIZER;
    /*这4个为该方向是否有汽车等待的标志*/
int north_car_flag = 0,
    south_car_flag = 0,
    west_car_flag = 0,
    east_car_flag = 0,
    /*这4个为该方向是否有汽车全部走完的标志*/
    north_empty_flag = 0,
    south_empty_flag = 0,
    west_empty_flag = 0,
    east_empty_flag = 0,
    /*这4个为该方向是否有汽车是否可以通行的标志*/
    north_go = 0,
    south_go = 0,
    west_go = 0,
    east_go = 0;

/*
 * main()函数，主要过程是先根据命令行参数提取出每个方向汽车编号的
 * 队列，然后创建4个线程，将对应的队列传入线程，让其调度该方向汽
 * 车的行动；剩下的两个线程中detectDeadlockThread是检测死锁的线程，
 * exitThread是最后检测退出时机并终止其他线程的线程，辅助程
 * 序退出
 */
int main(int argc, char* argv[]){
    if(argc != 2){ /*确保有汽车序列参数*/
        printf("Usage: %s <sequence>\n", argv[0]);
        return 1;
    }

    seqQueue *north_s = initQueue(), /*创建并初始化四个序号队列*/
             *south_s = initQueue(),
             *west_s = initQueue(),
             *east_s = initQueue();

    char* s = argv[1];
    int i = 0;
    char c = s[i];

    while(c != '\0'){ /*解析序号参数，将序号填入对应的队列*/
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

    /*创建4个序列处理线程、死锁检测线程以及辅助退出线程*/
    pthread_create(&north_t, NULL, northThread, (void*)north_s);
    pthread_create(&south_t, NULL, southThread, (void*)south_s);
    pthread_create(&east_t, NULL, eastThread, (void*)east_s);
    pthread_create(&west_t, NULL, westThread, (void*)west_s);
    pthread_create(&dead_t, NULL, detectDeadlockThread, NULL);
    pthread_create(&empty_t, NULL, exitThread, NULL);

    /*join上述6个线程，确保不会因某个线程先完成而使得其他线程退出*/
    pthread_join(north_t, NULL);
    pthread_join(south_t, NULL);
    pthread_join(east_t, NULL);
    pthread_join(west_t, NULL);
    pthread_join(dead_t, NULL);
    pthread_join(empty_t, NULL);

    return 0;
}

/*
 * 死锁检测线程的实现。设置4个标志变量*_car_flag，初始为0，当某个方向有汽车wait时该方向
 * 的flag变为1.因此死锁检测线程在一个死循环里判断四个flag是否均为1，若是则判定此时为
 * 死锁状态，需要给北方等待的汽车发信号让其不再等待。整个过程中涉及到的flag和go变量需要
 * 与车辆线程互斥，故放在car_lock锁中。
 * 因为如果没有出现死锁，则这个线程会一直运行在while循环中，所以需要用exitThread辅助退出
 */
void* detectDeadlockThread(void* arg){
    /*这两行将这个线程的状态设置为允许被cancel，并且收到cancel信号立即退出，从而使exitThread
      能正常取消它。下面的四个方向线程与此原理相同，不再赘述。*/
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

/*
 * 辅助退出线程。与detectDeadThread类似，某个方向没有汽车时flag变为1，四个方向均为1时说明所
 * 所有汽车全部通过了路口，但此时detectDeadThread可能死循环，而四个方向线程肯定死循环，所有
 * exitThread向这5个线程发送cancel信号，让它们强制退出。
 */
void* exitThread(void* arg){
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

/*
 * 以下为4个方向的车辆序列处理线程，因为4个函数结构完全一样，故只说明第一个。
 * 线程收到的参数为该方向车辆的编号组成的队列，先判断队列是否为空，若不为空，则处理最前面的
 * 那辆车：让这辆车到达路口，提示arrive，进入临界区，开始等待，直到它收到可以前进的信号，然
 * 后才通过路口，提示leave，这时它提醒它左边的车可以前进，退出临界区。接下来开始创建下一辆车
 * 的线程：递归调用自己，参数改为pop前一辆车之后的队列，这样相当于每一辆车都有一个处理线程。
 * 若队列为空，则该线程进入死循环，它的唯一任务是给左边的汽车发送可以通过的信号。
 */
void* northThread(void* seq){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    seqQueue* carSeq = (seqQueue*)seq;
    if(isEmpty(carSeq) != 1){
        int car = pop(carSeq);
        printf("car %d from North arrives at crossing\n", car);

        pthread_mutex_lock(&car_lock);
        north_car_flag = 1;
        while(north_go == 0) /*让wait处于死循环中，防止其被意外唤醒*/
            pthread_cond_wait(&north_cond, &car_lock);
        printf("car %d from North leaving crossing\n", car);
        north_go = 0;
        pthread_cond_signal(&east_cond);
        east_go = 1;
        pthread_mutex_unlock(&car_lock);

        pthread_create(&north_t, NULL, northThread, (void*)carSeq); /*递归创建下一辆车的处理线程*/
        pthread_join(north_t, NULL);
    }
    else{
        while(1){ /*这个方向没有车辆时便只负责给左边汽车发送通行信号，这种情况也算是递归出口*/
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
