#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h> // include POSIX semaphores
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/msg.h>


typedef struct server_st{
  char nome_server[64];
  int primeiro_vCPU;
  int segundo_vCPU;
  //bool ocupado1;
  //bool ocupado2;
} server;

typedef struct {
    int num_filas;
    int tempo_max_espera;
    int num_edge_servers;
}config;

typedef struct{
  int total_tarefas;
  int tempo_medio;
  int tarefas_exec;
  int operacoes_manu;
  int failed;
}stats;

typedef struct {
  long priority;
  int msg_number;
} priority_msg;

sem_t *semlog;
sem_t *sem_shm;
sem_t *sem_stats;

config *shm_config;
server *shm_server;
stats  *shm_stats;

int shmid_config;
int shmid_stats;
int mqid;


time_t rawtime;
struct tm * timeinfo;

pthread_t* server_thread;
pthread_t scheduler;

pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

void MobileNode() {
  printf("por implementar");
}

void print_shared_mem(){
    printf("Num filas: %d\n",shm_config->num_filas);
    printf("Tempo max espera: %d\n",shm_config->tempo_max_espera);
    printf("Num edge servers: %d\n",shm_config->num_edge_servers);
    for(int i = 0 ; i < shm_config->num_edge_servers ; i++){
      printf("Nome do server %s primeiro vCpu %d segundo vCpu %d\n",shm_server[i].nome_server,shm_server[i].primeiro_vCPU,shm_server[i].segundo_vCPU);
    }
}


void geraOutput(char* mens){
    FILE *f;
    f = fopen("log.txt","a");
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );

    //sem_wait(semlog);

    fprintf(f,"%d:%d:%d ",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    fprintf(f,"%s\n",mens);

    //sem_post(semlog);

    printf("%d:%d:%d ",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    printf("%s\n",mens);


    fflush(stdout);

    fclose(f);

}


void clean_resources(){
    shmctl(shmid_config , IPC_RMID, NULL);
    msgctl(mqid,IPC_RMID,0);

     //fechar semaforo
    sem_close(sem_shm);
    sem_unlink("SEM_SHM");
    sem_close(semlog);
    sem_unlink("SEMLOG");

    unlink(PIPE_NAME);
    pthread_mutex_destroy(&server_mutex);
    geraOutput("SIMULATOR ENDING");

}

void inicializa(int size){
  if((shmid_config = shmget(IPC_PRIVATE,sizeof(config)+(sizeof(server)*size),IPC_CREAT|0700)) < 0){
		perror("ERRO ao criar a memoria partilhada\n");
		exit(1);
	}
  if((shm_config =(config*) shmat(shmid_config,NULL,0)) <(config*)1){
		perror("ERRO na memoria partilhada\n");
		exit(1);
	}

  if((shmid_stats = shmget(IPC_PRIVATE,sizeof(stats),IPC_CREAT|0700)) < 0){
		perror("ERRO ao criar a memoria partilhada\n");
		exit(1);
	}
  if((shm_stats =(stats*) shmat(shmid_stats,NULL,0)) <(stats*)1){
		perror("ERRO na memoria partilhada\n");
		exit(1);
	}

  sem_unlink("SEMLOG");
  if((semlog = sem_open("SEMLOG" , O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED){
    perror("ERRO ao criar o semaforo log\n");
    exit(1);
  }
  sem_unlink("SEM_SHM");
  if((sem_shm = sem_open("SEM_SHM" , O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED){
    perror("ERRO ao criar o semaforo da shm\n");
		exit(1);
  }

  sem_unlink("SEM_STATS");
  if((sem_stats = sem_open("SEM_STATS" , O_CREAT|O_EXCL, 0700, 1)) == SEM_FAILED){
    perror("ERRO ao criar o semaforo da shm\n");
		exit(1);
  }


  geraOutput("SHARED MEMORY, SEMAPHORES AND MUTEX CREATED");

}

config leConfig(char *filename){
    FILE* fich;
    config a;
    char aux[20];
    if((fich = fopen(filename , "r"))==NULL){
        perror("ERRO na abertuda do ficheiro config\n");
        exit(0) ;
    }

    fscanf(fich,"%d",&a.num_filas);
    fscanf(fich,"%d",&a.tempo_max_espera);
    fscanf(fich,"%d",&a.num_edge_servers);

    if(a.num_edge_servers < 2){
      printf("Erro na config edge server insuficiente\n");
      geraOutput("Erro na config edge server insuficiente");
      geraOutput("LEAVING\n");
      exit(0);
    }
    inicializa(a.num_edge_servers);

    sem_wait(sem_shm);

    shm_config->num_filas = a.num_filas;
    shm_config->tempo_max_espera = a.tempo_max_espera;
    shm_config->num_edge_servers = a.num_edge_servers;

    shm_server = (server*)(shm_config+1);


    for(int i = 0 ;i < shm_config->num_edge_servers;i++){
      fscanf(fich, "%s" ,aux);
      char *token = strtok(aux, ",");
      strcpy(shm_server[i].nome_server,token);
      token = strtok(NULL, ",");
      shm_server[i].primeiro_vCPU = atoi(token);
      token = strtok(NULL, ",");
      shm_server[i].segundo_vCPU = atoi(token);
    }

    sem_post(sem_shm);
    sem_wait(sem_stats);

    shm_stats->total_tarefas = 0;
    shm_stats->tempo_medio = 0;
    shm_stats->tarefas_exec = 0;
    shm_stats->operacoes_manu = 0;
    shm_stats->failed = 0;

    sem_post(sem_stats);

    fclose(fich);
    geraOutput("SHARED MEMORY FILLED");

    return a;
}

void sigint(int signum) { // handling of CTRL-C
    signal(SIGINT , SIG_IGN);
    printf("\n Ctrl C pressed.Cleaning resources...\n");
    geraOutput("SIGNAIL SIGINT DETECTED");
    geraOutput("WAITING FOR THE CLEAN");
    clean_resources();
	  geraOutput("SIMULATOR CLOSING\n");
	exit(0);
}

void sigtstp(int signum){
  sem_wait(sem_stats);
  printf("=== ESTATISTICAS ===\n");
  printf("Num total de tarefas executadas: %d\n",shm_stats->total_tarefas);
  printf("Tempo medio de resposta a cada tarefa: %d\n",shm_stats->tempo_medio);
  printf("Num de tarefas executadas em cada EDGE SERVER: %d\n",shm_stats->tarefas_exec);
  printf("Num de operacoes de manutencao de cada EDGE SERVER : %d\n",shm_stats->operacoes_manu);
  printf("Num de tarefas que não chegaram a ser executadas: %d\n",shm_stats->failed);

  sem_post(sem_stats);
  print_shared_mem();
}

void *vCpu(int* p){
  pthread_mutex_lock(&server_mutex);
  int cpu_id = *p;
  int value = cpu_id / 2;
  int choice = cpu_id % 2;
  //pthread_t id = pthread_self();

  printf("Thread %d\n",cpu_id);
  if(choice){
      printf("vCpu com valor %d do server %s\n",shm_server[value].segundo_vCPU,shm_server[value].nome_server);
  }
  else{
    printf("vCpu com valor %d do server %s\n",shm_server[value].primeiro_vCPU,shm_server[value].nome_server);
  }
  geraOutput("Criada a thread do EdgeServer ");
  pthread_mutex_unlock(&server_mutex);

  pthread_exit(0);
}

void EdgeServer(){
  sem_wait(sem_shm);
  server_thread =(pthread_t*)malloc(sizeof(pthread_t*)*shm_config->num_edge_servers*2);
  int index[shm_config->num_edge_servers*2];
  for(int i = 0 ; i < shm_config->num_edge_servers*2 ; i++){
      index[i] = i;
      if((pthread_create(&server_thread[i], NULL, (void*)vCpu, &index[i]))!=0){
        perror("ERRO ao criar as threads\n");
        exit(0);
      }
  }
  for(int i = 0 ; i < shm_config->num_edge_servers * 2 ; i++){
    pthread_join(server_thread[i],NULL);
  }
  sem_post(sem_shm);
}

void Monitor(){
  signal(SIGINT,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  printf("Criado o processo Monitor\n");
  geraOutput("PROCESS MONITOR CREATED");
}

void Maintence(){
  signal(SIGINT,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  printf("Criado o processo Maintence\n");
  geraOutput("PROCESS MAINTENENCE CREATED");
}

void scheduler_tasks(){
  printf("THREAD SCHEDULER\n");
  geraOutput("THREAD SCHEDULER");
}
void TaskManager(){
  signal(SIGINT,SIG_IGN);
  signal(SIGTSTP,sigtstp);

  geraOutput("PROCESS TASK MANAGER CREATED");
  scheduler =(pthread_t)malloc(sizeof(pthread_t));
  if((pthread_create(&scheduler, NULL, (void*)scheduler_tasks, NULL))!=0){
    perror("ERRO ao criar as threads\n");
    exit(0);
  }
  if ((fdPipe = open(PIPE_NAME, O_RDONLY)) < 0) {
    perror("Cannot open pipe for reading: ");
    exit(0);
  }
}

int main(int argc, char *argv[]){
  if (argc != 2) {
    perror("./offload_simulator {ficheiro de configuração}\n");
    exit(0);
  }
  unlink(PIPE_NAME);
  
  if ((mkfifo(PIPE_NAME, O_CREAT|O_EXCL|0600)<0) && (errno!= EEXIST)) {
    perror("Cannot create pipe: ");
    exit(0);
  }

  signal(SIGINT,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  geraOutput("OFFLOAD SIMULATOR STARTING");
  leConfig(argv[1]);
  //print_shared_mem();
  EdgeServer();

  priority_msg my_msg;
  mqid = msgget(IPC_PRIVATE,IPC_CREAT|0700);

  if(fork() == 0) TaskManager();
  if(fork() == 0) Monitor();
  if(fork() == 0) Maintence();

  signal(SIGINT , sigint);
  int teste = 1;
  while (teste) {
    printf("waiting...\n");
  	sleep(60);
  	teste++;
  }
  return 0;
}
