#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_FILHOS 10

int main() {
    pid_t pids_criados[NUM_FILHOS];
    
    printf("--- Iniciando criacao de %d processos filhos ---\n", NUM_FILHOS);
    for (int i = 0; i < NUM_FILHOS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Erro no fork");
            exit(1);
        } else if (pid == 0) {
            for(volatile int j = 0; j < 1000000; j++); 
            exit(i); 
        } else {
            pids_criados[i] = pid;
        }
    }

    int status;
    for (int i = 0; i < NUM_FILHOS; i++) {
        pid_t pid_finalizado = wait(&status);
        if (WIFEXITED(status)) {
            int ordem_criacao = WEXITSTATUS(status);
            printf("Processo coletado! PID: %d | Ordem de criacao original: %d\n", 
                   pid_finalizado, ordem_criacao);
        }
    }
    return 0;
}