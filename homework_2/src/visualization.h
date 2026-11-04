/*
 * visualization.h - Camada de apresentacao (terminal) do simulador.
 *
 * Este modulo contem APENAS apresentacao: nenhuma regra de gerenciamento de
 * memoria, nenhuma decisao de estrategia. Os dados exibidos vem sempre do
 * estado real da simulacao, nunca de valores fixos.
 */
#ifndef VISUALIZATION_H
#define VISUALIZATION_H

#include "memory.h"
#include "metrics.h"
#include "simulation.h"

/* Largura util da linha de texto do simulador. */
#define VISUALIZATION_LINE_WIDTH 78

/* Numero de estrategias comparadas side a side (First/Best/Worst Fit). */
#define VISUALIZATION_STRATEGY_COLUMNS 3

/*
 * Linha da tabela "endereco escolhido": uma operacao e o desfecho da MESMA
 * operacao em cada estrategia (comparacao justa, passo a passo).
 */
typedef struct {
    int step;   /* numero da operacao, comecando em 1 */
    char operation[40]; /* descricao da operacao, ex.: "ALLOCATE P6 180 KB" */
    int success[VISUALIZATION_STRATEGY_COLUMNS];  /* 1 = atendida, 0 = falha */
    int address[VISUALIZATION_STRATEGY_COLUMNS];  /* endereco escolhido; -1 se falhou */
} AddressComparisonRow;

/* Cabecalho principal do programa. */
void visualization_print_banner(void);

/* Menu de opcoes da interface de terminal. */
void visualization_print_menu(void);

/* Texto com os conceitos teoricos (fator de comparacao, fragmentacao, etc.). */
void visualization_print_concepts(void);

/* Mapa visual da regiao de memoria: blocos, enderecos e tamanhos reais. */
void visualization_print_memory_map(const Memory *memory);

/* Resultado de uma operacao (sucesso/falha, endereco, blocos examinados). */
void visualization_print_operation(const OperationRecord *record);

/* Bloco de metricas de uma unica simulacao. */
void visualization_print_metrics(const Metrics *metrics, const char *title);

/*
 * Tabela comparativa: "strategy_names" e "metrics" possuem a mesma quantidade
 * de elementos (tipicamente 3: First Fit, Best Fit e Worst Fit).
 */
void visualization_print_comparison(const char *const *strategy_names,
                                    const Metrics *const *metrics, size_t count);

/*
 * Tabela "endereco escolhido": para cada operacao da sequencia, mostra o
 * endereco que cada estrategia selecionou (ou "falhou"), permitindo comparar
 * o criterio de escolha passo a passo.
 */
void visualization_print_address_comparison(const char *const *strategy_names,
                                             const AddressComparisonRow *rows,
                                             size_t row_count);

/* Historico completo de operacoes da simulacao. */
void visualization_print_history(const Simulation *simulation);

/* Cabecalho de um cenario (nome, descricao, objetivo e tamanho de memoria). */
void visualization_print_scenario_header(const char *name, const char *description,
                                        const char *objective, int memory_size);

#endif /* VISUALIZATION_H */
