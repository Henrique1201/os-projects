/*
 * allocation.c - Estrategias de escolha de bloco livre (particoes variaveis).
 *
 * As tres estrategias (First Fit, Best Fit e Worst Fit) NAO sao simuladores
 * independentes: todas percorrem a MESMA lista de blocos de Memory e, ao
 * escolherem um bloco, usam a MESMA primitiva de divisao
 * memory_allocate_in_block(). O que muda e apenas o criterio de selecao.
 *
 * CONVENCAO DE CONTAGEM (blocks_examined)
 * "blocks_examined" e a quantidade de blocos efetivamente PERCORRIDOS pela
 * busca, incluindo blocos ocupados atravessados antes do bloco escolhido.
 *   - First Fit interrompe a busca no primeiro bloco livre adequado; logo,
 *     blocos posteriores a esse nao sao contados.
 *   - Best Fit e Worst Fit precisam comparar todos os candidatos; por isso
 *     examinam a lista inteira.
 *   - Falhas sem bloco adequado contam todos os blocos percorridos ate o fim
 *     da lista (ou zero, nos casos de curto-circuito descritos abaixo).
 *
 * EMPATES
 * A lista esta em ordem de endereco. Best Fit e Worst Fit trocam o candidato
 * apenas quando o novo bloco e estritamente menor/maior; em caso de tamanhos
 * iguais, o primeiro encontrado (menor start_address) permanece escolhido.
 */
#include "allocation.h"

/*
 * Executa uma tentativa de alocacao aplicando o criterio da estrategia.
 * Devolve 0 quando a tentativa foi processada (sucesso ou falha registrada em
 * result) e -1 para argumentos invalidos (sem tocar na memoria).
 */
static int allocation_execute(AllocationStrategy strategy, Memory *memory,
                              int process_id, int requested_size,
                              AllocationResult *result)
{
    MemoryBlock *block;
    MemoryBlock *chosen = NULL;
    int examined = 0;
    int start_address;
    MemoryStatus status;

    if (memory == NULL || result == NULL || requested_size <= 0 ||
        process_id < 0) {
        return -1;
    }

    result->success = 0;
    result->start_address = 0;
    result->allocated_size = 0;
    result->blocks_examined = 0;
    result->status = MEMORY_OK;

    /*
     * Um processo nao pode ocupar dois blocos ao mesmo tempo. Essa verificacao
     * dispensa qualquer busca, por isso blocks_examined permanece 0.
     */
    if (memory_find_process(memory, process_id) != NULL) {
        result->status = MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED;
        return 0;
    }

    /*
     * Solicitacao maior que a regiao inteira jamais sera atendida, mesmo que a
     * memoria estivesse totalmente livre. Curto-circuito: nenhuma busca.
     */
    if (requested_size > memory->total_size) {
        result->status = MEMORY_ERROR_NO_SPACE;
        return 0;
    }

    /*
     * Busca sequencial na ordem de endereco. O laco sempre conta cada bloco
     * visitado; First Fit pode sair cedo, Best/Worst Fit comparam todos os
     * blocos livres adequados para escolher o menor/maior.
     */
    for (block = memory->head; block != NULL; block = block->next) {
        examined++;

        if (!memory_block_is_free(block) || block->size < requested_size) {
            continue;
        }

        if (strategy == STRATEGY_FIRST_FIT) {
            chosen = block;
            break;
        }

        if (chosen == NULL) {
            chosen = block;
        } else if (strategy == STRATEGY_BEST_FIT &&
                   block->size < chosen->size) {
            chosen = block;
        } else if (strategy == STRATEGY_WORST_FIT &&
                   block->size > chosen->size) {
            chosen = block;
        }
        /* Tamanho igual: mantem o candidato anterior (menor endereco). */
    }

    result->blocks_examined = examined;

    if (chosen == NULL) {
        result->status = MEMORY_ERROR_NO_SPACE;
        return 0;
    }

    /* O bloco escolhido e sempre dividido pela primitiva unica do modelo. */
    start_address = chosen->start_address;
    status = memory_allocate_in_block(memory, chosen, process_id,
                                      requested_size);
    result->status = status;

    if (status != MEMORY_OK) {
        return 0;
    }

    result->success = 1;
    result->start_address = start_address;
    result->allocated_size = requested_size;
    return 0;
}

int allocation_run(AllocationStrategy strategy, Memory *memory, int process_id,
                   int requested_size, AllocationResult *result)
{
    switch (strategy) {
    case STRATEGY_FIRST_FIT:
        return allocation_first_fit(memory, process_id, requested_size, result);
    case STRATEGY_BEST_FIT:
        return allocation_best_fit(memory, process_id, requested_size, result);
    case STRATEGY_WORST_FIT:
        return allocation_worst_fit(memory, process_id, requested_size,
                                    result);
    default:
        /* Estrategia desconhecida: argumento invalido, memoria inalterada. */
        return -1;
    }
}

int allocation_first_fit(Memory *memory, int process_id, int requested_size,
                         AllocationResult *result)
{
    return allocation_execute(STRATEGY_FIRST_FIT, memory, process_id,
                              requested_size, result);
}

int allocation_best_fit(Memory *memory, int process_id, int requested_size,
                        AllocationResult *result)
{
    return allocation_execute(STRATEGY_BEST_FIT, memory, process_id,
                              requested_size, result);
}

int allocation_worst_fit(Memory *memory, int process_id, int requested_size,
                         AllocationResult *result)
{
    return allocation_execute(STRATEGY_WORST_FIT, memory, process_id,
                              requested_size, result);
}

const char *allocation_strategy_name(AllocationStrategy strategy)
{
    switch (strategy) {
    case STRATEGY_FIRST_FIT:
        return "First Fit";
    case STRATEGY_BEST_FIT:
        return "Best Fit";
    case STRATEGY_WORST_FIT:
        return "Worst Fit";
    default:
        return NULL;
    }
}

const char *allocation_strategy_description(AllocationStrategy strategy)
{
    switch (strategy) {
    case STRATEGY_FIRST_FIT:
        return "Percorre os blocos em ordem de endereco e escolhe o primeiro "
               "bloco livre com tamanho suficiente. Busca linear O(n), "
               "interrompida assim que um bloco adequado e encontrado.";
    case STRATEGY_BEST_FIT:
        return "Percorre todos os blocos e escolhe o menor bloco livre com "
               "tamanho suficiente, minimizando o desperdicio imediato. Busca "
               "linear O(n) com exame de todos os blocos; empates sao "
               "resolvidos pelo menor endereco inicial.";
    case STRATEGY_WORST_FIT:
        return "Percorre todos os blocos e escolhe o maior bloco livre com "
               "tamanho suficiente, deixando o maior remanescente possivel "
               "para futuras alocacoes. Busca linear O(n) com exame de todos "
               "os blocos; empates sao resolvidos pelo menor endereco "
               "inicial.";
    default:
        return NULL;
    }
}
