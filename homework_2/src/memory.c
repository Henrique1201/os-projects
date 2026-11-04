/*
 * memory.c - Modelo de memoria fisica simulada com particoes variaveis.
 *
 * A regiao simulada e uma lista simplesmente encadeada de blocos em ordem
 * crescente de endereco. Cada no representa uma particao (livre ou ocupada).
 * A soma dos tamanhos e sempre igual a total_size e os enderecos sao
 * contiguos a partir de zero; essa e a invariante central do modelo.
 *
 * Operacoes principais:
 *   - alocar: divide um bloco livre (particoes variaveis);
 *   - liberar: devolve o bloco ao estado livre e coalesce vizinhos;
 *   - validar: confere as invariantes e os contadores.
 *
 * Os contadores (block_count, free_block_count, occupied_block_count) sao
 * recalculados a partir da lista apos cada mutacao, o que elimina a
 * possibilidade de divergencia entre a lista e os campos auxiliares.
 */

#include "memory.h"

#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
/* Funcoes auxiliares internas                                               */
/* ------------------------------------------------------------------------- */

/* Libera todos os nos da lista encadeada (nao mexe nos campos da Memory). */
static void free_block_list(MemoryBlock *block)
{
    while (block != NULL) {
        MemoryBlock *next = block->next;
        free(block);
        block = next;
    }
}

/* Cria um unico bloco com os campos informados (next = NULL). */
static MemoryBlock *create_block(int start_address, int size, int process_id,
                                 BlockState state)
{
    MemoryBlock *block = malloc(sizeof(*block));

    if (block == NULL) {
        return NULL;
    }

    block->process_id = process_id;
    block->start_address = start_address;
    block->size = size;
    block->state = state;
    block->next = NULL;
    return block;
}

/* Recalcula os contadores a partir da lista, mantendo-os sempre coerentes. */
static void recount_blocks(Memory *memory)
{
    size_t total = 0;
    size_t free_count = 0;
    size_t occupied_count = 0;
    const MemoryBlock *block;

    for (block = memory->head; block != NULL; block = block->next) {
        total++;
        if (block->state == BLOCK_FREE) {
            free_count++;
        } else if (block->state == BLOCK_OCCUPIED) {
            occupied_count++;
        }
    }

    memory->block_count = total;
    memory->free_block_count = free_count;
    memory->occupied_block_count = occupied_count;
}

/*
 * Copia uma mensagem didatica para o buffer informado, respeitando o limite.
 * Nunca escreve fora de message[0 .. message_size - 1].
 */
static int set_message(char *message, size_t message_size, const char *text)
{
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size, "%s", text);
    }
    return 0;
}

/*
 * Mensagem especifica da violacao de adjacencia entre blocos livres.
 * O endereco informado e o inicio do segundo bloco livre, ponto em que os
 * dois blocos se tocam. snprintf respeita message_size em qualquer caminho.
 */
static int set_adjacent_free_message(char *message, size_t message_size,
                                     int address)
{
    if (message != NULL && message_size > 0) {
        snprintf(message, message_size,
                 "dois blocos livres adjacentes em %d KB", address);
    }
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Ciclo de vida                                                             */
/* ------------------------------------------------------------------------- */

int memory_init(Memory *memory, int total_size)
{
    MemoryBlock *block;

    if (memory == NULL) {
        return 0;
    }

    /* Estado neutro: garante que uma falha nao deixe campos indefinidos. */
    memory->total_size = 0;
    memory->head = NULL;
    memory->block_count = 0;
    memory->free_block_count = 0;
    memory->occupied_block_count = 0;

    if (total_size <= 0) {
        return 0;
    }

    block = create_block(0, total_size, NO_PROCESS, BLOCK_FREE);
    if (block == NULL) {
        return 0;
    }

    memory->total_size = total_size;
    memory->head = block;
    memory->block_count = 1;
    memory->free_block_count = 1;
    memory->occupied_block_count = 0;
    return 1;
}

void memory_destroy(Memory *memory)
{
    if (memory == NULL) {
        return;
    }

    free_block_list(memory->head);
    memory->head = NULL;
    memory->total_size = 0;
    memory->block_count = 0;
    memory->free_block_count = 0;
    memory->occupied_block_count = 0;
}

int memory_reset(Memory *memory)
{
    MemoryBlock *block;
    int total_size;

    if (memory == NULL) {
        return 0;
    }

    if (memory->total_size <= 0) {
        /* Nao ha regiao valida para reconstruir: apenas limpa a estrutura. */
        free_block_list(memory->head);
        memory->head = NULL;
        memory->total_size = 0;
        memory->block_count = 0;
        memory->free_block_count = 0;
        memory->occupied_block_count = 0;
        return 0;
    }

    total_size = memory->total_size;
    block = create_block(0, total_size, NO_PROCESS, BLOCK_FREE);
    if (block == NULL) {
        /* Falha de alocacao: preserva a estrutura antiga intacta. */
        return 0;
    }

    free_block_list(memory->head);
    memory->head = block;
    memory->block_count = 1;
    memory->free_block_count = 1;
    memory->occupied_block_count = 0;
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Alocacao (divisao de blocos livres)                                       */
/* ------------------------------------------------------------------------- */

MemoryStatus memory_allocate_in_block(Memory *memory, MemoryBlock *block,
                                      int process_id, int size)
{
    MemoryBlock *current;
    MemoryBlock *found = NULL;
    MemoryBlock *remainder;

    if (memory == NULL || block == NULL) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }
    if (process_id < 0 || size <= 0) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }

    /* Um processo nao pode ocupar dois blocos ao mesmo tempo. */
    if (memory_find_process(memory, process_id) != NULL) {
        return MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED;
    }

    /*
     * O bloco precisa pertencer a esta lista. A verificacao por percorrimento
     * evita aceitar ponteiros arbitrarios e impede corromper a lista.
     */
    for (current = memory->head; current != NULL; current = current->next) {
        if (current == block) {
            found = current;
            break;
        }
    }
    if (found == NULL) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }

    if (!memory_block_is_free(found)) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }

    if (size > found->size) {
        return MEMORY_ERROR_NO_SPACE;
    }

    /*
     * Divisao do bloco (particoes variaveis): o inicio vira a particao
     * ocupada e o restante continua livre. Se o tamanho for exatamente o
     * solicitado, nao ha remanescente e o proprio bloco e ocupado.
     */
    if (size < found->size) {
        remainder = create_block(found->start_address + size,
                                 found->size - size, NO_PROCESS, BLOCK_FREE);
        if (remainder == NULL) {
            return MEMORY_ERROR_OUT_OF_MEMORY;
        }
        remainder->next = found->next;
        found->next = remainder;
    }

    found->size = size;
    found->process_id = process_id;
    found->state = BLOCK_OCCUPIED;

    recount_blocks(memory);
    return MEMORY_OK;
}

MemoryStatus memory_allocate(Memory *memory, int process_id, int size)
{
    MemoryBlock *block;

    if (memory == NULL) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }
    if (process_id < 0 || size <= 0) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }

    if (memory_find_process(memory, process_id) != NULL) {
        return MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED;
    }

    /* Rejeicao antecipada: nenhum bloco poderia atender um pedido tao grande. */
    if (size > memory->total_size) {
        return MEMORY_ERROR_NO_SPACE;
    }

    /*
     * First Fit: primeiro bloco livre, em ordem de endereco, com tamanho
     * suficiente. A escolha em si e propositalmente simples nesta etapa;
     * as estrategias comparadas ficam em src/allocation.c.
     */
    for (block = memory->head; block != NULL; block = block->next) {
        if (memory_block_is_free(block) && block->size >= size) {
            return memory_allocate_in_block(memory, block, process_id, size);
        }
    }

    return MEMORY_ERROR_NO_SPACE;
}

/* ------------------------------------------------------------------------- */
/* Liberacao com coalescencia                                                */
/* ------------------------------------------------------------------------- */

MemoryStatus memory_free(Memory *memory, int process_id)
{
    MemoryBlock *previous = NULL;
    MemoryBlock *current;

    if (memory == NULL) {
        return MEMORY_ERROR_INVALID_ARGUMENT;
    }

    if (process_id < 0) {
        return MEMORY_ERROR_PROCESS_NOT_FOUND;
    }

    for (current = memory->head; current != NULL; current = current->next) {
        if (current->state == BLOCK_OCCUPIED &&
            current->process_id == process_id) {
            break;
        }
        previous = current;
    }

    if (current == NULL) {
        return MEMORY_ERROR_PROCESS_NOT_FOUND;
    }

    current->process_id = NO_PROCESS;
    current->state = BLOCK_FREE;

    /*
     * Coalescencia imediata: primeiro com o sucessor livre, depois com o
     * predecessor livre. Ao final, blocos livres adjacentes nunca ficam
     * separados, o que reduz a fragmentacao externa.
     */
    if (memory_block_is_free(current->next)) {
        MemoryBlock *next = current->next;

        current->size += next->size;
        current->next = next->next;
        free(next);
    }

    if (memory_block_is_free(previous)) {
        previous->size += current->size;
        previous->next = current->next;
        free(current);
    }

    recount_blocks(memory);
    return MEMORY_OK;
}

/* ------------------------------------------------------------------------- */
/* Consultas                                                                 */
/* ------------------------------------------------------------------------- */

/*
 * Predicado unico de bloco livre. Conceitualmente, um bloco livre e uma
 * regiao sem processo associado: o estado BLOCK_FREE e o identificador
 * NO_PROCESS representam o mesmo fato e NAO podem divergir. Centralizar essa
 * definicao evita que outros modulos (por exemplo, a escolha de bloco em
 * src/allocation.c) implementem uma nocao parcial ou conflitante de "livre".
 */
int memory_block_is_free(const MemoryBlock *block)
{
    return block != NULL && block->state == BLOCK_FREE &&
           block->process_id == NO_PROCESS;
}

const MemoryBlock *memory_find_process(const Memory *memory, int process_id)
{
    const MemoryBlock *block;

    if (memory == NULL || process_id < 0) {
        return NULL;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_OCCUPIED &&
            block->process_id == process_id) {
            return block;
        }
    }

    return NULL;
}

int memory_total_free(const Memory *memory)
{
    const MemoryBlock *block;
    int total = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_FREE) {
            total += block->size;
        }
    }

    return total;
}

int memory_total_used(const Memory *memory)
{
    const MemoryBlock *block;
    int total = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_OCCUPIED) {
            total += block->size;
        }
    }

    return total;
}

int memory_largest_free_block(const Memory *memory)
{
    const MemoryBlock *block;
    int largest = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_FREE && block->size > largest) {
            largest = block->size;
        }
    }

    return largest;
}

int memory_count_free_blocks(const Memory *memory)
{
    const MemoryBlock *block;
    int count = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_FREE) {
            count++;
        }
    }

    return count;
}

int memory_count_occupied_blocks(const Memory *memory)
{
    const MemoryBlock *block;
    int count = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (block->state == BLOCK_OCCUPIED) {
            count++;
        }
    }

    return count;
}

size_t memory_block_count(const Memory *memory)
{
    const MemoryBlock *block;
    size_t count = 0;

    if (memory == NULL) {
        return 0;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        count++;
    }

    return count;
}

const MemoryBlock *memory_block_at(const Memory *memory, size_t index)
{
    const MemoryBlock *block;
    size_t current_index = 0;

    if (memory == NULL) {
        return NULL;
    }

    for (block = memory->head; block != NULL; block = block->next) {
        if (current_index == index) {
            return block;
        }
        current_index++;
    }

    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Validacao                                                                 */
/* ------------------------------------------------------------------------- */

int memory_validate_ex(const Memory *memory, char *message, size_t message_size)
{
    const MemoryBlock *block;
    long long expected_address = 0;
    size_t total = 0;
    size_t free_count = 0;
    size_t occupied_count = 0;
    int previous_free = 0;

    if (message != NULL && message_size > 0) {
        message[0] = '\0';
    }

    if (memory == NULL) {
        return set_message(message, message_size, "ponteiro de memoria nulo");
    }

    if (memory->total_size < 0) {
        return set_message(message, message_size, "tamanho total negativo");
    }

    for (block = memory->head; block != NULL; block = block->next) {
        total++;

        if (block->size <= 0) {
            return set_message(message, message_size,
                               "bloco com tamanho invalido");
        }

        /* Contiguidade: cada bloco deve comecar onde o anterior terminou. */
        if ((long long)block->start_address != expected_address) {
            return set_message(message, message_size,
                               "lacuna ou sobreposicao de enderecos");
        }

        if (block->state == BLOCK_FREE) {
            if (block->process_id != NO_PROCESS) {
                return set_message(message, message_size,
                                   "bloco livre com processo associado");
            }
            free_count++;
        } else if (block->state == BLOCK_OCCUPIED) {
            const MemoryBlock *other;

            if (block->process_id < 0) {
                return set_message(message, message_size,
                                   "bloco ocupado sem processo");
            }

            /* Processos duplicados: compara com todos os blocos anteriores. */
            for (other = memory->head; other != block; other = other->next) {
                if (other->state == BLOCK_OCCUPIED &&
                    other->process_id == block->process_id) {
                    return set_message(message, message_size,
                                       "processo duplicado em blocos diferentes");
                }
            }
            occupied_count++;
        } else {
            return set_message(message, message_size, "estado de bloco invalido");
        }

        /*
         * Coalescencia imediata: dois blocos livres consecutivos significam
         * que a uniao deixou de ser feita. Estados validos nunca apresentam
         * essa adjacencia, portanto detecta-la aqui expoe coalescencia
         * quebrada em vez de apenas aceitar o estado como consistente.
         */
        if (previous_free && memory_block_is_free(block)) {
            return set_adjacent_free_message(message, message_size,
                                             block->start_address);
        }
        previous_free = memory_block_is_free(block);

        expected_address += block->size;
    }

    if (expected_address != (long long)memory->total_size) {
        return set_message(message, message_size,
                           "soma dos tamanhos difere do total");
    }

    if (memory->block_count != total) {
        return set_message(message, message_size,
                           "contador block_count inconsistente");
    }

    if (memory->free_block_count != free_count) {
        return set_message(message, message_size,
                           "contador free_block_count inconsistente");
    }

    if (memory->occupied_block_count != occupied_count) {
        return set_message(message, message_size,
                           "contador occupied_block_count inconsistente");
    }

    if (free_count + occupied_count != total) {
        return set_message(message, message_size,
                           "contadores de estado inconsistentes");
    }

    set_message(message, message_size, "memoria integra");
    return 1;
}

int memory_validate(const Memory *memory)
{
    return memory_validate_ex(memory, NULL, 0);
}

/* ------------------------------------------------------------------------- */
/* Mensagens didaticas                                                       */
/* ------------------------------------------------------------------------- */

const char *memory_status_message(MemoryStatus status)
{
    switch (status) {
    case MEMORY_OK:
        return "operacao concluida com sucesso";
    case MEMORY_ERROR_INVALID_ARGUMENT:
        return "argumento invalido";
    case MEMORY_ERROR_NO_SPACE:
        return "nao ha espaco contiguo suficiente";
    case MEMORY_ERROR_PROCESS_NOT_FOUND:
        return "processo nao encontrado na memoria";
    case MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED:
        return "processo ja alocado na memoria";
    case MEMORY_ERROR_OUT_OF_MEMORY:
        return "falha ao alocar memoria interna";
    default:
        return "status desconhecido";
    }
}
