#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define PIPE_NAME "/tmp/TASK_PIPE"

int isNumber(char number[]);

int main(int argc, char *argv[]){
  if (argc != 5) {
    printf(" ./mobile_node {nº pedidos a gerar} {intervalo entre pedidos em ms} {milhares de instruções de cada pedido} {tempo máximo para execução}");
    exit(0);
  }
  if(isNumber(argv[1]) == 0) {
  	printf("{nº de pedidos a gerar} inválido.\n");
  	exit(0);
  }
  else if(isNumber(argv[2]) == 0) {
  	printf("{intervalo entre pedidos em ms} inválido.\n");
  	exit(0);
  }
  else if(isNumber(argv[3]) == 0) {
  	printf("{milhares de instruções de cada pedido} inválido.\n");
  	exit(0);
  }
  else if(isNumber(argv[4]) == 0) {
  	printf("{tempo máximo para execução} inválido.\n");
  	exit(0);
  }
  
  int fdPipe = open(PIPE_NAME, O_WRONLY);
  if (fdPipe < 0) {
  perror("Cannot open pipe for writing: ");
  exit(0);
 }
  int size = strlen(argv[3]) + strlen(argv[4]) + 3;
  
  char *tarefa = (char*) malloc((size)*sizeof(char) + sizeof(int));  
  for (int i = 0; i < atoi(argv[1]); i++){
  	sprintf(tarefa, "%d;%s;%s",i, argv[3], argv[4]);
  	write(fdPipe, &tarefa, strlen(tarefa));
  	printf("%s\n", tarefa);
  	tarefa[0] = 0;
  	usleep(atoi(argv[2]) * 1000);
  }
  
	return 1;

}
int isNumber(char number[]) {
    for (int i = 0; number[i] != 0; i++)
    {
        if (!isdigit(number[i]))
            return 0;
    }
    return 1;
}
