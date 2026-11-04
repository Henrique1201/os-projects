/*
 * scenarios.c - Cenarios controlados e geracao reproduzivel de sequencias.
 *
 * CONCEITO TEORICO
 * Comparar estrategias de alocacao exige isolar o efeito do algoritmo: todas
 * recebem a MESMA sequencia, sobre a MESMA configuracao inicial de memoria.
 * Os tres primeiros cenarios exploram fenomenos didaticos (empacotamento
 * inicial, fragmentacao externa e escolha de bloco diferente). O cenario de
 * estresse usa um PRNG proprio com semente explicita, pois rand() da libc nao
 * garante a mesma sequencia entre plataformas.
 */
#include "scenarios.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------- */
/* Infraestrutura do conjunto de cenarios                                     */
/* ------------------------------------------------------------------------- */

static int scenario_set_reserve(ScenarioSet *set, size_t needed)
{
    size_t capacity;
    Scenario *grown;

    if (needed <= set->capacity) {
        return 1;
    }

    capacity = set->capacity;
    if (capacity == 0) {
        capacity = 4;
    }
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = needed;
            break;
        }
        capacity *= 2u;
    }

    if (capacity > SIZE_MAX / sizeof(Scenario)) {
        return 0;
    }

    grown = realloc(set->items, capacity * sizeof(*grown));
    if (grown == NULL) {
        return 0;
    }

    set->items = grown;
    set->capacity = capacity;
    return 1;
}

/* scenario_set_add - Copia metadados e a sequencia para o conjunto. */
static int scenario_set_add(ScenarioSet *set, const char *name,
                            const char *description, const char *objective,
                            int memory_size, const Operation *operations,
                            size_t operation_count)
{
    Scenario *scenario;

    if (!scenario_set_reserve(set, set->count + 1u)) {
        return 0;
    }

    scenario = &set->items[set->count];
    memset(scenario, 0, sizeof(*scenario));

    snprintf(scenario->name, sizeof(scenario->name), "%s", name);
    snprintf(scenario->description, sizeof(scenario->description), "%s",
             description);
    snprintf(scenario->objective, sizeof(scenario->objective), "%s", objective);
    scenario->memory_size = memory_size;

    if (operation_count > 0) {
        if (operation_count > SIZE_MAX / sizeof(Operation)) {
            return 0;
        }
        scenario->operations = malloc(operation_count * sizeof(Operation));
        if (scenario->operations == NULL) {
            return 0;
        }
        memcpy(scenario->operations, operations,
               operation_count * sizeof(Operation));
    }
    scenario->operation_count = operation_count;

    set->count++;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Cenarios controlados                                                       */
/* ------------------------------------------------------------------------- */

int scenarios_create_controlled(ScenarioSet *set)
{
    static const Operation sequential_operations[] = {
        {OPERATION_ALLOCATE, 1, 100},
        {OPERATION_ALLOCATE, 2, 200},
        {OPERATION_ALLOCATE, 3, 150},
        {OPERATION_ALLOCATE, 4, 300}
    };

    static const Operation fragmentation_operations[] = {
        {OPERATION_ALLOCATE, 1, 256},
        {OPERATION_ALLOCATE, 2, 256},
        {OPERATION_ALLOCATE, 3, 256},
        {OPERATION_ALLOCATE, 4, 256},
        {OPERATION_FREE, 2, 0},
        {OPERATION_FREE, 4, 0},
        {OPERATION_ALLOCATE, 5, 400},
        {OPERATION_ALLOCATE, 6, 200}
    };

    static const Operation comparison_operations[] = {
        {OPERATION_ALLOCATE, 1, 300},
        {OPERATION_ALLOCATE, 2, 100},
        {OPERATION_ALLOCATE, 3, 200},
        {OPERATION_ALLOCATE, 4, 100},
        {OPERATION_ALLOCATE, 5, 300},
        {OPERATION_FREE, 1, 0},
        {OPERATION_FREE, 3, 0},
        {OPERATION_FREE, 5, 0},
        {OPERATION_ALLOCATE, 6, 180},
        {OPERATION_ALLOCATE, 7, 250}
    };

    if (set == NULL) {
        return 0;
    }

    set->items = NULL;
    set->count = 0;
    set->capacity = 0;

    if (!scenario_set_add(
            set, "Alocacoes sequenciais",
            "Quatro processos solicitam blocos em sequencia, sem liberacoes.",
            "Observar o comportamento inicial das tres estrategias quando a "
            "memoria ainda esta praticamente livre.",
            1024, sequential_operations,
            sizeof(sequential_operations) / sizeof(sequential_operations[0]))) {
        scenarios_destroy(set);
        return 0;
    }

    if (!scenario_set_add(
            set, "Fragmentacao externa",
            "Quatro blocos de 256 KB ocupam exatamente os 1024 KB; ao liberar "
            "os processos 2 e 4, restam 512 KB livres em dois blocos de "
            "256 KB.",
            "Mostrar que 512 KB livres no total nao bastam para uma solicitacao "
            "de 400 KB quando a maior regiao contigua tem apenas 256 KB.",
            1024, fragmentation_operations,
            sizeof(fragmentation_operations) /
                sizeof(fragmentation_operations[0]))) {
        scenarios_destroy(set);
        return 0;
    }

    if (!scenario_set_add(
            set, "Criterios de escolha de bloco",
            "Processos de 100 KB separam as regioes livres; apos as tres "
            "liberacoes existem 300 KB em 0, 200 KB em 400 e 324 KB em 700.",
            "No pedido de 180 KB, First Fit escolhe 0, Best Fit escolhe 400 e "
            "Worst Fit escolhe 700; no pedido de 250 KB, First Fit escolhe 700 "
            "e Best Fit/Worst Fit escolhem 0.",
            1024, comparison_operations,
            sizeof(comparison_operations) / sizeof(comparison_operations[0]))) {
        scenarios_destroy(set);
        return 0;
    }

    return 1;
}

/* ------------------------------------------------------------------------- */
/* Cenario de estresse com PRNG deterministico                                */
/* ------------------------------------------------------------------------- */

/*
 * rng_next - xorshift32.
 *
 * Escolhido por ser um gerador pequeno, sem estado global e reproduzivel em
 * qualquer plataforma com inteiros de 32 bits. A semente zero e degenerada
 * (produz sempre zero), por isso e substituida por uma constante nao nula.
 */
static uint32_t rng_next(uint32_t *state)
{
    uint32_t x = *state;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Sorteia um inteiro uniforme em [0, bound - 1], sem vies de modulo. */
static uint32_t random_below(uint32_t *state, uint32_t bound)
{
    uint32_t limit;
    uint32_t value;

    if (bound <= 1u) {
        return 0u;
    }

    /*
     * Amostragem por rejeicao: descarta os valores do topo que fariam o
     * resto da divisao ficar mais frequente para alguns resultados.
     */
    limit = UINT32_MAX - (UINT32_MAX % bound);
    do {
        value = rng_next(state);
    } while (value >= limit);

    return value % bound;
}

/* Sorteia um tamanho em [min_size, max_size], tambem sem vies. */
static int random_size(uint32_t *state, int min_size, int max_size)
{
    uint32_t span = (uint32_t)(max_size - min_size) + 1u;

    return min_size + (int)random_below(state, span);
}

/* Sorteia um processo ainda nao alocado (1..process_count). */
static int pick_unallocated(const unsigned char *allocated, int process_count,
                            uint32_t random_value)
{
    unsigned int offset = random_value % (unsigned int)process_count;
    int i;

    for (i = 0; i < process_count; i++) {
        int pid = (int)((offset + (unsigned int)i) %
                        (unsigned int)process_count) +
                  1;
        if (!allocated[pid]) {
            return pid;
        }
    }
    return 0;
}

/* Sorteia um processo atualmente alocado (1..process_count). */
static int pick_allocated(const unsigned char *allocated, int process_count,
                          uint32_t random_value)
{
    unsigned int offset = random_value % (unsigned int)process_count;
    int i;

    for (i = 0; i < process_count; i++) {
        int pid = (int)((offset + (unsigned int)i) %
                        (unsigned int)process_count) +
                  1;
        if (allocated[pid]) {
            return pid;
        }
    }
    return 0;
}

int scenarios_append_stress(ScenarioSet *set, int memory_size,
                            int process_count, int min_size, int max_size,
                            int operation_count, unsigned int seed)
{
    unsigned char *allocated;
    Operation *operations;
    uint32_t state;
    int live_count;
    int index;
    char name[SCENARIO_NAME_MAX];
    char description[SCENARIO_TEXT_MAX];
    char objective[SCENARIO_TEXT_MAX];
    int ok;

    if (set == NULL || memory_size <= 0 || process_count <= 0 ||
        min_size <= 0 || max_size < min_size || min_size > memory_size ||
        operation_count <= 0) {
        return 0;
    }

    /*
     * Geracao CEGA em relacao as estrategias: o tamanho de cada pedido e
     * sorteado livremente em [min_size, max_size], sem consultar nenhuma
     * memoria simulada. Assim, a mesma sequencia pode ser atendida por uma
     * estrategia e recusada por outra, e as taxas de sucesso/falha passam a
     * diferenciar os algoritmos.
     *
     * A comparacao justa e preservada porque a sequencia e gerada UMA vez e
     * guardada no cenario; cada estrategia a reaplica em memoria nova.
     *
     * "allocated" acompanha os processos com ALLOCATE pendente na propria
     * sequencia. FREE so e emitido para um pid nessa condicao. O desfecho do
     * FREE em tempo de execucao depende da estrategia e e justamente o que se
     * deseja observar.
     */
    allocated = calloc((size_t)process_count + 1u, sizeof(*allocated));
    if (allocated == NULL) {
        return 0;
    }

    if ((size_t)operation_count > SIZE_MAX / sizeof(Operation)) {
        free(allocated);
        return 0;
    }
    operations = malloc((size_t)operation_count * sizeof(*operations));
    if (operations == NULL) {
        free(allocated);
        return 0;
    }

    state = (uint32_t)seed;
    if (state == 0u) {
        state = 0x9E3779B9u;
    }
    live_count = 0;

    for (index = 0; index < operation_count; index++) {
        int make_free;

        if (live_count == 0) {
            make_free = 0;
        } else if (live_count >= process_count) {
            make_free = 1;
        } else {
            make_free = random_below(&state, 100u) < 40u;
        }

        if (!make_free) {
            /*
             * Invariante: live_count < process_count, logo existe ao menos um
             * pid livre e pick_unallocated sempre devolve um valor valido.
             */
            int pid = pick_unallocated(allocated, process_count,
                                       random_below(&state,
                                                    (uint32_t)process_count));

            operations[index].type = OPERATION_ALLOCATE;
            operations[index].process_id = pid;
            operations[index].size = random_size(&state, min_size, max_size);
            allocated[pid] = 1;
            live_count++;
        } else {
            /*
             * Invariante: live_count > 0, logo pick_allocated sempre devolve
             * um pid efetivamente alocado antes na sequencia.
             */
            int pid = pick_allocated(allocated, process_count,
                                     random_below(&state,
                                                  (uint32_t)process_count));

            operations[index].type = OPERATION_FREE;
            operations[index].process_id = pid;
            operations[index].size = 0;
            allocated[pid] = 0;
            live_count--;
        }
    }

    free(allocated);

    snprintf(name, sizeof(name), "%s", "Estresse pseudoaleatorio");
    snprintf(description, sizeof(description),
             "Sequencia reproduzivel de %d operacoes com %d processos, "
             "tamanhos entre %d e %d KB e semente %u; o desfecho de cada "
             "pedido depende da estrategia.",
             operation_count, process_count, min_size, max_size, seed);
    snprintf(objective, sizeof(objective),
             "Comparar como First Fit, Best Fit e Worst Fit lidam com a mesma "
             "carga mista, incluindo solicitacoes recusadas e o efeito da "
             "fragmentacao externa.");

    ok = scenario_set_add(set, name, description, objective, memory_size,
                          operations, (size_t)operation_count);
    free(operations);
    return ok;
}

/* ------------------------------------------------------------------------- */
/* Consulta e destruicao                                                      */
/* ------------------------------------------------------------------------- */

const Scenario *scenarios_get(const ScenarioSet *set, size_t index)
{
    if (set == NULL || index >= set->count) {
        return NULL;
    }
    return &set->items[index];
}

void scenarios_destroy(ScenarioSet *set)
{
    size_t i;

    if (set == NULL) {
        return;
    }

    for (i = 0; i < set->count; i++) {
        free(set->items[i].operations);
        set->items[i].operations = NULL;
        set->items[i].operation_count = 0;
    }
    free(set->items);
    set->items = NULL;
    set->count = 0;
    set->capacity = 0;
}
