/*
 * metrics.h - Indicadores de desempenho e fragmentacao externa.
 *
 * CONCEITO TEORICO
 * As metricas comparam as tres estrategias sobre a MESMA sequencia de
 * operacoes. Todas sao calculadas a partir do estado final da memoria e dos
 * contadores acumulados durante a simulacao.
 *
 *   - Taxa de sucesso  = (alocacoes atendidas / solicitacoes) * 100
 *   - Taxa de falha    = (alocacoes nao atendidas / solicitacoes) * 100
 *   - Memoria livre    = somatorio dos blocos livres
 *   - Maior bloco livre= maior regiao contigua disponivel
 *   - Blocos livres    = quantidade de regioes livres
 *   - Fragmentacao externa = 1 - (maior bloco livre / memoria livre total)
 *   - Utilizacao       = (memoria ocupada / memoria total) * 100
 *   - Blocos examinados = total de blocos percorridos nas buscas
 *
 * CONVENCAO ADOTADA
 * Quando a memoria livre total e zero, o indicador de fragmentacao externa e
 * INDEFINIDO: o campo fragmentation_defined recebe 0 e o campo fragmentation
 * recebe 0.0 (evita divisao por zero sem inventar um valor).
 */
#ifndef METRICS_H
#define METRICS_H

#include "memory.h"

/* Conjunto de metricas de uma simulacao. */
typedef struct {
    long total_requests;           /* solicitacoes de alocacao processadas */
    long successful_allocations;   /* alocacoes bem-sucedidas */
    long failed_allocations;       /* alocacoes malsucedidas */
    long total_blocks_examined;    /* blocos percorridos nas buscas de selecao */
    int total_memory;              /* tamanho total da memoria, em KB */
    int free_memory;               /* memoria livre total, em KB */
    int used_memory;               /* memoria ocupada, em KB */
    int largest_free_block;        /* maior bloco livre, em KB */
    int free_block_count;          /* quantidade de blocos livres */
    int occupied_block_count;      /* quantidade de blocos ocupados */
    double fragmentation;          /* 0.0 .. 1.0 (valido se fragmentation_defined) */
    int fragmentation_defined;     /* 1 = fragmentacao definida, 0 = indefinida */
    double peak_fragmentation;     /* maior fragmentacao observada em algum passo */
    int peak_fragmentation_defined; /* 1 = houve algum passo com fragmentacao definida */
    double utilization_percent;    /* percentual de memoria ocupada */
    double success_rate_percent;   /* percentual de solicitudes atendidas */
    double failure_rate_percent;   /* percentual de solicitudes nao atendidas */
} Metrics;

/*
 * metrics_compute - Calcula todas as metricas a partir do estado da memoria e
 * dos contadores acumulados (alocacoes com sucesso, falhas e blocos examinados).
 *
 * "peak_fragmentation" e "peak_fragmentation_defined" descrevem o PICO de
 * fragmentacao observado ao longo da execucao (e nao apenas o estado final):
 * e o que evidencia, por exemplo, o instante em que uma solicitacao falha
 * apesar de haver memoria livre total suficiente. Sao preenchidos pela camada
 * de simulacao, que observa o estado apos cada operacao.
 */
void metrics_compute(const Memory *memory, long successful_allocations,
                     long failed_allocations, long total_blocks_examined,
                     double peak_fragmentation, int peak_fragmentation_defined,
                     Metrics *metrics);

/*
 * metrics_fragmentation - Indicador de fragmentacao externa:
 *   1 - (maior bloco livre / memoria livre total)
 * Devolve 0.0 quando a memoria livre total e zero (caso indefinido).
 */
double metrics_fragmentation(int largest_free_block, int total_free_memory);

#endif /* METRICS_H */
