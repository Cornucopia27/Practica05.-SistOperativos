#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

// Macro que incluye el código de la instrucción máquina xchg
#define atomic_xchg(A,B)    __asm__ __volatile__( \
                                        " lock xchg %1,%0 ;\n"  \
                                        : "=ir" (A)             \
                                        : "m" (B), "ir" (A)     \
                                        );

#define CICLOS 10
#define MAXQUEUE 10

typedef struct _QUEUE {
	int elements[MAXQUEUE];
        int n_elements;
} QUEUE;

typedef struct semaphore_t {
  int counter;
  QUEUE queue;
} SEMAPHORE;

char *pais[3]={"Peru","Bolvia","Colombia"};
int *g;
int shmid;

SEMAPHORE *sem;

void initqueue(QUEUE* queue)
{
  memset(queue,0,MAXQUEUE);
}

void push(QUEUE* queue, int val)
{
  if(queue->n_elements<MAXQUEUE){
	queue->n_elements++;
	queue->elements[queue->n_elements]=val;
  }
}

int pop(QUEUE* queue)
{
  int Qvalue=0;
  Qvalue = queue->elements[0];
  for(int i=0;i<(MAXQUEUE-2);i++)
	queue->elements[i]=queue->elements[i+1];
  queue->n_elements--;
  queue->elements[MAXQUEUE-1]=0;
  return(Qvalue);
}

void initsem(SEMAPHORE* sem, int value)
{
  sem->counter = value;
  initqueue(&sem->queue);
}

void wait_sem(SEMAPHORE* sem)
{
  int l = 1;
  do { atomic_xchg(l,*g); } while(l!=0);
  if(sem->counter > 0)
  {
    sem->counter --;
  }
  else
  {
    sem->counter--;
    push(&sem->queue, getpid());
    //kill(getpid(), SIGSTOP);
  }
  l=1;
  *g = 0;
}

void signal_sem(SEMAPHORE* sem){
  if(MAXQUEUE>sem->counter)
    {
    sem->counter ++;
    } 
  else{
    sem->counter++;
    pop(&sem->queue);
   }
}

void proceso(int i)
{
  int k;
  int l;
  for(k=0;k<CICLOS;k++)
  {
    wait_sem(sem);
   
    printf("Entra %s",pais[i]);
    fflush(stdout);
    sleep(rand()%3);
    printf("- %s Sale\n",pais[i]);
    
    signal_sem(sem);
    
    sleep(rand()%3);
  }
  exit(0); // Termina el proceso
}


int main()
{
  int pid;
  int status;
  int args[3];
  int i;
  void *thread_result;

  // Solicitar memoria compartida
  shmid=shmget(0x1234,sizeof(g),0666|IPC_CREAT);
  if(shmid==-1)
  {
    perror("Error en la memoria compartida\n");
    exit(1);
  }

  // Conectar la variable a la memoria compartida
  g=shmat(shmid,NULL,0);

  if(g==NULL)
  {
    perror("Error en el shmat\n");
    exit(2);
  }

  *g=0;

  srand(getpid());

  for(i=0;i<3;i++)
  {
  // Crea un nuevo proceso hijo que ejecuta la función proceso()
    pid=fork();
    if(pid==0)
      proceso(i);
  }
  for(i=0;i<3;i++)
    pid = wait(&status);

  // Eliminar la memoria compartida
  shmdt(g);
}
