/*
 * allocation.h - Estrategias de escolha de bloco livre (particoes variaveis).
 *
 * CONCEITO TEORICO
 * Quando um processo solicita memoria, o gerenciador precisa escolher QUAL
 * bloco livre sera usado. As tres estrategias classicas estudadas em Sistemas
 * Operacionais percorrem a MESMA lista de blocos do mesmo modelo de memoria:
 *
 *   - First Fit: percorre em ordem de endereco e para no primeiro bloco livre
 *     com tamanho suficiente (busca sequencial, custo menor de busca).
 *   - Best Fit: percorre todos os blocos livres e escolhe o MENOR bloco com
 *     tamanho suficiente; em empate, escolhe o de menor endereco inicial.
 *   - Worst Fit: percorre todos os blocos livres e escolhe o MAIOR bloco com
 *     tamanho suficiente; em empate, escolhe o de menor endereco inicial.
 *
 * As tres estrategias compartilham as mesmas primitivas de divisao e liberacao
 * (src/memory.c). Nao existem tres simuladores independentes aqui: existe um
 * unico modelo de memoria e tres criterios de selecao de bloco.
 */
#ifndef ALLOCATION_H
#define ALLOCATION_H

#include "memory.h"

/* Estrategias de alocacao contigua suportadas. */
typedef enum {
    STRATEGY_FIRST_FIT = 0,
    STRATEGY_BEST_FIT = 1,
    STRATEGY_WORST_FIT = 2,
    STRATEGY_COUNT = 3
} AllocationStrategy;

/*
 * CONVENCAO DE "blocks_examined"
 * Quantidade de blocos PERCORRIDOS DURANTE A BUSCA DE SELECAO do bloco livre.
 * First Fit pode encerrar a busca no primeiro bloco adequado; Best Fit e
 * Worst Fit percorrem a lista inteira. A contagem NAO inclui os percursos
 * auxiliares de manutencao (localizacao do processo, conferencia de
 * pertinencia do bloco e recalculo dos contadores), que sao identicos para as
 * tres estrategias. Por isso a metrica e um proxy COMPARATIVO do custo de
 * busca, e nao o tempo total da operacao.
 */
typedef struct {
    int success;            /* 1 = atendida, 0 = falha */
    int start_address;      /* endereco inicial da alocacao (valido se success) */
    int allocated_size;     /* tamanho efetivamente alocado, em KB */
    int blocks_examined;    /* blocos percorridos na busca de selecao */
    MemoryStatus status;    /* codigo de retorno detalhado */
} AllocationResult;

/*
 * As funcoes abaixo devolvem:
 *   0  -> a tentativa foi executada (sucesso OU falha registrada em "result");
 *   -1 -> argumentos invalidos (ponteiro nulo, tamanho <= 0, estrategia
 *         desconhecida). Nesses casos o estado da memoria nao e alterado.
 *
 * Uma falha por falta de espaco contiguo devolve 0, com result->success = 0
 * e result->status = MEMORY_ERROR_NO_SPACE, e NAO modifica a memoria.
 */
int allocation_run(AllocationStrategy strategy, Memory *memory, int process_id,
                   int requested_size, AllocationResult *result);

int allocation_first_fit(Memory *memory, int process_id, int requested_size,
                         AllocationResult *result);

int allocation_best_fit(Memory *memory, int process_id, int requested_size,
                        AllocationResult *result);

int allocation_worst_fit(Memory *memory, int process_id, int requested_size,
                         AllocationResult *result);

/* Nome curto da estrategia ("First Fit", "Best Fit", "Worst Fit"). */
const char *allocation_strategy_name(AllocationStrategy strategy);

/* Descricao didatica (sem acentos) do criterio de escolha da estrategia. */
const char *allocation_strategy_description(AllocationStrategy strategy);

#endif /* ALLOCATION_H */
