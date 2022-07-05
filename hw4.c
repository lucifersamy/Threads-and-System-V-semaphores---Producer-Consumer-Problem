#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/ipc.h>

void termination();

int C,N;
pthread_t* thread_id = NULL;
pthread_t t1Supplier;
int semId = -1;
char* inputFilePath=NULL;
FILE *fp = NULL;
int signalArrived=0;
pthread_attr_t attr;

void my_handler(){
    signalArrived++;
    //termination();
}

void printTimestamp(){
    time_t     now;
    struct tm  ts;
    char       buf[40];
    time(&now);
    ts = *localtime(&now);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    printf("%s\t",buf);
}

void createSemV(){

    union semun {
        int val; /*individual semaphore value*/
        struct semid_ds * buf ; /*semaphore data structure*/
        unsigned short* array; /*multiple semaphore values*/
        struct seminfo* __buf; /* linux specific */
    };

    if((semId = semget(IPC_PRIVATE, 2,  0777 | IPC_CREAT) ) == -1){
        perror("semget");
        exit(EXIT_FAILURE);
    }    
    union semun semun_g;
    unsigned short valuesArray[2];
  
    semun_g.array = valuesArray;
    valuesArray[0] = 0;
    valuesArray[1] = 0;


    if(semctl(semId, 0, SETALL, semun_g) == -1){
        perror("semctl");
        exit(EXIT_FAILURE);
    }  
}

static void *
supplierThreadFunc(void *arg)
{
    struct sembuf sop;

    int returnedValue0,returnedValue1, returnedValue;
    char *s = (char *) arg;

    char c;
    char currentValue;
    if ((fp = fopen (s, "r")) == NULL) {
         printf("fopen error!");
         exit(EXIT_FAILURE);
    }
    while (1)
    {
        if(signalArrived!=0){
            termination();
        }
        sop.sem_op = 1;
        sop.sem_flg = 0;
        
        //while(( (returnedValue0 =  semctl(semId, 0, GETVAL))  == -1) && (errno == EINTR))

        returnedValue0 = semctl(semId, 0, GETVAL);
        if(returnedValue0 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }    

        //while( ( (returnedValue1 =  semctl(semId, 0, GETVAL)) == -1) && (errno == EINTR))

        returnedValue1 = semctl(semId, 1, GETVAL);
        if(returnedValue1 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }    
        printTimestamp();
        printf("Supplier: read from input a '%c'. Current amounts: %d x '1', %d x '2'.\n", currentValue, returnedValue0, returnedValue1);
        if(signalArrived!=0){
            termination();
        }

        c = fgetc(fp);
        if(c == EOF){
            break;
        }
        if(c == '1'){
            currentValue = '1';
        }
        if(c == '2'){
            currentValue = '2';
        }

        if(c == '1'){
            sop.sem_num = 0;
        }
        if(c == '2'){
            sop.sem_num = 1;
        }


        //while(( (returnedValue = semop(semId, &sop, 1) ) == -1) && (errno == EINTR))
        returnedValue = semop(semId, &sop, 1);
        if(returnedValue == -1){
            perror("semop");
            exit(EXIT_FAILURE);
        }

        //while(( (returnedValue0 =  semctl(semId, 0, GETVAL)) == -1) && (errno == EINTR))

        returnedValue0 = semctl(semId, 0, GETVAL);
        if(returnedValue0 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }    

        //while(( (returnedValue1 =  semctl(semId, 0, GETVAL)) == -1) && (errno == EINTR))

        returnedValue1 = semctl(semId, 1, GETVAL);
        if(returnedValue1 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }
        
        printTimestamp();
        printf("Supplier: delivered a '%c'. Post-delivery amounts: %d x '1', %d x '2'.\n", currentValue, returnedValue0, returnedValue1);
        if(signalArrived!=0){
            termination();
        }
    }
    printTimestamp();
    printf("The supplier has left.\n");

    fclose (fp);

    pthread_exit(EXIT_SUCCESS);
}


static void *
consumerThreadFunc(void *arg)
{
    struct sembuf sop[2];
    int returnedValue0=0,returnedValue1=0, returnedValue=0;

    int i=0;
    while(i<N)
    {   
        if(signalArrived!=0){
            termination();
        }
        sop[0].sem_num = 0; 
        sop[1].sem_num = 1; 
        sop[0].sem_op = -1;
        sop[1].sem_op = -1;
        sop[0].sem_flg = 0;
        sop[1].sem_flg = 0;
        //while(( (returnedValue0 =  semctl(semId, 0, GETVAL)) == -1) && (errno == EINTR))
        returnedValue0 = semctl(semId, 0, GETVAL);
        if(returnedValue0 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }    

        //while(( (returnedValue1 =  semctl(semId, 1, GETVAL)) == -1) && (errno == EINTR))

        returnedValue1 = semctl(semId, 1, GETVAL);
        if(returnedValue1 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }
        printTimestamp();
        printf("Consumer-%d at iteration %d (waiting). Current amounts: %d x '1', %d x '2'.\n",*(int*)arg, i,returnedValue0,returnedValue1);
        if(signalArrived!=0){
            termination();
        }

        //while(( (returnedValue =  semop(semId, sop, 2)) == -1) && (errno == EINTR))

        returnedValue = semop(semId, sop, 2);
        if(returnedValue == -1){
            perror("semop");
            exit(EXIT_FAILURE);
        }

        //while(( (returnedValue0 =  semctl(semId, 0, GETVAL)) == -1) && (errno == EINTR))

        returnedValue0 = semctl(semId, 0, GETVAL);
        if(returnedValue0 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }    

        //while(( (returnedValue1 =  semctl(semId, 1, GETVAL)) == -1) && (errno == EINTR))
        returnedValue1 = semctl(semId, 1, GETVAL);
        if(returnedValue1 == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }
        printTimestamp();
        printf("Consumer-%d at iteration %d (consumed). Post-consumption amounts: %d x '1', %d x '2'.\n",*(int*)arg,i,returnedValue0,returnedValue1);
        ++i;
        if(signalArrived!=0){
            termination();
        }

    }
    printTimestamp();
    printf("Consumer-%d has left.\n", *(int*)arg);
    pthread_exit(EXIT_SUCCESS);
}

int main(int argc,char *argv[])
{
    int opt,control;
    control = setvbuf(stdout, NULL, _IONBF, 0);
    if(control == -1){
        perror("setvbuf");
        exit(EXIT_FAILURE);
    }

    while((opt = getopt(argc, argv, ":C:N:F:")) != -1)  
    {  
        switch(opt)  
        {  
            case 'F':
                inputFilePath = optarg;
                break;
            case 'C':
                C = atoi(optarg);
                break;
            case 'N':
                N = atoi(optarg);
                break;
            case ':':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s -C 10 -N 5 -F inputfilePath\n", argv[0]);
                exit(EXIT_FAILURE);     
                break;  
            case '?':  
                errno=EINVAL;
                fprintf(stderr, "Wrong format.\n" );
                fprintf(stderr, "Usage: %s  -C 10 -N 5 -F inputfilePath\n", argv[0]);
                exit(EXIT_FAILURE); 
                break; 
            case -1:
                break;
            default:
                abort (); 
        }
    }
    if(C<5 | N<2){
        fprintf(stderr, "Usage: %s  -C 10 -N 5 -F inputfilePath\n", argv[0]);
        fprintf(stderr, "Should be C>4 and N>1\n");
        exit(EXIT_FAILURE); 
    }

    if(optind!=7){
        errno=EINVAL;
        fprintf(stderr, "Wrong format.\n" );
        fprintf(stderr, "Usage: %s  -C 10 -N 5 -F inputfilePath\n", argv[0]);
        exit(EXIT_FAILURE); 
    }

    createSemV();

    struct sigaction newact;
    newact.sa_handler = &my_handler;
    newact.sa_flags = 0;

    if((sigemptyset(&newact.sa_mask) == -1) || (sigaction(SIGINT, &newact, NULL) == -1) ){
        perror("Failled to install SIGINT signal handler");
        exit(EXIT_FAILURE);
    }
    if(signalArrived!=0){
        termination();
    }

    
    int s;
    s = pthread_attr_init(&attr); 
    if (s != 0){
        fprintf(stderr, "pthread_attr_init\n");
        exit(EXIT_FAILURE); 
    }

    s = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (s != 0){
        fprintf(stderr, "pthread_attr_setdetachstate\n");
        exit(EXIT_FAILURE); 
    }
    
    s = pthread_create(&t1Supplier, &attr, supplierThreadFunc, inputFilePath);
    if (s != 0){
        fprintf(stderr, "pthread_create supplier\n");
        exit(EXIT_FAILURE); 
    }

    s = pthread_attr_destroy(&attr); 
    if (s != 0){
        fprintf(stderr, "pthread_attr_destroy\n");
        exit(EXIT_FAILURE); 
    }

    if(signalArrived!=0){
        termination();
    }

    thread_id = (pthread_t*) calloc(C, sizeof(pthread_t));
    int i;
    int threadNums[C];

    for(i=0; i<C; ++i){
        threadNums[i] = i;
        if(signalArrived!=0){
            termination();
        }
        s = pthread_create(&thread_id[i], NULL, consumerThreadFunc, &threadNums[i]);
        if (s != 0){
            fprintf(stderr, "pthread_create supplier\n");
            exit(EXIT_FAILURE); 
        }
    }

    for(i=0; i<C; ++i){
        pthread_join(thread_id[i],NULL);
    }

    if(signalArrived!=0){
        termination();
    }

    if(semctl(semId, 0 , IPC_RMID) == -1){
        perror("semctl");
        exit(EXIT_FAILURE);
    }  
    free(thread_id);
    exit(EXIT_SUCCESS);    

}

void termination(){
    int s;
    int i = 0;
    if(fp != NULL){
        if(fclose(fp) == -1){
            perror("fclose");
            exit(EXIT_FAILURE);
        }
    }
    if(semId != -1){
        if(semctl(semId, 0 , IPC_RMID) == -1){
            perror("semctl");
            exit(EXIT_FAILURE);
        }
    }

    s = pthread_attr_destroy(&attr); 
    if (s != 0){
        fprintf(stderr, "pthread_attr_destroy\n");
        exit(EXIT_FAILURE); 
    }

    for(i = 0; i < C; i++)
        pthread_cancel(thread_id[i]);

    for(i = 0; i < C; i++)
        pthread_join(thread_id[i], NULL);

    free(thread_id);
    pthread_cancel(t1Supplier);
    exit(EXIT_SUCCESS);
}