/*
 * simulation.c - Execucao controlada de sequencias de operacoes.
 *
 * CONCEITO TEORICO
 * A simulacao amarra tres componentes: o modelo de memoria (lista de blocos),
 * a estrategia de escolha de bloco (First/Best/Worst Fit) e os contadores que
 * alimentam as metricas. Cada operacao executada gera um registro no historico,
 * permitindo reconstruir passo a passo a evolucao da memoria.
 *
 * O historico e um vetor dinamico: cresce por dobragem para manter o custo
 * amortizado de insercao em O(1). A capacidade inicial e 16 registros.
 */
#include "simulation.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Capacidade inicial do vetor de historico. */
#define SIMULATION_INITIAL_HISTORY_CAPACITY 16u

/*
 * history_reserve - Garante capacidade para "needed" registros.
 *
 * A reserva acontece ANTES de alterar a memoria. Assim, uma eventual falha de
 * alocacao nunca deixa a memoria modificada sem o registro correspondente:
 * ou a operacao inteira acontece, ou nada muda.
 */
static int history_reserve(Simulation *simulation, size_t needed)
{
    size_t capacity;
    OperationRecord *grown;

    if (needed <= simulation->history_capacity) {
        return 1;
    }

    capacity = simulation->history_capacity;
    if (capacity == 0) {
        capacity = SIMULATION_INITIAL_HISTORY_CAPACITY;
    }
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2u) {
            capacity = needed;
            break;
        }
        capacity *= 2u;
    }

    if (capacity > SIZE_MAX / sizeof(OperationRecord)) {
        return 0;
    }

    grown = realloc(simulation->history, capacity * sizeof(*grown));
    if (grown == NULL) {
        return 0;
    }

    simulation->history = grown;
    simulation->history_capacity = capacity;
    return 1;
}

/* history_append - Copia um registro para o fim do historico. */
static void history_append(Simulation *simulation, const OperationRecord *record)
{
    simulation->history[simulation->history_count] = *record;
    simulation->history_count++;
}

/*
 * simulation_observe - Atualiza o pico de fragmentacao externa.
 *
 * Formula: 1 - (maior bloco livre / memoria livre total). Sem memoria livre,
 * o indicador e indefinido naquele passo e o pico anterior e preservado.
 */
static void simulation_observe(Simulation *simulation)
{
    int free_memory = memory_total_free(&simulation->memory);
    double fragmentation;

    if (free_memory <= 0) {
        return;
    }

    fragmentation = metrics_fragmentation(
        memory_largest_free_block(&simulation->memory), free_memory);

    if (!simulation->peak_fragmentation_defined ||
        fragmentation > simulation->peak_fragmentation) {
        simulation->peak_fragmentation = fragmentation;
    }
    simulation->peak_fragmentation_defined = 1;
}

int simulation_init(Simulation *simulation, int total_size,
                    AllocationStrategy strategy)
{
    if (simulation == NULL) {
        return 0;
    }

    memset(simulation, 0, sizeof(*simulation));

    if (strategy != STRATEGY_FIRST_FIT && strategy != STRATEGY_BEST_FIT &&
        strategy != STRATEGY_WORST_FIT) {
        return 0;
    }

    if (!memory_init(&simulation->memory, total_size)) {
        return 0;
    }

    simulation->strategy = strategy;
    simulation->history = malloc(SIMULATION_INITIAL_HISTORY_CAPACITY *
                                 sizeof(*simulation->history));
    if (simulation->history == NULL) {
        memory_destroy(&simulation->memory);
        memset(simulation, 0, sizeof(*simulation));
        return 0;
    }
    simulation->history_capacity = SIMULATION_INITIAL_HISTORY_CAPACITY;
    simulation->history_count = 0;
    simulation->successful_allocations = 0;
    simulation->failed_allocations = 0;
    simulation->total_blocks_examined = 0;
    simulation->peak_fragmentation = 0.0;
    simulation->peak_fragmentation_defined = 0;

    return 1;
}

void simulation_destroy(Simulation *simulation)
{
    if (simulation == NULL) {
        return;
    }

    free(simulation->history);
    simulation->history = NULL;
    simulation->history_count = 0;
    simulation->history_capacity = 0;
    memory_destroy(&simulation->memory);
}

int simulation_reset(Simulation *simulation)
{
    if (simulation == NULL) {
        return 0;
    }

    if (!memory_reset(&simulation->memory)) {
        return 0;
    }

    simulation->history_count = 0;
    simulation->successful_allocations = 0;
    simulation->failed_allocations = 0;
    simulation->total_blocks_examined = 0;
    simulation->peak_fragmentation = 0.0;
    simulation->peak_fragmentation_defined = 0;

    /* A estrategia e o buffer do historico sao preservados. */
    return 1;
}

int simulation_execute(Simulation *simulation, const Operation *operation)
{
    OperationRecord record;
    int success = 0;
    int observed = 0;

    if (simulation == NULL || operation == NULL) {
        return 0;
    }

    /* Reserva antes de tocar na memoria para manter historico e estado
     * consistentes mesmo em caso de falta de memoria do proprio vetor. */
    if (!history_reserve(simulation, simulation->history_count + 1u)) {
        return 0;
    }

    memset(&record, 0, sizeof(record));
    record.type = operation->type;
    record.process_id = operation->process_id;
    record.size = operation->size;
    record.start_address = -1;
    record.blocks_examined = 0;

    if (operation->type == OPERATION_ALLOCATE) {
        AllocationResult result;

        memset(&result, 0, sizeof(result));
        observed = 1;

        if (allocation_run(simulation->strategy, &simulation->memory,
                           operation->process_id, operation->size,
                           &result) != 0) {
            record.status = OPERATION_STATUS_INVALID;
            snprintf(record.message, sizeof(record.message),
                     "Solicitacao de alocacao invalida");
        } else {
            /* Toda busca custa percorrimento de blocos, mesmo ao falhar. */
            record.blocks_examined = result.blocks_examined;
            simulation->total_blocks_examined += (long)result.blocks_examined;

            if (result.success) {
                record.status = OPERATION_STATUS_SUCCESS;
                record.start_address = result.start_address;
                simulation->successful_allocations++;
                snprintf(record.message, sizeof(record.message),
                         "Bloco alocado em %d KB", result.start_address);
                success = 1;
            } else {
                record.status = OPERATION_STATUS_FAILURE;
                simulation->failed_allocations++;
                if (result.status == MEMORY_ERROR_NO_SPACE) {
                    snprintf(record.message, sizeof(record.message),
                             "Sem espaco contiguo suficiente para %d KB",
                             operation->size);
                } else {
                    snprintf(record.message, sizeof(record.message),
                             "Alocacao recusada: %s",
                             memory_status_message(result.status));
                }
            }
        }
    } else if (operation->type == OPERATION_FREE) {
        const MemoryBlock *block;
        MemoryStatus status;
        int freed_start;
        int freed_size;

        observed = 1;

        /* O endereco e o tamanho precisam ser lidos ANTES da liberacao: a
         * coalescencia pode fundir o bloco com os vizinhos livres. */
        block = memory_find_process(&simulation->memory, operation->process_id);
        freed_start = (block != NULL) ? block->start_address : -1;
        freed_size = (block != NULL) ? block->size : 0;

        status = memory_free(&simulation->memory, operation->process_id);
        if (status == MEMORY_OK) {
            record.status = OPERATION_STATUS_SUCCESS;
            record.start_address = freed_start;
            record.size = freed_size;
            snprintf(record.message, sizeof(record.message),
                     "Processo %d liberado (%d KB)", operation->process_id,
                     freed_size);
            success = 1;
        } else {
            /* IMPORTANTE: uma falha de FREE nao e uma solicitacao de
             * alocacao recusada; por isso NAO incrementa failed_allocations.
             * O contador mede apenas tentativas de alocar memoria. */
            record.status = OPERATION_STATUS_FAILURE;
            snprintf(record.message, sizeof(record.message),
                     "Processo %d nao encontrado na memoria",
                     operation->process_id);
        }
    } else {
        /* Tipo desconhecido: nao ha operacao de memoria a executar. */
        record.status = OPERATION_STATUS_INVALID;
        snprintf(record.message, sizeof(record.message),
                 "Tipo de operacao desconhecido");
    }

    history_append(simulation, &record);

    /* Observa o estado resultante de toda tentativa de alocacao/liberacao. */
    if (observed) {
        simulation_observe(simulation);
    }

    return success;
}

int simulation_execute_sequence(Simulation *simulation,
                                const Operation *operations, size_t count)
{
    size_t i;
    int successes = 0;

    if (simulation == NULL || (operations == NULL && count > 0)) {
        return 0;
    }

    for (i = 0; i < count; i++) {
        if (simulation_execute(simulation, &operations[i])) {
            successes++;
        }
    }

    return successes;
}

Metrics simulation_metrics(const Simulation *simulation)
{
    Metrics metrics;

    memset(&metrics, 0, sizeof(metrics));
    if (simulation == NULL) {
        return metrics;
    }

    metrics_compute(&simulation->memory, simulation->successful_allocations,
                    simulation->failed_allocations,
                    simulation->total_blocks_examined,
                    simulation->peak_fragmentation,
                    simulation->peak_fragmentation_defined, &metrics);
    return metrics;
}
