/*
 * test_simulation.c - Testes da camada de simulacao.
 *
 * Verifica a integracao entre memoria, estrategia e contadores, alem da
 * consistencia do historico e da independencia entre simulacoes comparadas.
 */
#include "test_utils.h"

#include "allocation.h"
#include "metrics.h"
#include "scenarios.h"
#include "simulation.h"

/* Ultimo registro do historico (NULL quando vazio). */
static const OperationRecord *last_record(const Simulation *simulation)
{
    if (simulation->history_count == 0) {
        return NULL;
    }
    return &simulation->history[simulation->history_count - 1u];
}

/* Localiza no historico a alocacao de um processo com o tamanho pedido. */
static const OperationRecord *find_allocation(const Simulation *simulation,
                                              int process_id, int size)
{
    size_t index;

    for (index = 0; index < simulation->history_count; index++) {
        const OperationRecord *record = &simulation->history[index];

        if (record->type == OPERATION_ALLOCATE &&
            record->process_id == process_id && record->size == size) {
            return record;
        }
    }
    return NULL;
}

static void test_init_cria_memoria_livre(void)
{
    Simulation simulation;

    TEST_CHECK(simulation_init(&simulation, 1024, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK_EQ_INT(simulation.memory.total_size, 1024,
                      "tamanho total preservado");
    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 1024,
                      "toda a memoria deveria estar livre");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), 0,
                      "nenhum bloco deveria estar ocupado");
    TEST_CHECK_EQ_INT(memory_block_count(&simulation.memory), 1,
                      "memoria livre comeca com um unico bloco");
    TEST_CHECK_EQ_INT(simulation.history_count, 0, "historico vazio");
    TEST_CHECK_EQ_INT(simulation.history_capacity, 16,
                      "capacidade inicial do historico");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 0,
                      "contador de sucessos zerado");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0,
                      "contador de falhas zerado");
    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 0,
                      "blocos examinados zerados");
    TEST_CHECK_EQ_DOUBLE(simulation.peak_fragmentation, 0.0, 1e-9,
                         "pico de fragmentacao inicial");
    TEST_CHECK_EQ_INT(simulation.peak_fragmentation_defined, 0,
                      "pico de fragmentacao inicialmente indefinido");
    TEST_CHECK_EQ_INT(simulation.strategy, STRATEGY_FIRST_FIT,
                      "estrategia registrada");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos init");

    simulation_destroy(&simulation);
    /* destroy deve ser idempotente. */
    simulation_destroy(&simulation);
}

static void test_reset_limpa_estado(void)
{
    Simulation simulation;
    Operation allocate = {OPERATION_ALLOCATE, 1, 100};
    Operation allocate_again = {OPERATION_ALLOCATE, 2, 50};

    TEST_CHECK(simulation_init(&simulation, 256, STRATEGY_BEST_FIT),
               "init deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &allocate),
               "alocacao inicial deveria funcionar");
    TEST_CHECK(simulation_reset(&simulation), "reset deveria funcionar");

    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 256,
                      "reset devolve toda a memoria");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), 0,
                      "reset zera a memoria ocupada");
    TEST_CHECK_EQ_INT(memory_block_count(&simulation.memory), 1,
                      "reset reconstroi um unico bloco livre");
    TEST_CHECK_EQ_INT(simulation.history_count, 0, "reset esvazia historico");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 0,
                      "reset zera sucessos");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0, "reset zera falhas");
    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 0,
                      "reset zera blocos examinados");
    TEST_CHECK_EQ_DOUBLE(simulation.peak_fragmentation, 0.0, 1e-9,
                         "reset zera o pico de fragmentacao");
    TEST_CHECK_EQ_INT(simulation.peak_fragmentation_defined, 0,
                      "reset torna o pico indefinido");
    TEST_CHECK_EQ_INT(simulation.strategy, STRATEGY_BEST_FIT,
                      "reset preserva a estrategia");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos reset");

    TEST_CHECK(simulation_execute(&simulation, &allocate_again),
               "simulacao deve voltar a funcionar apos reset");
    TEST_CHECK_EQ_INT(simulation.history_count, 1,
                      "historico volta a crescer apos reset");

    simulation_destroy(&simulation);
}

static void test_alocacao_bem_sucedida(void)
{
    Simulation simulation;
    Operation operation = {OPERATION_ALLOCATE, 1, 100};
    const OperationRecord *record;

    TEST_CHECK(simulation_init(&simulation, 1024, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &operation),
               "alocacao deveria ser executada");

    record = last_record(&simulation);
    TEST_CHECK(record != NULL, "historico deveria ter um registro");
    if (record != NULL) {
        TEST_CHECK_EQ_INT(record->type, OPERATION_ALLOCATE, "tipo registrado");
        TEST_CHECK_EQ_INT(record->process_id, 1, "processo registrado");
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_SUCCESS,
                          "status de sucesso");
        TEST_CHECK_EQ_INT(record->start_address, 0, "endereco inicial");
        TEST_CHECK_EQ_INT(record->blocks_examined, 1,
                          "unico bloco livre examinado");
        TEST_CHECK(record->message[0] != '\0', "mensagem didatica preenchida");
    }

    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 1,
                      "blocos examinados acumulados");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 1,
                      "contador de sucessos");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0,
                      "nenhuma falha de alocacao");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), 100,
                      "memoria ocupada");
    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 924,
                      "memoria livre");
    TEST_CHECK_EQ_INT(simulation.history_count, 1, "um registro no historico");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos alocar");

    simulation_destroy(&simulation);
}

static void test_alocacao_sem_espaco(void)
{
    Simulation simulation;
    Operation operation = {OPERATION_ALLOCATE, 1, 200};
    const OperationRecord *record;

    TEST_CHECK(simulation_init(&simulation, 100, STRATEGY_WORST_FIT),
               "init deveria funcionar");
    TEST_CHECK(!simulation_execute(&simulation, &operation),
               "alocacao sem espaco deveria falhar");

    record = last_record(&simulation);
    TEST_CHECK(record != NULL, "falha tambem gera registro");
    if (record != NULL) {
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_FAILURE,
                          "status de falha");
        TEST_CHECK_EQ_INT(record->blocks_examined, 0,
                          "pedido maior que a memoria dispensa busca");
        TEST_CHECK(record->message[0] != '\0', "mensagem de falha preenchida");
    }

    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 0,
                      "curto-circuito nao acumula blocos examinados");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 0,
                      "nenhum sucesso de alocacao");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 1,
                      "falha de alocacao contabilizada");
    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 100,
                      "memoria permanece livre");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), 0,
                      "memoria permanece vazia");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos falha de alocacao");

    simulation_destroy(&simulation);
}

static void test_free_existente(void)
{
    Simulation simulation;
    Operation allocate = {OPERATION_ALLOCATE, 7, 128};
    Operation free_operation = {OPERATION_FREE, 7, 0};
    const OperationRecord *record;

    TEST_CHECK(simulation_init(&simulation, 512, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &allocate),
               "alocacao inicial deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &free_operation),
               "liberacao deveria funcionar");

    record = last_record(&simulation);
    TEST_CHECK(record != NULL, "liberacao gera registro");
    if (record != NULL) {
        TEST_CHECK_EQ_INT(record->type, OPERATION_FREE, "tipo FREE");
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_SUCCESS,
                          "status de sucesso na liberacao");
        TEST_CHECK_EQ_INT(record->start_address, 0,
                          "endereco do bloco antes de liberar");
        TEST_CHECK_EQ_INT(record->size, 128, "tamanho do bloco liberado");
    }

    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 512,
                      "bloco volta a ficar livre e coalesce");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), 0,
                      "nenhum bloco ocupado");
    TEST_CHECK_EQ_INT(memory_block_count(&simulation.memory), 1,
                      "coalescencia devolve um unico bloco livre");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0,
                      "FREE nao conta como falha de alocacao");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos liberar");

    simulation_destroy(&simulation);
}

static void test_free_inexistente(void)
{
    Simulation simulation;
    Operation free_operation = {OPERATION_FREE, 99, 0};
    const OperationRecord *record;

    TEST_CHECK(simulation_init(&simulation, 256, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK(!simulation_execute(&simulation, &free_operation),
               "FREE de processo ausente deveria falhar");

    record = last_record(&simulation);
    TEST_CHECK(record != NULL, "falha de FREE gera registro");
    if (record != NULL) {
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_FAILURE,
                          "status de falha no FREE");
    }

    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0,
                      "falha de FREE nao entra em failed_allocations");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 0,
                      "nenhuma alocacao bem-sucedida");
    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 256,
                      "memoria permanece intacta");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos FREE inexistente");

    simulation_destroy(&simulation);
}

static void test_historico_cresce_em_ordem(void)
{
    Simulation simulation;
    size_t index;

    TEST_CHECK(simulation_init(&simulation, 1000, STRATEGY_FIRST_FIT),
               "init deveria funcionar");

    for (index = 0; index < 40; index++) {
        Operation operation;

        operation.type = OPERATION_ALLOCATE;
        operation.process_id = (int)index + 1;
        operation.size = 1;
        TEST_CHECK(simulation_execute(&simulation, &operation),
                   "alocacao %zu deveria funcionar", index);
    }

    TEST_CHECK_EQ_INT(simulation.history_count, 40,
                      "historico deveria conter 40 registros");
    TEST_CHECK_EQ_INT(simulation.history_capacity, 64,
                      "capacidade deveria dobrar de 16 para 64");

    for (index = 0; index < simulation.history_count; index++) {
        const OperationRecord *record = &simulation.history[index];

        TEST_CHECK_EQ_INT(record->process_id, (int)index + 1,
                          "ordem dos registros preservada");
        TEST_CHECK_EQ_INT(record->start_address, (int)index,
                          "endereco escolhido pelo First Fit");
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_SUCCESS,
                          "todos os registros de sucesso");
        TEST_CHECK_EQ_INT(record->blocks_examined, (int)index + 1,
                          "blocos examinados pela busca sequencial");
    }

    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 820,
                      "soma exata de 1 a 40 blocos examinados");

    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos 40 alocacoes");
    simulation_destroy(&simulation);
}

static void test_metricas_coerentes(void)
{
    Simulation simulation;
    Operation success = {OPERATION_ALLOCATE, 1, 50};
    Operation failure = {OPERATION_ALLOCATE, 2, 300};
    Metrics metrics;

    TEST_CHECK(simulation_init(&simulation, 200, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &success),
               "primeira alocacao deveria funcionar");
    TEST_CHECK(!simulation_execute(&simulation, &failure),
               "segunda alocacao deveria falhar");

    metrics = simulation_metrics(&simulation);
    TEST_CHECK_EQ_INT(metrics.total_requests, 2, "duas solicitacoes");
    TEST_CHECK_EQ_INT(metrics.successful_allocations, 1, "um sucesso");
    TEST_CHECK_EQ_INT(metrics.failed_allocations, 1, "uma falha");
    TEST_CHECK_EQ_INT(metrics.total_memory, 200, "memoria total");
    TEST_CHECK_EQ_INT(metrics.free_memory, 150, "memoria livre");
    TEST_CHECK_EQ_INT(metrics.used_memory, 50, "memoria ocupada");
    TEST_CHECK_EQ_INT(metrics.largest_free_block, 150,
                      "maior bloco livre contiguo");
    TEST_CHECK_EQ_INT(metrics.free_block_count, 1, "um bloco livre");
    TEST_CHECK_EQ_INT(metrics.occupied_block_count, 1, "um bloco ocupado");
    TEST_CHECK_EQ_INT(metrics.total_blocks_examined, 1,
                      "uma busca de um bloco e um curto-circuito");
    TEST_CHECK_EQ_INT(metrics.fragmentation_defined, 1,
                      "fragmentacao definida com memoria livre");
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 0.0, 1e-9,
                         "bloco unico nao e fragmentado");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.0, 1e-9,
                         "pico coerente com um unico bloco livre");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "pico definido apos operacoes com memoria livre");
    TEST_CHECK_EQ_DOUBLE(metrics.utilization_percent, 25.0, 1e-9,
                         "utilizacao de 25 por cento");
    TEST_CHECK_EQ_DOUBLE(metrics.success_rate_percent, 50.0, 1e-9,
                         "taxa de sucesso de 50 por cento");
    TEST_CHECK_EQ_DOUBLE(metrics.failure_rate_percent, 50.0, 1e-9,
                         "taxa de falha de 50 por cento");

    simulation_destroy(&simulation);
}

static void test_sequencia_completa(void)
{
    static const Operation operations[] = {
        {OPERATION_ALLOCATE, 1, 250}, {OPERATION_ALLOCATE, 2, 250},
        {OPERATION_ALLOCATE, 3, 250}, {OPERATION_ALLOCATE, 4, 250},
        {OPERATION_FREE, 2, 0},       {OPERATION_FREE, 4, 0},
        {OPERATION_ALLOCATE, 5, 400}, {OPERATION_ALLOCATE, 6, 200}
    };
    const size_t count = sizeof(operations) / sizeof(operations[0]);
    Simulation simulation;
    Metrics metrics;
    int successes;

    TEST_CHECK(simulation_init(&simulation, 1024, STRATEGY_FIRST_FIT),
               "init deveria funcionar");

    successes = simulation_execute_sequence(&simulation, operations, count);
    TEST_CHECK_EQ_INT(successes, 7,
                      "sete das oito operacoes devem ter sucesso");
    TEST_CHECK_EQ_INT(simulation.history_count, count,
                      "todas as operacoes ficam registradas");

    metrics = simulation_metrics(&simulation);
    TEST_CHECK_EQ_INT(metrics.total_requests, 6,
                      "seis solicitacoes de alocacao");
    TEST_CHECK_EQ_INT(metrics.successful_allocations, 5,
                      "cinco alocacoes atendidas");
    TEST_CHECK_EQ_INT(metrics.failed_allocations, 1,
                      "uma alocacao nao atendida");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos sequencia completa");

    simulation_destroy(&simulation);
}

static void test_comparacao_independente(void)
{
    static const Operation operations[] = {
        {OPERATION_ALLOCATE, 1, 100},
        {OPERATION_ALLOCATE, 2, 200},
        {OPERATION_ALLOCATE, 3, 150}
    };
    const size_t count = sizeof(operations) / sizeof(operations[0]);
    Simulation simulations[STRATEGY_COUNT];
    Metrics metrics[STRATEGY_COUNT];
    int strategy;

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        TEST_CHECK(simulation_init(&simulations[strategy], 1024,
                                   (AllocationStrategy)strategy),
                   "init da estrategia %d", strategy);
        TEST_CHECK_EQ_INT(
            simulation_execute_sequence(&simulations[strategy], operations,
                                        count),
            (int)count, "sequencia completa na estrategia %d", strategy);
        TEST_CHECK_MEMORY_VALID(&simulations[strategy].memory,
                                "estrategia %d integra", strategy);
        metrics[strategy] = simulation_metrics(&simulations[strategy]);
        TEST_CHECK_EQ_INT(metrics[strategy].total_memory, 1024,
                          "memoria total independente");
        TEST_CHECK_EQ_INT(metrics[strategy].used_memory, 450,
                          "mesma sequencia usa a mesma memoria");
    }

    /* Alterar uma simulacao nao pode afetar as demais. */
    {
        Operation free_operation = {OPERATION_FREE, 1, 0};

        TEST_CHECK(simulation_execute(&simulations[0], &free_operation),
                   "liberacao apenas na primeira simulacao");
    }

    TEST_CHECK(memory_find_process(&simulations[0].memory, 1) == NULL,
               "processo 1 removido da primeira simulacao");
    TEST_CHECK(memory_find_process(&simulations[1].memory, 1) != NULL,
               "processo 1 permanece na segunda simulacao");
    TEST_CHECK(memory_find_process(&simulations[2].memory, 1) != NULL,
               "processo 1 permanece na terceira simulacao");
    TEST_CHECK_EQ_INT(memory_total_used(&simulations[1].memory), 450,
                      "segunda simulacao intacta");

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        TEST_CHECK_MEMORY_VALID(&simulations[strategy].memory,
                                "estrategia %d final", strategy);
        simulation_destroy(&simulations[strategy]);
    }
}

static void test_cenario3_escolhas_distintas(void)
{
    static const int expected_six[STRATEGY_COUNT] = {0, 400, 700};
    static const int expected_seven[STRATEGY_COUNT] = {700, 0, 0};
    ScenarioSet set;
    const Scenario *scenario;
    Simulation simulations[STRATEGY_COUNT];
    int initialized[STRATEGY_COUNT] = {0, 0, 0};
    int all_ok = 1;
    int strategy;

    TEST_CHECK(scenarios_create_controlled(&set),
               "conjunto de cenarios deveria ser criado");
    scenario = scenarios_get(&set, 2);
    TEST_CHECK(scenario != NULL, "cenario 3 deveria existir");
    if (scenario == NULL) {
        scenarios_destroy(&set);
        return;
    }

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        if (!simulation_init(&simulations[strategy], scenario->memory_size,
                             (AllocationStrategy)strategy)) {
            TEST_CHECK(0, "init da estrategia %d", strategy);
            all_ok = 0;
            break;
        }
        initialized[strategy] = 1;

        (void)simulation_execute_sequence(
            &simulations[strategy], scenario->operations,
            scenario->operation_count);
        TEST_CHECK_MEMORY_VALID(&simulations[strategy].memory,
                                "memoria integra na estrategia %d", strategy);
    }

    if (all_ok) {
        for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
            const OperationRecord *record_six =
                find_allocation(&simulations[strategy], 6, 180);
            const OperationRecord *record_seven =
                find_allocation(&simulations[strategy], 7, 250);

            TEST_CHECK(record_six != NULL,
                       "ALLOCATE 6 180 deveria estar no historico");
            if (record_six != NULL) {
                TEST_CHECK_EQ_INT(record_six->status,
                                  OPERATION_STATUS_SUCCESS,
                                  "ALLOCATE 6 180 deveria ter sucesso");
                TEST_CHECK_EQ_INT(record_six->start_address,
                                  expected_six[strategy],
                                  "endereco do pedido 180 na estrategia %d",
                                  strategy);
            }

            TEST_CHECK(record_seven != NULL,
                       "ALLOCATE 7 250 deveria estar no historico");
            if (record_seven != NULL) {
                TEST_CHECK_EQ_INT(record_seven->status,
                                  OPERATION_STATUS_SUCCESS,
                                  "ALLOCATE 7 250 deveria ter sucesso");
                TEST_CHECK_EQ_INT(record_seven->start_address,
                                  expected_seven[strategy],
                                  "endereco do pedido 250 na estrategia %d",
                                  strategy);
            }
        }
    }

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        if (initialized[strategy]) {
            simulation_destroy(&simulations[strategy]);
        }
    }
    scenarios_destroy(&set);
}

static void test_fragmentacao_pico_cenario2(void)
{
    static const Operation operations[] = {
        {OPERATION_ALLOCATE, 1, 256},
        {OPERATION_ALLOCATE, 2, 256},
        {OPERATION_ALLOCATE, 3, 256},
        {OPERATION_ALLOCATE, 4, 256},
        {OPERATION_FREE, 2, 0},
        {OPERATION_FREE, 4, 0}
    };
    const size_t count = sizeof(operations) / sizeof(operations[0]);
    Simulation simulation;
    Metrics metrics;
    Operation big = {OPERATION_ALLOCATE, 5, 400};
    size_t index;

    TEST_CHECK(simulation_init(&simulation, 1024, STRATEGY_FIRST_FIT),
               "init do cenario de fragmentacao");
    for (index = 0; index < count; index++) {
        TEST_CHECK(simulation_execute(&simulation, &operations[index]),
                   "operacao %zu do cenario 2", index);
    }

    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), 512,
                      "512 KB livres apos liberar 2 e 4");
    TEST_CHECK_EQ_INT(memory_largest_free_block(&simulation.memory), 256,
                      "maior bloco contiguo de 256 KB");
    TEST_CHECK_EQ_DOUBLE(
        metrics_fragmentation(memory_largest_free_block(&simulation.memory),
                              memory_total_free(&simulation.memory)),
        0.5, 1e-6, "fragmentacao de 50 por cento no estado alcancado");
    TEST_CHECK_EQ_DOUBLE(simulation.peak_fragmentation, 0.5, 1e-6,
                         "pico registrado pela simulacao");
    TEST_CHECK_EQ_INT(simulation.peak_fragmentation_defined, 1,
                      "pico definido no estado alcancado");

    metrics = simulation_metrics(&simulation);
    TEST_CHECK_EQ_DOUBLE(metrics.fragmentation, 0.5, 1e-6,
                         "snapshot tambem mostra 50 por cento");
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.5, 1e-6,
                         "metricas repassam o pico");
    TEST_CHECK_EQ_INT(metrics.peak_fragmentation_defined, 1,
                      "metricas repassam o indicador de pico");

    TEST_CHECK(!simulation_execute(&simulation, &big),
               "pedido de 400 KB deve falhar");
    metrics = simulation_metrics(&simulation);
    TEST_CHECK_EQ_DOUBLE(metrics.peak_fragmentation, 0.5, 1e-6,
                         "falha nao apaga o pico de 50 por cento");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos falha do pedido 400");

    simulation_destroy(&simulation);
}

static void test_operacao_tipo_invalido(void)
{
    Simulation simulation;
    Operation allocate = {OPERATION_ALLOCATE, 1, 100};
    Operation invalid;
    const OperationRecord *record;
    int free_before;
    int used_before;
    size_t blocks_before;

    TEST_CHECK(simulation_init(&simulation, 256, STRATEGY_FIRST_FIT),
               "init deveria funcionar");
    TEST_CHECK(simulation_execute(&simulation, &allocate),
               "alocacao valida inicial");

    free_before = memory_total_free(&simulation.memory);
    used_before = memory_total_used(&simulation.memory);
    blocks_before = memory_block_count(&simulation.memory);

    invalid.type = (OperationType)99;
    invalid.process_id = 2;
    invalid.size = 50;
    TEST_CHECK(!simulation_execute(&simulation, &invalid),
               "tipo invalido deve ser recusado");

    record = last_record(&simulation);
    TEST_CHECK(record != NULL, "tipo invalido tambem gera registro");
    if (record != NULL) {
        TEST_CHECK_EQ_INT(record->status, OPERATION_STATUS_INVALID,
                          "status INVALID para tipo desconhecido");
        TEST_CHECK_EQ_INT(record->blocks_examined, 0,
                          "tipo invalido nao realiza busca");
    }

    TEST_CHECK_EQ_INT(simulation.history_count, 2,
                      "tipo invalido entra no historico");
    TEST_CHECK_EQ_INT(simulation.successful_allocations, 1,
                      "tipo invalido nao conta sucesso");
    TEST_CHECK_EQ_INT(simulation.failed_allocations, 0,
                      "tipo invalido nao conta falha de alocacao");
    TEST_CHECK_EQ_INT(simulation.total_blocks_examined, 1,
                      "tipo invalido nao examina blocos");
    TEST_CHECK_EQ_INT(memory_total_free(&simulation.memory), free_before,
                      "tipo invalido nao altera a memoria livre");
    TEST_CHECK_EQ_INT(memory_total_used(&simulation.memory), used_before,
                      "tipo invalido nao altera a memoria ocupada");
    TEST_CHECK_EQ_INT(memory_block_count(&simulation.memory), blocks_before,
                      "tipo invalido nao altera os blocos");
    TEST_CHECK_MEMORY_VALID(&simulation.memory, "apos tipo invalido");

    simulation_destroy(&simulation);
}

static void test_estresse_deterministico(void)
{
    ScenarioSet first;
    ScenarioSet second;
    ScenarioSet different;
    const Scenario *a;
    const Scenario *b;
    const Scenario *c;
    size_t index;
    int same = 1;
    int differs = 0;
    int sequence_valid = 1;
    unsigned char allocated[9] = {0};

    TEST_CHECK(scenarios_create_controlled(&first),
               "conjunto base da primeira geracao");
    TEST_CHECK(scenarios_append_stress(&first, 1024, 8, 50, 300, 200, 42u),
               "primeira geracao de estresse");
    TEST_CHECK(scenarios_create_controlled(&second),
               "conjunto base da segunda geracao");
    TEST_CHECK(scenarios_append_stress(&second, 1024, 8, 50, 300, 200, 42u),
               "segunda geracao com a mesma semente");
    TEST_CHECK(scenarios_create_controlled(&different),
               "conjunto base da geracao com outra semente");
    TEST_CHECK(scenarios_append_stress(&different, 1024, 8, 50, 300, 200,
                                       43u),
               "geracao com semente diferente");

    a = scenarios_get(&first, 3);
    b = scenarios_get(&second, 3);
    c = scenarios_get(&different, 3);
    TEST_CHECK(a != NULL && b != NULL && c != NULL,
               "cenarios de estresse deveriam existir");

    if (a != NULL && b != NULL && c != NULL) {
        TEST_CHECK_EQ_INT(a->operation_count, 200,
                          "quantidade de operacoes do estresse");
        TEST_CHECK_EQ_INT(a->operation_count, b->operation_count,
                          "mesma semente preserva a quantidade");

        for (index = 0; index < a->operation_count; index++) {
            const Operation *operation = &a->operations[index];
            int pid = operation->process_id;

            if (pid < 1 || pid > 8) {
                sequence_valid = 0;
                continue;
            }
            if (operation->type == OPERATION_ALLOCATE) {
                if (operation->size < 50 || operation->size > 300 ||
                    allocated[pid]) {
                    sequence_valid = 0;
                } else {
                    allocated[pid] = 1;
                }
            } else if (operation->type == OPERATION_FREE) {
                if (!allocated[pid]) {
                    sequence_valid = 0;
                } else {
                    allocated[pid] = 0;
                }
            } else {
                sequence_valid = 0;
            }
        }
        TEST_CHECK(sequence_valid,
                   "sequencia so libera processo alocado antes e usa faixa "
                   "valida de tamanhos");

        for (index = 0; index < a->operation_count; index++) {
            if (a->operations[index].type != b->operations[index].type ||
                a->operations[index].process_id !=
                    b->operations[index].process_id ||
                a->operations[index].size != b->operations[index].size) {
                same = 0;
            }
        }
        TEST_CHECK(same, "mesma semente gera sequencia identica");

        if (a->operation_count != c->operation_count) {
            differs = 1;
        } else {
            for (index = 0; index < a->operation_count; index++) {
                if (a->operations[index].type != c->operations[index].type ||
                    a->operations[index].process_id !=
                        c->operations[index].process_id ||
                    a->operations[index].size != c->operations[index].size) {
                    differs = 1;
                }
            }
        }
        TEST_CHECK(differs, "semente diferente gera sequencia diferente");
    }

    scenarios_destroy(&first);
    scenarios_destroy(&second);
    scenarios_destroy(&different);
}

int main(void)
{
    printf("Testes da simulacao\n");

    TEST_RUN(test_init_cria_memoria_livre);
    TEST_RUN(test_reset_limpa_estado);
    TEST_RUN(test_alocacao_bem_sucedida);
    TEST_RUN(test_alocacao_sem_espaco);
    TEST_RUN(test_free_existente);
    TEST_RUN(test_free_inexistente);
    TEST_RUN(test_historico_cresce_em_ordem);
    TEST_RUN(test_metricas_coerentes);
    TEST_RUN(test_sequencia_completa);
    TEST_RUN(test_comparacao_independente);
    TEST_RUN(test_cenario3_escolhas_distintas);
    TEST_RUN(test_fragmentacao_pico_cenario2);
    TEST_RUN(test_operacao_tipo_invalido);
    TEST_RUN(test_estresse_deterministico);

    printf("\n%d verificacoes, %d falhas\n", test_checks, test_failures);
    return TEST_SUMMARY();
}
