/*
 * scenarios.h - Cenarios controlados e geracao reproduzivel de sequencias.
 *
 * CONCEITO TEORICO
 * Para comparar as estrategias de forma justa, todas recebem EXATAMENTE a
 * mesma sequencia de operacoes, partindo da mesma configuracao de memoria.
 * Alem dos cenarios controlados (que isolam um fenomeno por vez), o cenario de
 * estresse usa um gerador pseudoaleatorio COM SEMENTE explicita, de modo que a
 * sequencia possa ser reproduzida integralmente por qualquer pessoa.
 */
#ifndef SCENARIOS_H
#define SCENARIOS_H

#include "simulation.h"

#define SCENARIO_NAME_MAX 64
#define SCENARIO_TEXT_MAX 240

/* Cenario controlado: metadados + sequencia de operacoes. */
typedef struct {
    char name[SCENARIO_NAME_MAX];
    char description[SCENARIO_TEXT_MAX];
    char objective[SCENARIO_TEXT_MAX];
    int memory_size;              /* tamanho da memoria do cenario, em KB */
    Operation *operations;        /* sequencia de operacoes */
    size_t operation_count;       /* quantidade de operacoes da sequencia */
} Scenario;

/* Colecao de cenarios. */
typedef struct {
    Scenario *items;
    size_t count;
    size_t capacity;
} ScenarioSet;

/*
 * scenarios_create_controlled - Monta os cenarios controlados:
 *   1) Alocacoes sequenciais (comportamento inicial das estrategias);
 *   2) Fragmentacao externa (memoria livre total suficiente, mas nao contigua);
 *   3) Comparacao entre algoritmos (sequencia em que as tres estrategias
 *      escolhem blocos diferentes).
 * Devolve 1 em caso de sucesso.
 */
int scenarios_create_controlled(ScenarioSet *set);

/*
 * scenarios_append_stress - Acrescenta ao conjunto o cenario de estresse,
 * com sequencia pseudoaleatoria reproduzivel (mesma semente => mesma sequencia).
 * Parametros: tamanho da memoria (KB), quantidade de processos, tamanho
 * minimo e maximo das solicitacoes (KB), quantidade de operacoes e semente.
 * Devolve 1 em caso de sucesso.
 */
int scenarios_append_stress(ScenarioSet *set, int memory_size, int process_count,
                            int min_size, int max_size, int operation_count,
                            unsigned int seed);

/* scenarios_destroy - Libera todos os cenarios e suas sequencias. */
void scenarios_destroy(ScenarioSet *set);

/* scenarios_get - Acesso a um cenario por indice (NULL se indice invalido). */
const Scenario *scenarios_get(const ScenarioSet *set, size_t index);

#endif /* SCENARIOS_H */
