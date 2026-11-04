/*
 * metrics.c - Indicadores de desempenho e fragmentacao externa.
 *
 * Todas as metricas derivam do estado final da memoria (via primitivas de
 * memory.h) e dos contadores acumulados pela simulacao. O modulo nao altera o
 * estado: apenas observa e calcula.
 *
 * CONVENCOES
 *   - "solicitacoes" (total_requests) contam apenas pedidos de ALOCACAO. Uma
 *     falha de FREE nao e uma solicitacao de alocacao e, portanto, nao entra
 *     nesse total.
 *   - Sem memoria livre total, a fragmentacao externa e INDEFINIDA: o valor
 *     fica 0.0 e fragmentation_defined fica 0 (evita divisao por zero).
 *   - Denominadores zero produzem taxas 0.0, e nao valores inventados.
 *   - O pico de fragmentacao (peak_fragmentation / peak_fragmentation_defined)
 *     NAO e calculado aqui: a camada de simulacao observa o estado apos cada
 *     operacao e repassa o maior valor encontrado. Este modulo apenas o copia
 *     para o registro de metricas, preservando a responsabilidade unica de
 *     cada camada.
 */
#include "metrics.h"

/* Restringe um valor ao intervalo fechado [0.0, 1.0]. */
static double clamp_unit(double value)
{
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

double metrics_fragmentation(int largest_free_block, int total_free_memory)
{
    double fragmentation;

    /*
     * Caso indefinido: nao ha memoria livre para dividir. A convencao adotada
     * em metrics.h devolve 0.0 (e metrics_compute marca o indicador como
     * indefinido).
     */
    if (total_free_memory <= 0) {
        return 0.0;
    }

    /*
     * Fragmentacao externa = 1 - (maior bloco livre / memoria livre total).
     * Quanto mais espalhada a memoria livre, menor o maior bloco contiguo e
     * mais proximo de 1 o indicador.
     */
    fragmentation = 1.0 - (double)largest_free_block /
                              (double)total_free_memory;
    return clamp_unit(fragmentation);
}

void metrics_compute(const Memory *memory, long successful_allocations,
                     long failed_allocations, long total_blocks_examined,
                     double peak_fragmentation, int peak_fragmentation_defined,
                     Metrics *metrics)
{
    Metrics computed = {0};
    long total_requests;

    if (metrics == NULL) {
        return;
    }

    if (successful_allocations < 0) {
        successful_allocations = 0;
    }
    if (failed_allocations < 0) {
        failed_allocations = 0;
    }
    if (total_blocks_examined < 0) {
        total_blocks_examined = 0;
    }

    /*
     * Total de SOLICITACOES DE ALOCACAO: sucessos + falhas de alocacao.
     * Falhas de FREE ficam de fora dessa contagem por nao serem pedidos de
     * alocacao (ver simulation.h).
     */
    total_requests = successful_allocations + failed_allocations;

    computed.successful_allocations = successful_allocations;
    computed.failed_allocations = failed_allocations;
    computed.total_requests = total_requests;
    computed.total_blocks_examined = total_blocks_examined;

    /*
     * O pico e observado pela simulacao passo a passo; aqui apenas o
     * transportamos para o registro final de metricas.
     */
    computed.peak_fragmentation = peak_fragmentation;
    computed.peak_fragmentation_defined = peak_fragmentation_defined;

    if (memory != NULL) {
        computed.total_memory = memory->total_size;
        computed.free_memory = memory_total_free(memory);
        computed.used_memory = memory_total_used(memory);
        computed.largest_free_block = memory_largest_free_block(memory);
        computed.free_block_count = memory_count_free_blocks(memory);
        computed.occupied_block_count = memory_count_occupied_blocks(memory);

        computed.fragmentation =
            metrics_fragmentation(computed.largest_free_block,
                                  computed.free_memory);
        computed.fragmentation_defined = (computed.free_memory > 0) ? 1 : 0;

        if (computed.total_memory > 0) {
            computed.utilization_percent =
                (double)computed.used_memory * 100.0 /
                (double)computed.total_memory;
        }
    }

    if (total_requests > 0) {
        computed.success_rate_percent =
            (double)successful_allocations * 100.0 / (double)total_requests;
        computed.failure_rate_percent =
            (double)failed_allocations * 100.0 / (double)total_requests;
    }

    *metrics = computed;
}
