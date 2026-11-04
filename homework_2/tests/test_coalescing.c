/*
 * test_coalescing.c - Testes da coalescencia imediata de blocos livres.
 *
 * A coalescencia e verificada em tres situacoes classicas: dois blocos
 * adjacentes, um bloco cercado por dois vizinhos livres (tres ou mais) e as
 * extremidades da regiao. Tambem se verifica que blocos ocupados impedem a
 * uniao e que uma liberacao invalida nao altera a estrutura.
 */

#include "test_utils.h"

static void test_coalesce_two_blocks(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 50), MEMORY_OK, "processo 3");

    /* Libera o bloco do meio: apenas ele fica livre, cercado por ocupados. */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory.block_count, 3, "tres blocos apos liberar o meio");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "um bloco livre isolado");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 2,
                      "dois blocos ocupados");

    block = memory_block_at(&memory, 1);
    TEST_CHECK(block != NULL, "bloco livre do meio existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 20, "inicio do bloco livre");
        TEST_CHECK_EQ_INT(block->size, 30, "tamanho do bloco livre");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco do meio esta livre");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos liberar apenas o bloco do meio");

    /*
     * Agora libera o processo 1. Ele e vizinho anterior do bloco livre do
     * processo 2; os dois devem formar um unico bloco livre em [0, 50).
     */
    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "libera processo 1");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "dois blocos apos coalescer");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "um bloco livre resultante");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 1,
                      "um bloco ocupado restante");

    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "bloco resultante existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 0,
                          "coalescencia usa o menor endereco");
        TEST_CHECK_EQ_INT(block->size, 50, "tamanhos somados na coalescencia");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco resultante livre");
        TEST_CHECK_EQ_INT(block->process_id, NO_PROCESS,
                          "bloco resultante sem processo");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescer dois blocos");

    memory_destroy(&memory);
}

static void test_coalesce_three_blocks(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 10), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 20), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 30), MEMORY_OK, "processo 3");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 20), MEMORY_OK, "processo 4");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 5, 20), MEMORY_OK, "processo 5");

    /*
     * Estado inicial: [1:0-10][2:10-30][3:30-60][4:60-80][5:80-100].
     * Liberar 2 e 4 cria dois blocos livres nao adjacentes, pois o processo 3
     * permanece ocupado entre eles.
     */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK, "libera processo 4");
    TEST_CHECK_EQ_INT(memory.block_count, 5, "cinco blocos antes da coalescencia");
    TEST_CHECK_EQ_INT(memory.free_block_count, 2, "dois blocos livres separados");

    /*
     * Ao liberar o processo 3 (bloco do meio), ele coalesce com o vizinho
     * anterior (processo 2) e com o vizinho seguinte (processo 4), formando
     * um unico bloco livre em [10, 80) com 70 KB.
     */
    TEST_CHECK_EQ_INT(memory_free(&memory, 3), MEMORY_OK, "libera processo 3");
    TEST_CHECK_EQ_INT(memory.block_count, 3, "tres blocos apos coalescer tres");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "um unico bloco livre");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 2,
                      "dois blocos ocupados restantes");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 70, "memoria livre resultante");

    block = memory_block_at(&memory, 1);
    TEST_CHECK(block != NULL, "bloco resultante dos tres existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 10,
                          "endereco do bloco resultante");
        TEST_CHECK_EQ_INT(block->size, 70, "tamanho do bloco resultante");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "estado livre resultante");
        TEST_CHECK(block->next != NULL, "processo 5 continua na lista");
        if (block->next != NULL) {
            TEST_CHECK_EQ_INT(block->next->process_id, 5,
                              "processo seguinte preservado");
            TEST_CHECK_EQ_INT(block->next->start_address, 80,
                              "endereco do processo seguinte");
        }
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescer tres blocos");

    memory_destroy(&memory);
}

static void test_coalesce_at_start(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 50), MEMORY_OK, "processo 3");

    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK,
                      "libera processo 1 no inicio da regiao");

    TEST_CHECK_EQ_INT(memory.block_count, 2, "dois blocos apos coalescer no inicio");
    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "bloco livre inicial existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 0,
                          "bloco resultante comeca em zero");
        TEST_CHECK_EQ_INT(block->size, 50, "bloco resultante tem 50 KB");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco inicial livre");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescer no inicio");

    memory_destroy(&memory);
}

static void test_coalesce_at_end(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 50), MEMORY_OK, "processo 3");

    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 3), MEMORY_OK,
                      "libera processo 3 no fim da regiao");

    TEST_CHECK_EQ_INT(memory.block_count, 2, "dois blocos apos coalescer no fim");
    block = memory_block_at(&memory, 1);
    TEST_CHECK(block != NULL, "bloco livre final existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 20,
                          "bloco resultante comeca em 20");
        TEST_CHECK_EQ_INT(block->size, 80, "bloco resultante tem 80 KB");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco final livre");
        TEST_CHECK(block->next == NULL, "bloco resultante termina a lista");
        TEST_CHECK_EQ_INT(block->start_address + block->size, 100,
                          "bloco resultante termina no fim da regiao");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescer no fim");

    memory_destroy(&memory);
}

static void test_no_coalesce_with_occupied(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 50), MEMORY_OK, "processo 3");

    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");

    /* O bloco livre tem vizinhos ocupados: nenhuma coalescencia deve ocorrer. */
    TEST_CHECK_EQ_INT(memory.block_count, 3, "blocos nao foram unidos");
    TEST_CHECK_EQ_INT(memory.free_block_count, 1, "apenas um bloco livre");
    TEST_CHECK_EQ_INT(memory_count_occupied_blocks(&memory), 2,
                      "vizinhos continuam ocupados");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 30, "livre permanece 30 KB");

    block = memory_block_at(&memory, 0);
    TEST_CHECK(block != NULL, "vizinho anterior existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->state, BLOCK_OCCUPIED,
                          "vizinho anterior continua ocupado");
        TEST_CHECK_EQ_INT(block->size, 20, "vizinho anterior nao foi alterado");
    }
    block = memory_block_at(&memory, 2);
    TEST_CHECK(block != NULL, "vizinho seguinte existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->state, BLOCK_OCCUPIED,
                          "vizinho seguinte continua ocupado");
        TEST_CHECK_EQ_INT(block->size, 50, "vizinho seguinte nao foi alterado");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "apos free sem coalescencia");

    memory_destroy(&memory);
}

static void test_coalesced_size_and_address(void)
{
    Memory memory;
    const MemoryBlock *block;

    TEST_CHECK(memory_init(&memory, 200) == 1, "init de 200 KB");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 25), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 35), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 40), MEMORY_OK, "processo 3");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 100), MEMORY_OK, "processo 4");

    /* Libera 2 e 4; o processo 3 fica entre dois blocos livres. */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK, "libera processo 4");
    TEST_CHECK_EQ_INT(memory_free(&memory, 3), MEMORY_OK,
                      "libera processo 3 entre dois livres");

    /* [1:0-25] + [2:25-60] + [3:60-100] + [4:100-200] => livre [25, 200). */
    TEST_CHECK_EQ_INT(memory.block_count, 2, "dois blocos apos coalescencia");
    block = memory_block_at(&memory, 1);
    TEST_CHECK(block != NULL, "bloco resultante existe");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 25,
                          "menor endereco preservado");
        TEST_CHECK_EQ_INT(block->size, 175, "soma dos tamanhos dos blocos");
        TEST_CHECK_EQ_INT(block->state, BLOCK_FREE, "bloco resultante livre");
        TEST_CHECK_EQ_INT(block->process_id, NO_PROCESS,
                          "bloco resultante sem processo");
        TEST_CHECK(block->next == NULL, "bloco resultante e o ultimo");
        TEST_CHECK_EQ_INT(block->start_address + block->size, 200,
                          "bloco resultante alcanca o fim da memoria");
    }
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 175, "memoria livre total");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&memory), 175,
                      "maior bloco livre contiguo");
    TEST_CHECK_MEMORY_VALID(&memory, "apos coalescencia de quatro blocos");

    memory_destroy(&memory);
}

static void test_free_nonexistent_does_not_coalesce(void)
{
    Memory memory;
    const MemoryBlock *block;
    size_t blocks_before;
    int free_before;
    int used_before;

    TEST_CHECK(memory_init(&memory, 100) == 1, "init");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 20), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 30), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 20), MEMORY_OK, "processo 3");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 30), MEMORY_OK, "processo 4");

    /* Cria dois blocos livres nao adjacentes. */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK, "libera processo 4");

    blocks_before = memory.block_count;
    free_before = memory_total_free(&memory);
    used_before = memory_total_used(&memory);

    TEST_CHECK_EQ_INT(memory_free(&memory, 77), MEMORY_ERROR_PROCESS_NOT_FOUND,
                      "free de processo inexistente falha");
    TEST_CHECK_EQ_INT(memory.block_count, (int)blocks_before,
                      "block_count nao muda");
    TEST_CHECK_EQ_INT(memory.free_block_count, 2,
                      "quantidade de blocos livres nao muda");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), free_before,
                      "memoria livre nao muda");
    TEST_CHECK_EQ_INT(memory_total_used(&memory), used_before,
                      "memoria usada nao muda");

    block = memory_block_at(&memory, 1);
    TEST_CHECK(block != NULL, "primeiro bloco livre preservado");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 20,
                          "endereco do primeiro bloco livre");
        TEST_CHECK_EQ_INT(block->size, 30, "tamanho do primeiro bloco livre");
    }
    block = memory_block_at(&memory, 3);
    TEST_CHECK(block != NULL, "segundo bloco livre preservado");
    if (block != NULL) {
        TEST_CHECK_EQ_INT(block->start_address, 70,
                          "endereco do segundo bloco livre");
        TEST_CHECK_EQ_INT(block->size, 30, "tamanho do segundo bloco livre");
    }
    TEST_CHECK_MEMORY_VALID(&memory,
                            "apos free inexistente sem coalescencia");

    memory_destroy(&memory);
}

static void test_validate_after_neighbor_frees(void)
{
    Memory memory;

    TEST_CHECK(memory_init(&memory, 200) == 1, "init de 200 KB");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1, "inicio valido");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 25), MEMORY_OK, "processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 35), MEMORY_OK, "processo 2");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 3, 40), MEMORY_OK, "processo 3");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 4, 100), MEMORY_OK, "processo 4");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1, "apos alocar todos");

    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1, "apos liberar processo 2");

    TEST_CHECK_EQ_INT(memory_free(&memory, 3), MEMORY_OK,
                      "libera processo 3 vizinho do livre anterior");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                      "coalescencia com o vizinho anterior mantem validade");

    TEST_CHECK_EQ_INT(memory_allocate(&memory, 5, 10), MEMORY_OK,
                      "processo 5 usa parte do bloco livre");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1, "apos alocar processo 5");

    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK,
                      "libera processo 1 no inicio");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                      "bloco livre no inicio mantem validade");

    TEST_CHECK_EQ_INT(memory_free(&memory, 5), MEMORY_OK,
                      "libera processo 5 entre dois blocos livres");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                      "coalescencia dos dois vizinhos mantem validade");
    TEST_CHECK_EQ_INT(memory.block_count, 2, "bloco livre unico mais processo 4");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 100, "100 KB livres apos o meio");

    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK,
                      "libera processo 4 no fim");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                      "coalescencia no fim mantem validade");

    TEST_CHECK_EQ_INT(memory.block_count, 1, "um unico bloco livre ao final");
    TEST_CHECK_EQ_INT(memory_total_free(&memory), 200, "toda a memoria livre");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1,
                      "estado final permanece valido");

    memory_destroy(&memory);
}

int main(void)
{
    printf("Testes de coalescencia de blocos livres\n");
    TEST_RUN(test_coalesce_two_blocks);
    TEST_RUN(test_coalesce_three_blocks);
    TEST_RUN(test_coalesce_at_start);
    TEST_RUN(test_coalesce_at_end);
    TEST_RUN(test_no_coalesce_with_occupied);
    TEST_RUN(test_coalesced_size_and_address);
    TEST_RUN(test_free_nonexistent_does_not_coalesce);
    TEST_RUN(test_validate_after_neighbor_frees);

    printf("Checks: %d, falhas: %d\n", test_checks, test_failures);
    return TEST_SUMMARY();
}
