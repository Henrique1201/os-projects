/*
 * simulation.h - Execucao controlada de sequencias de operacoes.
 *
 * CONCEITO TEORICO
 * A simulacao amarra o modelo de memoria (src/memory.c), as estrategias de
 * alocacao (src/allocation.c) e as metricas (src/metrics.c). Cada simulacao
 * pertence a UMA estrategia e comeca com a memoria totalmente livre, de modo
 * que o resultado de uma estrategia nunca influencia o resultado das demais:
 * a mesma sequencia de operacoes e reaplicada em memorias diferentes.
 *
 * O historico de operacoes permite exibir, passo a passo, o que aconteceu em
 * cada solicitacao (endereco escolhido, sucesso/falha e blocos examinados).
 *
 * A simulacao tambem observa o estado da memoria APOS cada operacao e mantem
 * o PICO de fragmentacao externa observado. Isso e essencial para o cenario
 * de fragmentacao externa: o instante em que uma solicitacao falha e
 * individo em fragmentacao, mas o snapshot final da simulacao ja nao o
 * evidencia.
 */
#ifndef SIMULATION_H
#define SIMULATION_H

#include "allocation.h"
#include "memory.h"
#include "metrics.h"

/* Tipo de operacao registrada no historico. */
typedef enum {
    OPERATION_ALLOCATE = 0,
    OPERATION_FREE = 1
} OperationType;

/* Operacao executavel sobre a simulacao. */
typedef struct {
    OperationType type; /* OPERATION_ALLOCATE ou OPERATION_FREE */
    int process_id;     /* processo alvo */
    int size;           /* tamanho solicitado em KB (ignorado em FREE) */
} Operation;

/* Desfecho de uma operacao. */
typedef enum {
    OPERATION_STATUS_SUCCESS = 0,
    OPERATION_STATUS_FAILURE = 1,
    OPERATION_STATUS_INVALID = 2
} OperationStatus;

/* Registro de uma operacao executada (linha do historico). */
typedef struct {
    OperationType type;
    int process_id;
    int size;
    OperationStatus status;
    int start_address;   /* endereco escolhido (valido quando status = SUCCESS) */
    int blocks_examined; /* blocos percorridos durante a busca */
    char message[160];   /* descricao didatica do resultado */
} OperationRecord;

/* Estado de uma simulacao (memoria + estrategia + historico + contadores). */
typedef struct {
    Memory memory;
    AllocationStrategy strategy;
    OperationRecord *history;
    size_t history_count;
    size_t history_capacity;
    long successful_allocations;
    long failed_allocations;
    long total_blocks_examined;
    double peak_fragmentation;       /* maior fragmentacao observada (0.0 .. 1.0) */
    int peak_fragmentation_defined;  /* 1 = houve algum passo com fragmentacao definida */
} Simulation;

/*
 * simulation_init - Prepara a simulacao: memoria totalmente livre com
 * "total_size" KB e estrategia definida. Devolve 1 em caso de sucesso.
 */
int simulation_init(Simulation *simulation, int total_size,
                    AllocationStrategy strategy);

/* simulation_destroy - Libera o historico e a memoria da simulacao. */
void simulation_destroy(Simulation *simulation);

/*
 * simulation_reset - Reinicia a simulacao: memoria totalmente livre, historico
 * vazio e contadores zerados, preservando tamanho de memoria e estrategia.
 */
int simulation_reset(Simulation *simulation);

/*
 * simulation_execute - Executa uma operacao e registra o resultado no
 * historico. Devolve 1 quando a operacao foi executada com sucesso e 0 quando
 * falhou (sem alterar a memoria) ou quando os argumentos eram invalidos.
 */
int simulation_execute(Simulation *simulation, const Operation *operation);

/*
 * simulation_execute_sequence - Executa uma sequencia de operacoes, na ordem.
 * Devolve a quantidade de operacoes executadas com sucesso.
 */
int simulation_execute_sequence(Simulation *simulation, const Operation *operations,
                                size_t count);

/* simulation_metrics - Snapshot das metricas do estado atual da simulacao. */
Metrics simulation_metrics(const Simulation *simulation);

#endif /* SIMULATION_H */
