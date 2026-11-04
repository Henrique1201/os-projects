/*
 * test_allocation.c - Testes das tres estrategias de escolha de bloco.
 *
 * As memorias dos cenarios sao montadas MANUALMENTE (lista de blocos), sem
 * usar src/simulation.c, para controlar exatamente a geometria dos blocos
 * livres e verificar qual deles cada estrategia escolhe.
 */
#include <stdlib.h>

#include "allocation.h"
#include "test_utils.h"

#define MAX_BLOCKS 16

typedef struct {
    int size;
    int process_id; /* NO_PROCESS para bloco livre */
} BlockSpec;

/* Monta uma lista de blocos em ordem de endereco a partir das especificacoes. */
static int build_layout(Memory *memory, int total_size,
                        const BlockSpec *specs, size_t count)
{
    size_t i;
    int address = 0;
    MemoryBlock *tail = NULL;

    memory->total_size = total_size;
    memory->head = NULL;
    memory->block_count = 0;
    memory->free_block_count = 0;
    memory->occupied_block_count = 0;

    for (i = 0; i < count; i++) {
        MemoryBlock *block = malloc(sizeof(*block));

        if (block == NULL) {
            memory_destroy(memory);
            return 0;
        }

        block->process_id = specs[i].process_id;
        block->start_address = address;
        block->size = specs[i].size;
        block->state = (specs[i].process_id == NO_PROCESS) ? BLOCK_FREE
                                                           : BLOCK_OCCUPIED;
        block->next = NULL;

        if (tail == NULL) {
            memory->head = block;
        } else {
            tail->next = block;
        }
        tail = block;

        address += specs[i].size;
        memory->block_count++;
        if (block->state == BLOCK_FREE) {
            memory->free_block_count++;
        } else {
            memory->occupied_block_count++;
        }
    }

    if (address != total_size) {
        memory_destroy(memory);
        return 0;
    }

    return 1;
}

/*
 * Guarda tamanhos, enderecos, estados e processos dos blocos para comparar
 * depois. O process_id torna explicito que nenhum processo se moveu e que nao
 * houve compactacao.
 */
static size_t snapshot(const Memory *memory, int *sizes, int *starts,
                       BlockState *states, int *process_ids, size_t capacity)
{
    size_t count = 0;
    const MemoryBlock *block;

    for (block = memory->head; block != NULL; block = block->next) {
        if (count >= capacity) {
            return capacity + 1;
        }
        sizes[count] = block->size;
        starts[count] = block->start_address;
        states[count] = block->state;
        process_ids[count] = block->process_id;
        count++;
    }
    return count;
}

static int snapshot_matches(const Memory *memory, const int *sizes,
                            const int *starts, const BlockState *states,
                            const int *process_ids, size_t count)
{
    size_t i = 0;
    const MemoryBlock *block;

    for (block = memory->head; block != NULL; block = block->next) {
        if (i >= count) {
            return 0;
        }
        if (block->size != sizes[i] || block->start_address != starts[i] ||
            block->state != states[i] || block->process_id != process_ids[i]) {
            return 0;
        }
        i++;
    }
    return i == count && memory->block_count == count;
}

static void test_first_fit_picks_first_adequate(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {300, NO_PROCESS},
        {100, 1},
        {200, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 600, specs, 3), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_first_fit(&memory, 2, 80, &result), 0,
                      "retorno da tentativa");
    TEST_CHECK_EQ_INT(result.success, 1, "alocacao deve ter sucesso");
    /* Dois blocos livres (300 em 0 e 200 em 400) servem; First Fit usa o 0. */
    TEST_CHECK_EQ_INT(result.start_address, 0, "primeiro bloco adequado");
    TEST_CHECK_EQ_INT(result.allocated_size, 80, "tamanho alocado");
    TEST_CHECK_EQ_INT(result.blocks_examined, 1, "blocos examinados");
    TEST_CHECK_EQ_INT(result.status, MEMORY_OK, "status de sucesso");
    TEST_CHECK_MEMORY_VALID(&memory, "apos First Fit");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 180, "memoria ocupada");

    memory_destroy(&memory);
}

static void test_best_fit_picks_smallest_adequate(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {300, NO_PROCESS},
        {100, 1},
        {100, NO_PROCESS},
        {100, 2},
        {200, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 800, specs, 5), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_best_fit(&memory, 3, 80, &result), 0,
                      "retorno da tentativa");
    TEST_CHECK_EQ_INT(result.success, 1, "alocacao deve ter sucesso");
    /* Candidatos: 300 em 0, 100 em 400 e 200 em 600. Best Fit usa o 100. */
    TEST_CHECK_EQ_INT(result.start_address, 400, "menor bloco adequado");
    TEST_CHECK_EQ_INT(result.allocated_size, 80, "tamanho alocado");
    TEST_CHECK_EQ_INT(result.blocks_examined, 5, "examina todos os blocos");
    TEST_CHECK_MEMORY_VALID(&memory, "apos Best Fit");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 520, "memoria livre");

    memory_destroy(&memory);
}

static void test_best_fit_tie_uses_lowest_address(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {200, NO_PROCESS},
        {100, 1},
        {200, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 500, specs, 3), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_best_fit(&memory, 2, 100, &result), 0,
                      "retorno da tentativa");
    TEST_CHECK_EQ_INT(result.success, 1, "alocacao deve ter sucesso");
    TEST_CHECK_EQ_INT(result.start_address, 0, "empate pelo menor endereco");
    TEST_CHECK_EQ_INT(result.blocks_examined, 3, "examina todos os blocos");
    TEST_CHECK_MEMORY_VALID(&memory, "apos Best Fit com empate");

    memory_destroy(&memory);
}

static void test_worst_fit_picks_largest_adequate(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {300, NO_PROCESS},
        {100, 1},
        {100, NO_PROCESS},
        {100, 2},
        {200, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 800, specs, 5), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_worst_fit(&memory, 3, 80, &result), 0,
                      "retorno da tentativa");
    TEST_CHECK_EQ_INT(result.success, 1, "alocacao deve ter sucesso");
    /* Candidatos: 300 em 0, 100 em 400 e 200 em 600. Worst Fit usa o 300. */
    TEST_CHECK_EQ_INT(result.start_address, 0, "maior bloco adequado");
    TEST_CHECK_EQ_INT(result.allocated_size, 80, "tamanho alocado");
    TEST_CHECK_EQ_INT(result.blocks_examined, 5, "examina todos os blocos");
    TEST_CHECK_MEMORY_VALID(&memory, "apos Worst Fit");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 520, "memoria livre");

    memory_destroy(&memory);
}

static void test_worst_fit_tie_uses_lowest_address(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {200, NO_PROCESS},
        {100, 1},
        {200, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 500, specs, 3), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_worst_fit(&memory, 2, 100, &result), 0,
                      "retorno da tentativa");
    TEST_CHECK_EQ_INT(result.success, 1, "alocacao deve ter sucesso");
    TEST_CHECK_EQ_INT(result.start_address, 0, "empate pelo menor endereco");
    TEST_CHECK_EQ_INT(result.blocks_examined, 3, "examina todos os blocos");
    TEST_CHECK_MEMORY_VALID(&memory, "apos Worst Fit com empate");

    memory_destroy(&memory);
}

static void test_failure_external_fragmentation_keeps_memory(void)
{
    int strategy;

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        Memory memory;
        AllocationResult result;
        BlockSpec specs[] = {
            {100, NO_PROCESS},
            {100, 1},
            {100, NO_PROCESS}
        };
        int sizes[MAX_BLOCKS];
        int starts[MAX_BLOCKS];
        BlockState states[MAX_BLOCKS];
        int process_ids[MAX_BLOCKS];
        size_t count;

        TEST_CHECK(build_layout(&memory, 300, specs, 3),
                   "montagem do cenario");
        count = snapshot(&memory, sizes, starts, states, process_ids,
                         MAX_BLOCKS);
        TEST_CHECK_EQ_INT(count, 3, "tres blocos no snapshot");

        /* Ha 200 KB livres, mas nenhum bloco contiguo de 150 KB. */
        TEST_CHECK_EQ_INT(allocation_run((AllocationStrategy)strategy, &memory,
                                         5, 150, &result),
                          0, "tentativa processada");
        TEST_CHECK_EQ_INT(result.success, 0, "alocacao deve falhar");
        TEST_CHECK_EQ_INT(result.status, MEMORY_ERROR_NO_SPACE,
                          "status sem espaco");
        TEST_CHECK_EQ_INT(result.blocks_examined, 3,
                          "todos os blocos percorridos");
        TEST_CHECK_EQ_INT(memory.block_count, 3,
                          "quantidade de blocos inalterada");
        TEST_CHECK(snapshot_matches(&memory, sizes, starts, states, process_ids,
                                    count),
                   "geometria e processos inalterados");
        TEST_CHECK_MEMORY_VALID(&memory, "apos falha por fragmentacao");

        memory_destroy(&memory);
    }
}

static void test_request_larger_than_total(void)
{
    int strategy;

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        Memory memory;
        AllocationResult result;
        BlockSpec specs[] = {
            {100, 1},
            {200, NO_PROCESS}
        };

        TEST_CHECK(build_layout(&memory, 300, specs, 2),
                   "montagem do cenario");

        TEST_CHECK_EQ_INT(allocation_run((AllocationStrategy)strategy, &memory,
                                         2, 301, &result),
                          0, "tentativa processada");
        TEST_CHECK_EQ_INT(result.success, 0, "alocacao deve falhar");
        TEST_CHECK_EQ_INT(result.status, MEMORY_ERROR_NO_SPACE,
                          "status sem espaco");
        TEST_CHECK_EQ_INT(result.blocks_examined, 0,
                          "pedido maior que a memoria total: nenhuma busca");
        TEST_CHECK_EQ_INT(memory.block_count, 2,
                          "quantidade de blocos inalterada");
        TEST_CHECK_EQ_INT(memory_total_free(&memory), 200,
                          "memoria livre inalterada");
        TEST_CHECK_MEMORY_VALID(&memory, "apos falha por tamanho excessivo");

        memory_destroy(&memory);
    }
}

static void test_process_already_allocated(void)
{
    int strategy;

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        Memory memory;
        AllocationResult result;
        BlockSpec specs[] = {
            {100, 7},
            {300, NO_PROCESS}
        };

        TEST_CHECK(build_layout(&memory, 400, specs, 2),
                   "montagem do cenario");

        TEST_CHECK_EQ_INT(allocation_run((AllocationStrategy)strategy, &memory,
                                         7, 50, &result),
                          0, "tentativa processada");
        TEST_CHECK_EQ_INT(result.success, 0, "alocacao deve falhar");
        TEST_CHECK_EQ_INT(result.status, MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED,
                          "processo ja alocado");
        TEST_CHECK_EQ_INT(result.blocks_examined, 0,
                          "processo ja alocado: nenhuma busca");
        TEST_CHECK_EQ_INT(memory.block_count, 2,
                          "quantidade de blocos inalterada");
        TEST_CHECK_MEMORY_VALID(&memory, "apos falha por processo duplicado");

        memory_destroy(&memory);
    }
}

static void test_blocks_examined_convention(void)
{
    BlockSpec specs[] = {
        {100, 1},
        {100, NO_PROCESS},
        {100, 2},
        {300, NO_PROCESS},
        {100, 3}
    };
    Memory first;
    Memory best;
    Memory worst;
    AllocationResult result;

    TEST_CHECK(build_layout(&first, 700, specs, 5), "montagem First Fit");
    TEST_CHECK(build_layout(&best, 700, specs, 5), "montagem Best Fit");
    TEST_CHECK(build_layout(&worst, 700, specs, 5), "montagem Worst Fit");

    TEST_CHECK_EQ_INT(allocation_first_fit(&first, 9, 80, &result), 0,
                      "tentativa First Fit");
    TEST_CHECK_EQ_INT(result.success, 1, "First Fit deve alocar");
    TEST_CHECK_EQ_INT(result.start_address, 100, "First Fit para no bloco 2");
    TEST_CHECK_EQ_INT(result.blocks_examined, 2, "First Fit conta ate o bloco");
    TEST_CHECK(result.blocks_examined < (int)first.block_count,
               "First Fit examina menos que o total");

    TEST_CHECK_EQ_INT(allocation_best_fit(&best, 9, 80, &result), 0,
                      "tentativa Best Fit");
    TEST_CHECK_EQ_INT(result.success, 1, "Best Fit deve alocar");
    TEST_CHECK_EQ_INT(result.start_address, 100, "Best Fit escolhe 100 em 100");
    TEST_CHECK_EQ_INT(result.blocks_examined, 5, "Best Fit examina todos");

    TEST_CHECK_EQ_INT(allocation_worst_fit(&worst, 9, 80, &result), 0,
                      "tentativa Worst Fit");
    TEST_CHECK_EQ_INT(result.success, 1, "Worst Fit deve alocar");
    TEST_CHECK_EQ_INT(result.start_address, 300, "Worst Fit escolhe 300 em 300");
    TEST_CHECK_EQ_INT(result.blocks_examined, 5, "Worst Fit examina todos");

    TEST_CHECK_MEMORY_VALID(&first, "First Fit integra");
    TEST_CHECK_MEMORY_VALID(&best, "Best Fit integra");
    TEST_CHECK_MEMORY_VALID(&worst, "Worst Fit integra");

    memory_destroy(&first);
    memory_destroy(&best);
    memory_destroy(&worst);
}

static void test_same_layout_three_independent_simulations(void)
{
    BlockSpec specs[] = {
        {400, NO_PROCESS},
        {200, 1},
        {200, NO_PROCESS},
        {200, 2}
    };
    Memory first;
    Memory best;
    Memory worst;
    AllocationResult result_a;
    AllocationResult result_b;
    AllocationResult result_c;

    TEST_CHECK(build_layout(&first, 1000, specs, 4), "montagem First Fit");
    TEST_CHECK(build_layout(&best, 1000, specs, 4), "montagem Best Fit");
    TEST_CHECK(build_layout(&worst, 1000, specs, 4), "montagem Worst Fit");

    /* Mesma sequencia, aplicada apenas ao First Fit: os outros nao mudam. */
    TEST_CHECK_EQ_INT(allocation_first_fit(&first, 10, 100, &result_a), 0,
                      "First Fit: alocacao inicial");
    TEST_CHECK_EQ_INT(result_a.start_address, 0, "First Fit escolhe 400 em 0");
    TEST_CHECK_EQ_INT(memory_total_free(&best), 600,
                      "Best Fit permanece independente");
    TEST_CHECK_EQ_INT(memory_total_free(&worst), 600,
                      "Worst Fit permanece independente");
    TEST_CHECK_EQ_INT(best.block_count, 4, "Best Fit com geometria original");

    TEST_CHECK_EQ_INT(allocation_best_fit(&best, 10, 100, &result_b), 0,
                      "Best Fit: alocacao inicial");
    TEST_CHECK_EQ_INT(result_b.start_address, 600, "Best Fit escolhe 200 em 600");
    TEST_CHECK_EQ_INT(memory_total_free(&worst), 600,
                      "Worst Fit permanece independente");

    TEST_CHECK_EQ_INT(allocation_worst_fit(&worst, 10, 100, &result_c), 0,
                      "Worst Fit: alocacao inicial");
    TEST_CHECK_EQ_INT(result_c.start_address, 0, "Worst Fit escolhe 400 em 0");

    /* Libera o processo 1 nas tres memorias (coalescencia pela primitiva). */
    TEST_CHECK_EQ_INT(memory_free(&first, 1), MEMORY_OK, "free First Fit");
    TEST_CHECK_EQ_INT(memory_free(&best, 1), MEMORY_OK, "free Best Fit");
    TEST_CHECK_EQ_INT(memory_free(&worst, 1), MEMORY_OK, "free Worst Fit");

    TEST_CHECK_EQ_INT(memory_total_free(&first), 700, "livre First Fit");
    TEST_CHECK_EQ_INT(memory_total_free(&best), 700, "livre Best Fit");
    TEST_CHECK_EQ_INT(memory_total_free(&worst), 700, "livre Worst Fit");

    /* Segunda alocacao identica: cada estrategia decide de forma coerente. */
    TEST_CHECK_EQ_INT(allocation_first_fit(&first, 11, 250, &result_a), 0,
                      "First Fit: segunda alocacao");
    TEST_CHECK_EQ_INT(result_a.start_address, 100, "First Fit usa livre em 100");

    TEST_CHECK_EQ_INT(allocation_best_fit(&best, 11, 250, &result_b), 0,
                      "Best Fit: segunda alocacao");
    TEST_CHECK_EQ_INT(result_b.start_address, 0, "Best Fit usa livre em 0");

    TEST_CHECK_EQ_INT(allocation_worst_fit(&worst, 11, 250, &result_c), 0,
                      "Worst Fit: segunda alocacao");
    TEST_CHECK_EQ_INT(result_c.start_address, 100, "Worst Fit usa livre em 100");

    TEST_CHECK_EQ_INT(result_a.success, 1, "First Fit coerente");
    TEST_CHECK_EQ_INT(result_b.success, 1, "Best Fit coerente");
    TEST_CHECK_EQ_INT(result_c.success, 1, "Worst Fit coerente");

    TEST_CHECK_MEMORY_VALID(&first, "First Fit integra ao final");
    TEST_CHECK_MEMORY_VALID(&best, "Best Fit integra ao final");
    TEST_CHECK_MEMORY_VALID(&worst, "Worst Fit integra ao final");

    memory_destroy(&first);
    memory_destroy(&best);
    memory_destroy(&worst);
}

static void test_invalid_arguments(void)
{
    Memory memory;
    AllocationResult result;
    BlockSpec specs[] = {
        {500, NO_PROCESS}
    };

    TEST_CHECK(build_layout(&memory, 500, specs, 1), "montagem do cenario");

    TEST_CHECK_EQ_INT(allocation_first_fit(NULL, 1, 10, &result), -1,
                      "memoria nula");
    TEST_CHECK_EQ_INT(allocation_first_fit(&memory, 1, 10, NULL), -1,
                      "resultado nulo");
    TEST_CHECK_EQ_INT(allocation_best_fit(&memory, 1, 0, &result), -1,
                      "tamanho zero");
    TEST_CHECK_EQ_INT(allocation_worst_fit(&memory, -1, 10, &result), -1,
                      "processo negativo");
    TEST_CHECK_EQ_INT(allocation_run(STRATEGY_COUNT, &memory, 1, 10, &result),
                      -1, "estrategia desconhecida");
    TEST_CHECK_EQ_INT(memory.block_count, 1,
                      "memoria inalterada por argumentos invalidos");
    TEST_CHECK_MEMORY_VALID(&memory, "apos argumentos invalidos");

    memory_destroy(&memory);
}

static void test_strategy_labels(void)
{
    TEST_CHECK_EQ_STR(allocation_strategy_name(STRATEGY_FIRST_FIT), "First Fit",
                      "nome First Fit");
    TEST_CHECK_EQ_STR(allocation_strategy_name(STRATEGY_BEST_FIT), "Best Fit",
                      "nome Best Fit");
    TEST_CHECK_EQ_STR(allocation_strategy_name(STRATEGY_WORST_FIT), "Worst Fit",
                      "nome Worst Fit");
    TEST_CHECK(allocation_strategy_description(STRATEGY_FIRST_FIT) != NULL,
               "descricao First Fit");
    TEST_CHECK(allocation_strategy_description(STRATEGY_BEST_FIT) != NULL,
               "descricao Best Fit");
    TEST_CHECK(allocation_strategy_description(STRATEGY_WORST_FIT) != NULL,
               "descricao Worst Fit");
}

int main(void)
{
    TEST_RUN(test_first_fit_picks_first_adequate);
    TEST_RUN(test_best_fit_picks_smallest_adequate);
    TEST_RUN(test_best_fit_tie_uses_lowest_address);
    TEST_RUN(test_worst_fit_picks_largest_adequate);
    TEST_RUN(test_worst_fit_tie_uses_lowest_address);
    TEST_RUN(test_failure_external_fragmentation_keeps_memory);
    TEST_RUN(test_request_larger_than_total);
    TEST_RUN(test_process_already_allocated);
    TEST_RUN(test_blocks_examined_convention);
    TEST_RUN(test_same_layout_three_independent_simulations);
    TEST_RUN(test_invalid_arguments);
    TEST_RUN(test_strategy_labels);

    printf("  Checks: %d, falhas: %d\n", test_checks, test_failures);
    return TEST_SUMMARY();
}
