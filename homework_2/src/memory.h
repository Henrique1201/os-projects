/*
 * memory.h - Modelo de memoria fisica simulada com particoes variaveis.
 *
 * CONCEITO TEORICO
 * Uma regiao contigua de memoria fisica e mantida como uma lista encadeada de
 * blocos. Cada bloco e OU livre (nenhum processo associado) OU ocupado
 * (reservado por um unico processo). Os enderecos sao contiguos, sem
 * sobreposicao, e a soma dos tamanhos dos blocos e sempre igual ao tamanho
 * total da memoria simulada.
 *
 * O divisor do bloco livre ao alocar representa a alocacao de particoes
 * variaveis: o bloco restante volta a ser uma regiao livre. A liberacao com
 * coalescencia representa o reaproveitamento de regioes de memoria.
 *
 * RESPONSABILIDADE DESTE MODULO
 * Fornecer as primitivas do "sistema operacional" simulado:
 *   - inicializar a regiao totalmente livre;
 *   - dividir um bloco livre (alocar);
 *   - liberar a regiao de um processo;
 *   - unir (coalescer) blocos livres adjacentes;
 *   - validar a integridade do modelo.
 *
 * Este modulo NAO conhece os algoritmos de escolha (First/Best/Worst Fit).
 * A busca de um bloco adequado pertence a src/allocation.c, que utiliza as
 * primitivas declaradas aqui.
 *
 * CONVENCOES
 *   - Todas as medidas estao em KB.
 *   - Os enderecos sao offsets, em KB, a partir do inicio da regiao simulada.
 *   - Um bloco livre usa o identificador NO_PROCESS.
 *   - Um processo nao pode ocupar dois blocos diferentes ao mesmo tempo.
 */
#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

/* Identificador reservado: blocos livres nao possuem processo associado. */
#define NO_PROCESS (-1)

/* Estado de um bloco da regiao simulada. */
typedef enum {
    BLOCK_FREE = 0,
    BLOCK_OCCUPIED = 1
} BlockState;

/* Bloco da regiao de memoria (parcela livre ou ocupada). */
typedef struct MemoryBlock {
    int process_id;            /* NO_PROCESS quando o bloco esta livre */
    int start_address;         /* endereco inicial do bloco, em KB */
    int size;                  /* tamanho do bloco, em KB */
    BlockState state;          /* BLOCK_FREE ou BLOCK_OCCUPIED */
    struct MemoryBlock *next;  /* proximo bloco, em ordem de endereco */
} MemoryBlock;

/* Regiao de memoria simulada: lista de blocos + contadores auxiliares. */
typedef struct {
    int total_size;              /* tamanho total da memoria, em KB */
    MemoryBlock *head;           /* primeiro bloco da lista */
    size_t block_count;          /* total de blocos (livres + ocupados) */
    size_t free_block_count;     /* quantidade de blocos livres */
    size_t occupied_block_count; /* quantidade de blocos ocupados */
} Memory;

/* Codigos de retorno das operacoes do modulo de memoria. */
typedef enum {
    MEMORY_OK = 0,
    MEMORY_ERROR_INVALID_ARGUMENT = -1,
    MEMORY_ERROR_NO_SPACE = -2,
    MEMORY_ERROR_PROCESS_NOT_FOUND = -3,
    MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED = -4,
    MEMORY_ERROR_OUT_OF_MEMORY = -5
} MemoryStatus;

/* Mensagem didatica (sem acentos) associada a um codigo de status. */
const char *memory_status_message(MemoryStatus status);

/*
 * memory_init - Cria a regiao totalmente livre (um unico bloco livre).
 * Retorna 1 em caso de sucesso e 0 em caso de falha (argumentos invalidos
 * ou falta de memoria para alocar o bloco inicial).
 */
int memory_init(Memory *memory, int total_size);

/* memory_destroy - Libera todos os blocos e devolve a estrutura ao estado neutro. */
void memory_destroy(Memory *memory);

/* memory_reset - Reconstroi a regiao totalmente livre, preservando o tamanho. */
int memory_reset(Memory *memory);

/*
 * memory_allocate - Alocacao basica: usa o PRIMEIRO bloco livre com tamanho
 * suficiente (equivalente ao First Fit). Atalho util para testes e para a
 * etapa de gerenciamento basico; as tres estrategias comparadas ficam em
 * src/allocation.c.
 * Devolve MEMORY_ERROR_NO_SPACE se a memoria livre total for insuficiente ou
 * se nao houver bloco contiguo com tamanho adequado, sem alterar o estado.
 */
MemoryStatus memory_allocate(Memory *memory, int process_id, int size);

/*
 * memory_allocate_in_block - Primitiva de divisao: ocupa os primeiros "size"
 * KB de um bloco livre especifico. Se o bloco for maior que o solicitado, o
 * remanescente volta a ser um bloco livre; se for do mesmo tamanho, o bloco
 * e removido da lista.
 */
MemoryStatus memory_allocate_in_block(Memory *memory, MemoryBlock *block,
                                      int process_id, int size);

/*
 * memory_free - Libera a regiao ocupada por um processo e executa a
 * coalescencia imediata com os vizinhos livres adjacentes.
 * Devolve MEMORY_ERROR_PROCESS_NOT_FOUND se o processo nao estiver na memoria.
 */
MemoryStatus memory_free(Memory *memory, int process_id);

/* memory_find_process - Localiza o bloco ocupado pelo processo (NULL se ausente). */
const MemoryBlock *memory_find_process(const Memory *memory, int process_id);

/*
 * memory_block_is_free - Predicado unico de "bloco livre": estado BLOCK_FREE
 * e ausencia de processo associado (process_id == NO_PROCESS). Usado pelo
 * modulo de alocacao para nao duplicar a definicao de bloco livre.
 */
int memory_block_is_free(const MemoryBlock *block);

/*
 * memory_validate - Verifica a integridade do modelo:
 *   - sem sobreposicao e sem lacunas (enderecos contiguos a partir de 0);
 *   - nenhum bloco com tamanho zero ou negativo;
 *   - soma dos tamanhos igual ao tamanho total da memoria;
 *   - nenhum processo duplicado em blocos diferentes;
 *   - coerencia entre o estado do bloco e o identificador do processo;
 *   - nenhum par de blocos livres adjacentes (a coalesciencia e imediata, logo
 *     dois blocos livres consecutivos indicariam coalesciencia quebrada).
 * Devolve 1 se a memoria for integra e 0 caso contrario.
 */
int memory_validate(const Memory *memory);

/*
 * memory_validate_ex - Igual a memory_validate, porem descreve em "message"
 * a primeira violacao encontrada (util nos testes automatizados).
 */
int memory_validate_ex(const Memory *memory, char *message, size_t message_size);

/* memory_total_free - Somatorio dos tamanhos de todos os blocos livres (KB). */
int memory_total_free(const Memory *memory);

/* memory_total_used - Somatorio dos tamanhos de todos os blocos ocupados (KB). */
int memory_total_used(const Memory *memory);

/* memory_largest_free_block - Tamanho do maior bloco livre contiguo (KB). */
int memory_largest_free_block(const Memory *memory);

/* memory_count_free_blocks - Numero de regioes livres existentes. */
int memory_count_free_blocks(const Memory *memory);

/* memory_count_occupied_blocks - Numero de regioes ocupadas existentes. */
int memory_count_occupied_blocks(const Memory *memory);

/* memory_block_count - Numero total de blocos (livres + ocupados). */
size_t memory_block_count(const Memory *memory);

/* memory_block_at - Acesso ao i-esimo bloco por ordem de endereco (NULL se ausente). */
const MemoryBlock *memory_block_at(const Memory *memory, size_t index);

#endif /* MEMORY_H */
