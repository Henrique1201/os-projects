/*
 * test_metrics.c - Testes dos indicadores de desempenho e fragmentacao.
 *
 * Os cenarios sao construidos com a API real do modulo de memoria
 * (memory_init + memory_allocate/memory_free), de modo que todos os estados
 * testados sejam ALCANCAVEIS pela simulacao e passem em memory_validate.
 */
#include <stdlib.h>

#include "metrics.h"
#include "test_utils.h"

#define TOLERANCE 1e-6

static void test_success_and_failure_rates(void)
{
    Memory memory;
    Metrics metrics;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 256), MEMORY_OK,
                      "aloca processo 1");
    TEST_CHECK_MEMORY_VALID(&memory, "estado inicial");

    /* 3 sucessos + 1 falha = 4 solicitacoes de alocacao. */
    metrics_compute(&memory, 3, 1, 17, 0.5, 1, &metrics);

    TEST_CHECK_EQ_INT(metrics.successful_allocations, 3, "sucessos");
    TEST_CHECK_EQ_INT(metrics.failed_allocations, 1, "falhas");
    TEST_CHECK_EQ_INT(metrics.total_requests, 4, "total de solicitacoes");
    TEST_CHECK_EQ_INT(metrics.total_blocks_examined, 17, "blocos examinados");
    TEST_CHECK_EQ_DOUBLE(metrics.success_rate_percent, 75.0, TOLERANCE,
                         "taxa de sucesso");
    TEST_CHECK_EQ_DOUBLE(metrics.failure_rate_percent, 25.0, TOLERANCE,
                         "taxa de falha");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.5, TOLERANCE,
                         "pico de fragmentacao copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico de fragmentacao definido copiado");

    memory_destroy(&memory);
}

static void test_memory_distribution(void)
{
    Memory memory;
    Metrics metrics;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 256), MEMORY_OK,
                      "aloca processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 99, 100), MEMORY_OK,
                      "aloca processo temporario");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 468), MEMORY_OK,
                      "aloca processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 99), MEMORY_OK,
                      "libera processo temporario");
    TEST_CHECK_MEMORY_VALID(&memory, "layout [256][100 livre][468][200 livre]");

    /*
     * Estado alcancavel: 256 + 468 = 724 KB ocupados e 300 KB livres em dois
     * blocos nao adjacentes (100 e 200 KB).
     */
    metrics_compute(&memory, 0, 0, 0, 0.75, 1, &metrics);

    TEST_CHECK_EQ_INT(metrics.total_memory, 1024, "memoria total");
    TEST_CHECK_EQ_INT(metrics.used_memory, 724, "memoria ocupada");
    TEST_CHECK_EQ_INT(metrics.free_memory, 300, "memoria livre");
    TEST_CHECK_EQ_INT(metrics.largest_free_block, 200, "maior bloco livre");
    TEST_CHECK_EQ_INT(metrics.free_block_count, 2, "blocos livres");
    TEST_CHECK_EQ_INT(metrics.occupied_block_count, 2, "blocos ocupados");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 1,
                      "fragmentacao definida");
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 1.0 - 200.0 / 300.0, TOLERANCE,
                         "fragmentacao externa");
    TEST_CHECK_EQ_DOUBLE(metrics.success_rate_percent, 0.0, TOLERANCE,
                         "taxa de sucesso sem solicitacoes");
    TEST_CHECK_EQ_DOUBLE(metrics.failure_rate_percent, 0.0, TOLERANCE,
                         "taxa de falha sem solicitacoes");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.75, TOLERANCE,
                         "pico copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico definido copiado");

    memory_destroy(&memory);
}

static void test_external_fragmentation(void)
{
    Memory memory;
    Metrics metrics;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 100), MEMORY_OK,
                      "aloca processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 724), MEMORY_OK,
                      "aloca processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 1), MEMORY_OK, "libera processo 1");
    TEST_CHECK_MEMORY_VALID(&memory, "layout [100 livre][724][200 livre]");

    /* 300 KB livres em blocos de 100 e 200: 1 - 200/300 = 0.3333... */
    TEST_CHECK_EQ_DOUBLE(metrics_fragmentation(200, 300), 1.0 - 2.0 / 3.0,
                         TOLERANCE, "fragmentacao direta");

    metrics_compute(&memory, 0, 0, 0, 1.0 / 3.0, 1, &metrics);
    TEST_CHECK_EQ_INT(metrics.free_memory, 300, "memoria livre");
    TEST_CHECK_EQ_INT(metrics.largest_free_block, 200, "maior bloco");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 1, "definida");
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 1.0 - 2.0 / 3.0, TOLERANCE,
                         "fragmentacao pela memoria");
    TEST_CHECK_EQ_INT(metrics.free_block_count, 2, "dois blocos livres");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 1.0 / 3.0, TOLERANCE,
                         "pico copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico definido copiado");

    /* Limites do indicador. */
    TEST_CHECK_EQ_DOUBLE(metrics_fragmentation(300, 300), 0.0, TOLERANCE,
                         "memoria livre contigua");
    TEST_CHECK_EQ_DOUBLE(metrics_fragmentation(0, 300), 1.0, TOLERANCE,
                         "memoria toda fragmentada");

    memory_destroy(&memory);
}

static void test_zero_free_memory_is_undefined(void)
{
    Memory memory;
    Metrics metrics;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 512), MEMORY_OK,
                      "aloca processo 1");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 2, 512), MEMORY_OK,
                      "aloca processo 2");
    TEST_CHECK_MEMORY_VALID(&memory, "memoria totalmente ocupada");

    metrics_compute(&memory, 1, 0, 3, 0.0, 0, &metrics);

    TEST_CHECK_EQ_INT(metrics.free_memory, 0, "memoria livre zero");
    TEST_CHECK_EQ_INT(metrics.largest_free_block, 0, "sem bloco livre");
    TEST_CHECK_EQ_INT(metrics.free_block_count, 0, "nenhum bloco livre");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 0,
                      "fragmentacao indefinida");
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 0.0, TOLERANCE,
                         "valor convencionado");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.0, TOLERANCE,
                         "pico indefinido copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 0,
                      "pico indefinido copiado");
    TEST_CHECK_EQ_DOUBLE(metrics_fragmentation(0, 0), 0.0, TOLERANCE,
                         "funcao direta com livre zero");
    TEST_CHECK_EQ_DOUBLE(metrics_fragmentation(10, -5), 0.0, TOLERANCE,
                         "funcao direta com livre negativo");

    memory_destroy(&memory);
}

static void test_utilization_percent(void)
{
    Memory memory;
    Metrics metrics;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");
    TEST_CHECK_EQ_INT(memory_allocate(&memory, 1, 256), MEMORY_OK,
                      "aloca processo 1");
    TEST_CHECK_MEMORY_VALID(&memory, "estado [256][768 livre]");

    metrics_compute(&memory, 0, 0, 0, 0.25, 1, &metrics);
    TEST_CHECK_EQ_DOUBLE(metrics.utilization_percent, 25.0, TOLERANCE,
                         "256 de 1024 KB");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.25, TOLERANCE,
                         "pico copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico definido copiado");

    memory_destroy(&memory);
}

static void test_fragmentation_nontrivial_reachable(void)
{
    Memory memory;
    Metrics metrics;
    int i;

    TEST_CHECK(memory_init(&memory, 1024), "inicializacao da memoria");

    /* Quatro processos de 256 KB ocupam toda a memoria. */
    for (i = 0; i < 4; i++) {
        TEST_CHECK_EQ_INT(memory_allocate(&memory, i + 1, 256), MEMORY_OK,
                          "aloca processo de 256 KB");
    }
    TEST_CHECK_MEMORY_VALID(&memory, "memoria totalmente ocupada");

    /* Liberar dois alternados deixa 512 KB em dois blocos de 256 KB. */
    TEST_CHECK_EQ_INT(memory_free(&memory, 2), MEMORY_OK, "libera processo 2");
    TEST_CHECK_EQ_INT(memory_free(&memory, 4), MEMORY_OK, "libera processo 4");
    TEST_CHECK_EQ_INT(memory_validate(&memory), 1, "estado alcancavel valido");
    TEST_CHECK_MEMORY_VALID(&memory, "layout [256][256 livre][256][256 livre]");

    metrics_compute(&memory, 4, 0, 10, 0.5, 1, &metrics);

    TEST_CHECK_EQ_INT(metrics.used_memory, 512, "memoria ocupada");
    TEST_CHECK_EQ_INT(metrics.free_memory, 512, "memoria livre");
    TEST_CHECK_EQ_INT(metrics.largest_free_block, 256, "maior bloco livre");
    TEST_CHECK_EQ_INT(metrics.free_block_count, 2, "dois blocos livres");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 1,
                      "fragmentacao definida");
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 0.5, TOLERANCE,
                         "fragmentacao 1 - 256/512");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.5, TOLERANCE,
                         "pico copiado");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico definido copiado");

    memory_destroy(&memory);
}

static void test_empty_memory_does_not_divide_by_zero(void)
{
    Memory memory = {0};
    Metrics metrics;

    /*
     * Estado degenerado (nenhuma regiao): exercita apenas as guardas de
     * divisao por zero. Nao representa um layout de simulacao.
     */
    metrics_compute(&memory, 0, 0, 0, 0.0, 0, &metrics);

    TEST_CHECK_EQ_INT(metrics.total_memory, 0, "memoria total zero");
    TEST_CHECK_EQ_DOUBLE(metrics.utilization_percent, 0.0, TOLERANCE,
                         "utilizacao sem divisao por zero");
    TEST_CHECK_EQ_DOUBLE(metrics.success_rate_percent, 0.0, TOLERANCE,
                         "sucesso sem divisao por zero");
    TEST_CHECK_EQ_DOUBLE(metrics.failure_rate_percent, 0.0, TOLERANCE,
                         "falha sem divisao por zero");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 0,
                      "fragmentacao indefinida");
}

int main(void)
{
    TEST_RUN(test_success_and_failure_rates);
    TEST_RUN(test_memory_distribution);
    TEST_RUN(test_external_fragmentation);
    TEST_RUN(test_zero_free_memory_is_undefined);
    TEST_RUN(test_utilization_percent);
    TEST_RUN(test_fragmentation_nontrivial_reachable);
    TEST_RUN(test_empty_memory_does_not_divide_by_zero);

    printf("  Checks: %d, falhas: %d\n", test_checks, test_failures);
    return TEST_SUMMARY();
}
