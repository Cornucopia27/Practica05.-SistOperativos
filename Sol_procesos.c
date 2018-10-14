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
	int head;
	int tail;
  int n_elements;
} QUEUE;

typedef struct semaphore_t
{
  int counter;
  //unsigned int blocked;
  QUEUE queue;
} SEMAPHORE;

char *pais[3]={"Peru","Bolivia","Colombia"};
int *g;
int *f;

SEMAPHORE *sem;

void initqueue(QUEUE* queue)
{
  memset(queue,0,MAXQUEUE);
}

void push(QUEUE* queue, int val)
{
  queue->elements[queue->tail]=val;
  queue->tail++;
  queue->tail = queue->tail%MAXQUEUE;
  queue->n_elements++;
}
int pop(QUEUE* queue)
{
  int value;
  value = queue->elements[queue->head];
  queue->head++;
  queue->head = queue->head%MAXQUEUE;
  queue->n_elements--;
  return(value);
}

void initsem(SEMAPHORE* sem, int value)
{
  sem->counter = value;
  initqueue(&sem->queue);
}

void wait_sem(SEMAPHORE* sem)
{
  int l = 1;
  do { atomic_xchg(l,*g); } while(l!=0); // Puede pasar si otro proceso no esta en esta función
  if(sem->counter > 0) // Si puede continuar, pasa.
  {
    sem->counter--;
  }
  else // Si esta ocupado se bloquea
  {
    sem->counter--;
    push(&sem->queue, getpid());
    kill(getpid(), SIGSTOP);
  }
  *g=0; // Otro proceso puede entrar a la sección crítica
}

void signal_sem(SEMAPHORE* sem)
{
  int l = 1;
  do { atomic_xchg(l,*f); } while(l!=0); // Puede pasar si otro proceso no esta en esta función
  if((&sem->queue.n_elements) == 0) // Si no hay procesos en espera, se suma al contador
  {
    sem->counter++;
  }
  else if(sem->counter <= 0) // Se desbloquea el proximo proceso en la cola
  {
    int p_id;
    sem->counter++;
    p_id = pop(&sem->queue);
    kill(p_id, SIGCONT);
  }
  *f=0;
}

void proceso(SEMAPHORE* sem,int i)
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
  int shmid_g, shmid_f, shmid_sem;

  // Solicitar memoria compartida
  shmid_g=shmget(0x1234,sizeof(g),0666|IPC_CREAT);
  shmid_f=shmget(0x2345,sizeof(g),0666|IPC_CREAT);
  shmid_sem=shmget(0x3456,sizeof(g),0666|IPC_CREAT);
  if(shmid_sem == -1 || shmid_g == -1 || shmid_f == -1)
  {
    perror("Error en la memoria compartida\n");
    exit(1);
  }

  // Conectar la variable a la memoria compartida
  g = shmat(shmid_g,NULL,0);
  f = shmat(shmid_f,NULL,0);
  sem = shmat(shmid_sem,NULL,0);

  if(sem == NULL || g == NULL || f == NULL)
  {
    perror("Error en el shmat\n");
    exit(2);
  }

  initsem(sem,1);
  *g = 0;
  *f = 0;

  srand(getpid());

  for(i=0;i<3;i++)
  {
  // Crea un nuevo proceso hijo que ejecuta la función proceso()
    pid=fork();
    if(pid==0)
      proceso(sem,i);
  }
  for(i=0;i<3;i++)
    pid = wait(&status);

  // Eliminar la memoria compartida
  shmdt(g);
  shmdt(f);
  shmdt(sem);
}
