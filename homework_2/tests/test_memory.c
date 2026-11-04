/*
 * test_memory.c - Testes do gerenciamento basico de memoria contigua.
 *
 * Cobre inicializacao, alocacao (divisao de blocos), liberacao com
 * coalescencia, falhas sem alteracao de estado, consultas e validacao.
 */

#include "test_utils.h"

static void test_init(void)
{
    Memory memory;

    TEST_CHECK(memory_init(&memory, 100) == 1,
               "memory_init deve retornar 1 para tamanho valido");
    TEST_CHECK_EQ_INT(memory.total_size, 100, "tamanho total");
    TEST_CHECK(memory.head != NULL, "lista deve ter o bloco inicial");
    TEST_CHECK_EQ_INT(memory.block_count, 1, "block_count inicial");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "free_block_count inicial");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 0,
                      "occupied_block_count inicial");

    if (memory.head != NULL) {
        TEST_CHECK_EQ_INT(memory.head->process_id, NO_PROCESS,
                          "bloco inicial livre");
        TEST_CHECK_EQ_INT(memory.head->start_address, 0,
                          "bloco inicial comeca em 0");
        TEST_CHECK_EQ_INT(memory.head->size, 100, "bloco inicial cobre tudo");
        TEST_CHECK_EQ_INT(memory.head->state, BLOCK_FREE, "estado inicial livre");
        TEST_CHECK(memory.head->next == NULL, "bloco inicial e o unico");
    }

    TEST_CHECK_MEMORY_VALID(&memory, "apos memory_init");
    memory_destroy(&memory);

    TEST_CHECK(memory.head == NULL, "destroy limpa a lista");
    TEST_CHECK_EQ_INT(memory.total_size, 0, "destroy zera o tamanho");
    TEST_CHECK_EQ_INT(memory.block_count, 0, "destroy zera block_count");
    TEST_CHECK_EQ_INT(memory.free_block_count, 0,
                      "destroy zera free_block_count");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 0,
                      "destroy zera occupied_block_count");

    /* destroy deve ser idempotente. */
    memory_destroy(&memory);
    TEST_CHECK(memory.head == NULL, "destroy repetido e seguro");

    /* Tamanhos invalidos deixam a estrutura zerada. */
    TEST_CHECK(memory_init(&memory, 0) == 0, "init com tamanho 0 falha");
    TEST_CHECK_EQ_INT(memory.total_size, 0, "init invalido zera total_size");
    TEST_CHECK(memory.head == NULL, "init invalido zera a lista");
    TEST_CHECK_EQ_INT(memory.block_count, 0, "init invalido zera contadores");

    TEST_CHECK(memory_init(&memory, -10) == 0, "init com tamanho negativo falha");
    TEST_CHECK_EQ_INT(memory.total_size, 0, "init negativo zera total_size");
    TEST_CHECK(memory.head == NULL, "init negativo zera a lista");
}

static void test_allocate(void)
{
    Memory memory;
    const MemoryBlock *first;
    const MemoryBlock *second;
    const MemoryBlock *process;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 30), MEMORY_OK,
                      "alocacao de 30 KB");

    first = memory_block_at(&memory, 0);
    second = memory_block_at(&memory, 1);
    TEST_CHECK(first != NULL, "primeiro bloco existe");
    TEST_CHECK(second != NULL, "segundo bloco existe");

    if (first != NULL) {
        TEST_CHECK_EQ_INT(first->process_id, 1, "processo do primeiro bloco");
        TEST_CHECK_EQ_INT(first->start_address, 0, "endereco do processo");
        TEST_CHECK_EQ_INT(first->size, 30, "tamanho alocado");
        TEST_CHECK_EQ_INT(first->state, BLOCK_OCCUPIED, "bloco ocupado");
    }
    if (second != NULL) {
        TEST_CHECK_EQ_INT(second->process_id, NO_PROCESS,
                          "remanescente sem processo");
        TEST_CHECK_EQ_INT(second->start_address, 30,
                          "remanescente comeca apos a alocacao");
        TEST_CHECK_EQ_INT(second->size, 70, "tamanho do remanescente");
        TEST_CHECK_EQ_INT(second->state, BLOCK_FREE, "remanescente livre");
        TEST_CHECK(second->next == NULL, "remanescente e o ultimo bloco");
    }

    TEST_CHECK_EQ_INT(memory.block_count, 2, "block_count apos alocar");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "free_block_count apos alocar");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 1,
                      "occupied_block_count apos alocar");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 70, "memoria livre");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 30, "memoria ocupada");

    process = memory_find_process(&memory, 1);
    TEST_CHECK(process != NULL, "memory_find_process encontra o processo");
    if (process != NULL) {
        TEST_CHECK_EQ_INT(process->start_address, 0, "endereco do processo 1");
        TEST_CHECK_EQ_INT(process->size, 30, "tamanho do processo 1");
    }
    TEST_CHECK(memory_find_process(&memory, 2) == NULL,
               "processo ausente nao e encontrado");
    TEST_CHECK(memory_block_at(&memory, 2) == NULL,
               "indice fora do intervalo devolve NULL");
    TEST_CHECK_MEMORY_VALID(&memory, "apos alocacao de 30 KB");

    /* Segunda alocacao usa o primeiro bloco livre suficiente. */
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 40), MEMORY_OK,
                      "alocacao de 40 KB");
    first = memory_block_at(&memory, 0);
    second = memory_block_at(&memory, 1);
    TEST_CHECK(first != NULL && second != NULL, "blocos apos segunda alocacao");
    if (first != NULL) {
        TEST_CHECK_EQ_INT(first->process_id, 1, "processo 1 no inicio");
        TEST_CHECK_EQ_INT(first->size, 30, "processo 1 mantem tamanho");
    }
    if (second != NULL) {
        TEST_CHECK_EQ_INT(second->process_id, 2, "processo 2 logo apos");
        TEST_CHECK_EQ_INT(second->start_address, 30, "endereco do processo 2");
        TEST_CHECK_EQ_INT(second->size, 40, "tamanho do processo 2");
    }
    TEST_CHECK_EQ_INT(memory.block_count, 3, "block_count com dois processos");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 30, "livre restante");
    TEST_CHECK_MEMORY_VALID(&memory, "apos segunda alocacao");

    memory_destroy(&memory);
}

static void test_split_exact_fit(void)
{
    Memory memory;
    const MemoryBlock *second;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 40), MEMORY_OK,
                      "alocacao de 40 KB");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 60), MEMORY_OK,
                      "alocacao exata do remanescente");

    second = memory_block_at(&memory, 1);
    TEST_CHECK(second != NULL, "segundo bloco existe");
    if (second != NULL) {
        TEST_CHECK_EQ_INT(second->process_id, 2, "processo do bloco exato");
        TEST_CHECK_EQ_INT(second->start_address, 40, "endereco do bloco exato");
        TEST_CHECK_EQ_INT(second->size, 60, "tamanho do bloco exato");
    }
    TEST_CHECK_EQ_INT(memory.block_count, 2, "sem bloco livre remanescente");
    TEST_CHECK_EQ_INT(memory.free_block_count, 0, "nenhum bloco livre");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 2, "dois blocos ocupados");
    TEST_CHECK(memory_block_at(&memory, 2) == NULL, "lista termina no bloco 2");
    TEST_CHECK_MEMORY_VALID(&memory, "apos alocacao exata");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 1), MEMORY_ERROR_NO_SPACE,
                      "memoria cheia nao aceita novo processo");
    TEST_CHECK_MEMORY_VALID(&memory, "apos falha por memoria cheia");

    memory_destroy(&memory);
}

static void test_no_space_contiguous(void)
{
    Memory memory;
    size_t blocks_before;
    int free_before;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 40), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 20), MEMORY_OK, "processo 3");

    blocks_before = memory.block_count;
    free_before = memory_total_free(&memory);
    TEST_CHECK_EQ_INT(free_before, 10, "livre total de 10 KB");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 10,
                      "maior bloco livre de 10 KB");

    /* Ha 10 KB livres, mas nao contiguos o bastante para 15 KB. */
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 15), MEMORY_ERROR_NO_SPACE,
                      "falha por falta de espaco contiguo");
    TEST_CHECK_EQ_INT(memory.block_count, (int)blocks_before,
                      "block_count inalterado na falha");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), free_before,
                      "memoria livre inalterada na falha");
    TEST_CHECK(memory_find_process(&memory, 4) == NULL,
               "processo nao foi alocado");
    TEST_CHECK_MEMORY_VALID(&memory, "apos falha por falta de espaco contiguo");

    memory_destroy(&memory);
}

static void test_request_larger_than_total(void)
{
    Memory memory;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 101), MEMORY_ERROR_NO_SPACE,
                      "pedido maior que a memoria total falha");
    TEST_CHECK_EQ_INT(memory.block_count, 1, "estado inalterado");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 100, "memoria intacta");
    TEST_CHECK(memory_find_process(&memory, 1) == NULL, "processo ausente");
    TEST_CHECK_MEMORY_VALID(&memory, "apos pedido grande demais");

    /* Exatamente o tamanho total e valido. */
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 100), MEMORY_OK,
                      "pedido igual ao total e atendido");
    TEST_CHECK_EQ_INT(memory.block_count, 1, "um unico bloco ocupado");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 100, "memoria totalmente usada");
    TEST_CHECK_MEMORY_VALID(&memory, "apos alocar toda a memoria");

    memory_destroy(&memory);
}

static void test_free(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 30), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 40), MEMORY_OK, "processo 2");

    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "liberacao do processo 1");

    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "bloco liberado existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->process_id, NO_PROCESS,
                          "bloco liberado sem processo");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco liberado esta livre");
        TEST_CHECK_EQ_INT(block->start_address, 0, "endereco do bloco liberado");
        TEST_CHECK_EQ_INT(block->size, 30, "tamanho do bloco liberado");
    }
    TEST_CHECK_EQ_INT(memory.block_count, 3, "block_count apos liberar");
    TEST_CHECK_EQ_INT(memory.free_block_count, 2, "dois blocos livres");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 1, "um bloco ocupado");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 60, "memoria livre apos free");
    TEST_CHECK(memory_find_process(&memory, 1) == NULL,
               "processo liberado nao e encontrado");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar o processo 1");

    /* Ao liberar o processo 2, os tres blocos viram um unico bloco livre. */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "liberacao do processo 2");
    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "bloco unico apos coalescencia");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->process_id, NO_PROCESS,
                          "bloco unico sem processo");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco unico livre");
        TEST_CHECK_EQ_INT(block->start_address, 0, "bloco unico comeca em 0");
        TEST_CHECK_EQ_INT(block->size, 100, "bloco unico cobre toda a memoria");
        TEST_CHECK(block->next == NULL, "bloco unico e o ultimo");
    }
    TEST_CHECK_EQ_INT(memory.block_count, 1, "coalescencia total");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "um bloco livre");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 0, "nenhum ocupado");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 100, "toda a memoria livre");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 100,
                      "maior bloco livre e a memoria inteira");
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescencia total");

    memory_destroy(&memory);
}

static void test_free_nonexistent(void)
{
    Memory memory;
    size_t blocks_before;
    int free_before;
    int used_before;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 30), MEMORY_OK, "processo 1");

    blocks_before = memory.block_count;
    free_before = memory_total_free(&memory);
    used_before = memory_total_used(&memory);

    TEST_CHECK_EQ_INT(memory_free(&memory, 99), MEMORY_ERROR_PROCESS_NOT_FOUND,
                      "liberar processo inexistente falha");
    TEST_CHECK_EQ_INT(memory.block_count, (int)blocks_before,
                      "block_count inalterado");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), free_before,
                      "memoria livre inalterada");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), used_before,
                      "memoria usada inalterada");
    TEST_CHECK_MEMORY_VALID(&memory, "apos free de processo inexistente");

    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "libera o processo 1");
    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_ERROR_PROCESS_NOT_FOUND,
                      "segunda liberacao falha");
    TEST_CHECK_EQ_INT(memory.block_count, 1, "estrutura continua com um bloco");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 100, "memoria toda livre");
    TEST_CHECK_MEMORY_VALID(&memory, "apos free repetido");

    memory_destroy(&memory);
}

static void test_allocate_duplicate(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 30), MEMORY_OK, "processo 1");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20),
                      MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED,
                      "processo duplicado deve falhar");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "block_count inalterado");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 70, "memoria livre inalterada");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 30, "memoria usada inalterada");

    block = memory_find_process(&memory, 1);
    TEST_CHECK(block != NULL, "processo 1 continua alocado");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 0, "endereco inalterado");
        TEST_CHECK_EQ_INT(block->size, 30, "tamanho inalterado");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos tentativa de alocacao duplicada");

    memory_destroy(&memory);
}

static void test_direct_split(void)
{
    Memory memory;
    MemoryBlock fake;
    MemoryBlock *first;
    MemoryBlock *remainder;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    first = memory.head;
    TEST_CHECK(first != NULL, "bloco inicial existe");

    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, first, 7, 40), MEMORY_OK,
                      "divisao direta do bloco livre");
    TEST_CHECK_EQ_INT(first->process_id, 7, "processo no inicio do bloco");
    TEST_CHECK_EQ_INT(first->start_address, 0, "endereco da particao");
    TEST_CHECK_EQ_INT(first->size, 40, "tamanho da particao");
    TEST_CHECK_EQ_INT(first->state, BLOCK_OCCUPIED, "particao ocupada");

    remainder = first->next;
    TEST_CHECK(remainder != NULL, "remanescente criado");
    if (remainder != NULL) {
        TEST_CHECK_EQ_INT(remainder->process_id, NO_PROCESS,
                          "remanescente sem processo");
        TEST_CHECK_EQ_INT(remainder->start_address, 40,
                          "remanescente logo apos a particao");
        TEST_CHECK_EQ_INT(remainder->size, 60, "tamanho do remanescente");
        TEST_CHECK_EQ_INT(remainder->state, BLOCK_FREE, "remanescente livre");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos divisao direta");

    /* Bloco que nao pertence a lista deve ser rejeitado. */
    fake.process_id = NO_PROCESS;
    fake.start_address = 0;
    fake.size = 10;
    fake.state = BLOCK_FREE;
    fake.next = NULL;
    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, &fake, 8, 5),
                      MEMORY_ERROR_INVALID_ARGUMENT,
                      "bloco externo a lista e rejeitado");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "estado preservado apos bloco invalido");
    TEST_CHECK_MEMORY_VALID(&memory, "apos bloco externo rejeitado");

    /* Bloco ocupado nao pode ser dividido novamente. */
    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, first, 9, 5),
                      MEMORY_ERROR_INVALID_ARGUMENT,
                      "bloco ocupado nao pode ser reutilizado");
    TEST_CHECK_MEMORY_VALID(&memory, "apos tentativa em bloco ocupado");

    /* Processo duplicado deve ser rejeitado. */
    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, remainder, 7, 5),
                      MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED,
                      "processo duplicado na divisao direta");
    TEST_CHECK_MEMORY_VALID(&memory, "apos duplicata na divisao direta");

    /* Pedido maior que o bloco livre alvo. */
    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, remainder, 9, 61),
                      MEMORY_ERROR_NO_SPACE,
                      "pedido maior que o bloco alvo falha");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "estado preservado apos falta de espaco");
    TEST_CHECK_MEMORY_VALID(&memory, "apos falta de espaco na divisao direta");

    /* Divisao exata remove o bloco livre remanescente. */
    TEST_CHECK_EQ_INT(memory_allocate_in_block(&memory, remainder, 9, 60),
                      MEMORY_OK, "divisao exata do remanescente");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "sem bloco livre apos divisao exata");
    TEST_CHECK_EQ_INT(memory.free_block_count, 0, "nenhum bloco livre");
    TEST_CHECK_EQ_INT(memory.occupied_block_count, 2, "dois blocos ocupados");
    TEST_CHECK_MEMORY_VALID(&memory, "apos divisao exata direta");

    memory_destroy(&memory);
}

static void test_integrity_after_operations(void)
{
    Memory memory;

    TEST_CHECK(memory_init(&memory, 200) == 1, "init de 200 KB");
    TEST_CHECK_MEMORY_VALID(&memory, "inicio");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 50), MEMORY_OK, "processo 1");
    TEST_CHECK_MEMORY_VALID(&memory, "apos processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_MEMORY_VALID(&memory, "apos processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 20), MEMORY_OK, "processo 3");
    TEST_CHECK_MEMORY_VALID(&memory, "apos processo 3");

    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar processo 2");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 25), MEMORY_OK, "processo 4");
    TEST_CHECK_MEMORY_VALID(&memory, "apos processo 4");

    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "libera processo 1");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar processo 1");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 5, 60), MEMORY_OK, "processo 5");
    TEST_CHECK_MEMORY_VALID(&memory, "apos processo 5");

    TEST_CHECK_EQ_INT(memory_free(&memory, 3), MEMORY_OK, "libera processo 3");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar processo 3");
    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK, "libera processo 4");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar processo 4");
    TEST_CHECK_EQ_INT(memory_free(&memory, 5), MEMORY_OK, "libera processo 5");
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar processo 5");

    TEST_CHECK_EQ_INT(memory.block_count, 1, "coalescencia final");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 200, "toda a memoria livre");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 0, "nada ocupado");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 200,
                      "maior bloco livre final");

    memory_destroy(&memory);
}

static void test_queries(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");

    TEST_CHECK_EQ_INT(memory_total_free(&memory), 50, "livre com dois processos");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 50, "usado com dois processos");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 50,
                      "maior bloco livre inicial");
    TEST_CHECK_EQ_INT(memory_count_free_blocks(&memory), 1, "um bloco livre");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 2,
                      "dois blocos ocupados");
    TEST_CHECK_EQ_INT(memory_block_count(&memory), 3, "tres blocos no total");

    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "libera processo 1");

    TEST_CHECK_EQ_INT(memory_total_free(&memory), 70, "livre apos liberar");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), 30, "usado apos liberar");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 50,
                      "maior bloco livre apos liberar");
    TEST_CHECK_EQ_INT(memory_count_free_blocks(&memory), 2,
                      "dois blocos livres apos liberar");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 1,
                      "um bloco ocupado apos liberar");
    TEST_CHECK_EQ_INT(memory_block_count(&memory), 3, "total de blocos apos liberar");

    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "primeiro bloco apos liberar");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 0, "endereco do primeiro bloco");
        TEST_CHECK_EQ_INT(block->size, 20, "tamanho do primeiro bloco");
    }
    block = memory_block_at(&memory, 2);
    TEST_CHECK(block != NULL, "ultimo bloco apos liberar");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 50, "endereco do ultimo bloco");
        TEST_CHECK_EQ_INT(block->size, 50, "tamanho do ultimo bloco");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos consultas");

    memory_destroy(&memory);
}

static void test_reset(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 30), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_reset(&memory), 1, "reset com sucesso");

    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "reset cria um bloco");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 0, "reset preserva o inicio");
        TEST_CHECK_EQ_INT(block->size, 100, "reset preserva o tamanho total");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "reset deixa tudo livre");
        TEST_CHECK_EQ_INT(block->process_id, NO_PROCESS,
                          "reset remove os processos");
    }
    TEST_CHECK_EQ_INT(memory.total_size, 100, "total_size preservado");
    TEST_CHECK_EQ_INT(memory.block_count, 1, "reset deixa um bloco");
    TEST_CHECK_MEMORY_VALID(&memory, "apos reset");

    memory_destroy(&memory);
}

static void test_block_is_free_predicate(void)
{
    MemoryBlock block;

    TEST_CHECK(memory_block_is_free(NULL) == 0, "NULL nao e bloco livre");

    block.process_id = NO_PROCESS;
    block.start_address = 0;
    block.size = 10;
    block.state = BLOCK_FREE;
    block.next = NULL;
    TEST_CHECK(memory_block_is_free(&block) == 1, "bloco livre consistente");

    /* Estado e identificador de processo nao podem divergir. */
    block.process_id = 3;
    TEST_CHECK(memory_block_is_free(&block) == 0,
               "bloco livre com processo associado nao e livre");

    block.process_id = 3;
    block.state = BLOCK_OCCUPIED;
    TEST_CHECK(memory_block_is_free(&block) == 0, "bloco ocupado nao e livre");

    block.process_id = NO_PROCESS;
    block.state = BLOCK_OCCUPIED;
    TEST_CHECK(memory_block_is_free(&block) == 0,
               "bloco ocupado sem processo nao e livre");
}

static void test_validate_rejects_adjacent_free_blocks(void)
{
    Memory memory;
    MemoryBlock *first;
    MemoryBlock *second;
    MemoryBlock *third;
    char message[128];

    first = malloc(sizeof(*first));
    second = malloc(sizeof(*second));
    TEST_CHECK(first != NULL && second != NULL, "blocos manuais alocados");
    if (first == NULL || second == NULL) {
        free(first);
        free(second);
        return;
    }

    first->process_id = NO_PROCESS;
    first->start_address = 0;
    first->size = 30;
    first->state = BLOCK_FREE;
    first->next = second;

    second->process_id = NO_PROCESS;
    second->start_address = 30;
    second->size = 70;
    second->state = BLOCK_FREE;
    second->next = NULL;

    memory.total_size = 100;
    memory.head = first;
    memory.block_count = 2;
    memory.free_block_count = 2;
    memory.occupied_block_count = 0;

    TEST_CHECK_EQ_INT(memory_validate(&memory), 0,
                      "dois blocos livres adjacentes invalidam a memoria");
    TEST_CHECK_EQ_INT(memory_validate_ex(&memory, message, sizeof(message)), 0,
                      "validate_ex rejeita dois blocos livres adjacentes");
    TEST_CHECK(strstr(message, "adjacentes") != NULL,
               "mensagem descreve a adjacencia: %s", message);

    /* Tres blocos livres consecutivos tambem devem ser rejeitados. */
    third = malloc(sizeof(*third));
    TEST_CHECK(third != NULL, "terceiro bloco manual alocado");
    if (third != NULL) {
        first->size = 20;
        second->start_address = 20;
        second->size = 30;
        second->next = third;

        third->process_id = NO_PROCESS;
        third->start_address = 50;
        third->size = 50;
        third->state = BLOCK_FREE;
        third->next = NULL;

        memory.block_count = 3;
        memory.free_block_count = 3;

        TEST_CHECK_EQ_INT(memory_validate(&memory), 0,
                          "tres blocos livres adjacentes invalidam a memoria");
        TEST_CHECK_EQ_INT(memory_validate_ex(&memory, message, sizeof(message)),
                          0, "validate_ex rejeita tres blocos livres adjacentes");
        TEST_CHECK(strstr(message, "adjacentes") != NULL,
                   "mensagem descreve a adjacencia tripla: %s", message);

        /* Estado equivalente com o bloco do meio ocupado e valido. */
        second->process_id = 1;
        second->state = BLOCK_OCCUPIED;
        memory.free_block_count = 2;
        memory.occupied_block_count = 1;
        TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                          "blocos livres separados por ocupado sao validos");
    }

    memory_destroy(&memory);
}

static void test_status_messages(void)
{
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_OK),
                      "operacao concluida com sucesso", "mensagem de sucesso");
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_ERROR_INVALID_ARGUMENT),
                      "argumento invalido", "mensagem de argumento invalido");
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_ERROR_NO_SPACE),
                      "nao ha espaco contiguo suficiente",
                      "mensagem de falta de espaco");
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_ERROR_PROCESS_NOT_FOUND),
                      "processo nao encontrado na memoria",
                      "mensagem de processo ausente");
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_ERROR_PROCESS_ALREADY_ALLOCATED),
                      "processo ja alocado na memoria",
                      "mensagem de processo duplicado");
    TEST_CHECK_EQ_STR(memory_status_message(MEMORY_ERROR_OUT_OF_MEMORY),
                      "falha ao alocar memoria interna",
                      "mensagem de falha de alocacao interna");
}

int main(void)
{
    printf("Testes do gerenciamento basico de memoria\n");
    TEST_RUN(test_init);
    TEST_RUN(test_allocate);
    TEST_RUN(test_split_exact_fit);
    TEST_RUN(test_no_space_contiguous);
    TEST_RUN(test_request_larger_than_total);
    TEST_RUN(test_free);
    TEST_RUN(test_free_nonexistent);
    TEST_RUN(test_allocate_duplicate);
    TEST_RUN(test_direct_split);
    TEST_RUN(test_integrity_after_operations);
    TEST_RUN(test_queries);
    TEST_RUN(test_reset);
    TEST_RUN(test_block_is_free_predicate);
    TEST_RUN(test_validate_rejects_adjacent_free_blocks);
    TEST_RUN(test_status_messages);

    printf("Checks: %d, falhas: %d\n", test_checks, test_failures);
    return TEST_SUMMARY();
}
