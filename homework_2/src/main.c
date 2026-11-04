/*
 * main.c - Interface de terminal do simulador de memoria contigua.
 *
 * CONCEITO TEORICO
 * A interface apenas orquestra os modulos: monta cenarios, executa sequencias,
 * consulta metricas e delega TODA a apresentacao para visualization.c. O modo
 * interativo explora os cenarios controlados e um cenario personalizado; o
 * modo por linha de comando garante reprodutibilidade (mesmos parametros,
 * mesma saida, inclusive para o cenario de estresse).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocation.h"
#include "memory.h"
#include "metrics.h"
#include "scenarios.h"
#include "simulation.h"
#include "visualization.h"

#define LINE_BUFFER 256
#define CUSTOM_MEMORY_SIZE 1024

#define DEFAULT_STRESS_MEMORY 1024
#define DEFAULT_STRESS_PROCESSES 8
#define DEFAULT_STRESS_MIN_SIZE 50
#define DEFAULT_STRESS_MAX_SIZE 300
#define DEFAULT_STRESS_OPERATIONS 200
#define DEFAULT_STRESS_SEED 42u

/* Parametros do cenario de estresse (todos com valores padrao). */
typedef struct {
    int memory_size;
    int process_count;
    int min_size;
    int max_size;
    int operation_count;
    unsigned int seed;
    int custom_memory_size;
} StressConfig;

/* ------------------------------------------------------------------------- */
/* Utilitarios de leitura e conversao                                         */
/* ------------------------------------------------------------------------- */

static void stress_config_defaults(StressConfig *config)
{
    config->memory_size = DEFAULT_STRESS_MEMORY;
    config->process_count = DEFAULT_STRESS_PROCESSES;
    config->min_size = DEFAULT_STRESS_MIN_SIZE;
    config->max_size = DEFAULT_STRESS_MAX_SIZE;
    config->operation_count = DEFAULT_STRESS_OPERATIONS;
    config->seed = DEFAULT_STRESS_SEED;
    config->custom_memory_size = CUSTOM_MEMORY_SIZE;
}

/*
 * read_input_line - Le uma linha e descarta o excedente.
 *
 * Se a linha nao couber no buffer, o restante e drenado ate o fim da linha
 * para que o trecho excedente nao seja interpretado como um segundo comando.
 * Devolve 0 apenas em EOF sem dados; "truncated" indica que houve descarte.
 */
static int read_input_line(char *buffer, size_t size, int *truncated)
{
    size_t length;
    int ch;

    *truncated = 0;
    if (fgets(buffer, (int)size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1u] == '\n') {
        return 1;
    }

    ch = fgetc(stdin);
    if (ch == '\n' || ch == EOF) {
        return 1;
    }

    *truncated = 1;
    while (ch != EOF && ch != '\n') {
        ch = fgetc(stdin);
    }
    return 1;
}

/* Remove espacos nas extremidades e o terminador de linha. */
static void trim_line(char *text)
{
    char *start = text;
    size_t length;

    while (*start != '\0' && isspace((unsigned char)*start)) {
        start++;
    }
    if (start != text) {
        memmove(text, start, strlen(start) + 1u);
    }

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1u])) {
        text[--length] = '\0';
    }
}

/* Converte a linha inteira em long; devolve 0 se houver qualquer lixo. */
static int parse_long_text(const char *text, long *value)
{
    char *end = NULL;
    long parsed;

    if (text == NULL || value == NULL || *text == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text) {
        return 0;
    }
    while (*end != '\0' && isspace((unsigned char)*end)) {
        end++;
    }
    if (*end != '\0') {
        return 0;
    }

    *value = parsed;
    return 1;
}

/* Le um inteiro a partir de um cursor, avancando-o. */
static int next_long(const char **cursor, long *value)
{
    const char *text = *cursor;
    char *end = NULL;
    long parsed;

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return 0;
    }

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text) {
        return 0;
    }

    *cursor = end;
    *value = parsed;
    return 1;
}

/* Verifica se restou apenas espaco em branco. */
static int only_space_left(const char *text)
{
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    return *text == '\0';
}

/* ALLOCATE <pid> <tamanho> */
static int parse_pair(const char *text, int *first, int *second)
{
    const char *cursor = text;
    long a;
    long b;

    if (!next_long(&cursor, &a) || !next_long(&cursor, &b) ||
        !only_space_left(cursor)) {
        return 0;
    }
    if (a < INT_MIN || a > INT_MAX || b < INT_MIN || b > INT_MAX) {
        return 0;
    }

    *first = (int)a;
    *second = (int)b;
    return 1;
}

/* FREE <pid> */
static int parse_single(const char *text, int *value)
{
    const char *cursor = text;
    long a;

    if (!next_long(&cursor, &a) || !only_space_left(cursor)) {
        return 0;
    }
    if (a < INT_MIN || a > INT_MAX) {
        return 0;
    }

    *value = (int)a;
    return 1;
}

static int starts_with(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

/* ------------------------------------------------------------------------- */
/* Montagem dos cenarios                                                      */
/* ------------------------------------------------------------------------- */

static int build_scenarios(ScenarioSet *set, const StressConfig *config)
{
    if (!scenarios_create_controlled(set)) {
        return 0;
    }
    if (!scenarios_append_stress(set, config->memory_size,
                                 config->process_count, config->min_size,
                                 config->max_size, config->operation_count,
                                 config->seed)) {
        scenarios_destroy(set);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------------- */
/* Execucao e exibicao                                                        */
/* ------------------------------------------------------------------------- */

/* Mostra o ultimo registro do historico, quando existir. */
static void print_last_operation(const Simulation *simulation, size_t before)
{
    if (simulation->history_count > before) {
        visualization_print_operation(
            &simulation->history[simulation->history_count - 1u]);
    } else {
        printf("Nao foi possivel registrar a operacao no historico.\n");
    }
}

/* Linha compacta com o estado da memoria imediatamente apos uma operacao. */
static void print_memory_state(const Memory *memory)
{
    int free_memory = memory_total_free(memory);
    int largest = memory_largest_free_block(memory);
    int free_blocks = memory_count_free_blocks(memory);

    if (free_memory > 0) {
        double fragmentation = metrics_fragmentation(largest, free_memory);

        printf("Estado: livre=%d KB | maior bloco=%d KB | "
               "fragmentacao=%.2f%% | blocos livres=%d\n",
               free_memory, largest, fragmentation * 100.0, free_blocks);
    } else {
        printf("Estado: livre=0 KB | maior bloco=0 KB | "
               "fragmentacao=indefinida | blocos livres=%d\n",
               free_blocks);
    }
}

/*
 * execute_scenario_detailed - Executa passo a passo, exibindo o resultado e o
 * mapa de memoria depois de cada operacao. Ao final mostra as metricas.
 */
static int execute_scenario_detailed(const Scenario *scenario,
                                     AllocationStrategy strategy)
{
    Simulation simulation;
    size_t index;
    Metrics metrics;

    if (!simulation_init(&simulation, scenario->memory_size, strategy)) {
        fprintf(stderr, "Falha ao iniciar a simulacao do cenario.\n");
        return 0;
    }

    visualization_print_scenario_header(scenario->name, scenario->description,
                                        scenario->objective,
                                        scenario->memory_size);

    for (index = 0; index < scenario->operation_count; index++) {
        size_t before = simulation.history_count;

        (void)simulation_execute(&simulation, &scenario->operations[index]);
        print_last_operation(&simulation, before);
        print_memory_state(&simulation.memory);
        visualization_print_memory_map(&simulation.memory);
    }

    metrics = simulation_metrics(&simulation);
    visualization_print_metrics(&metrics, allocation_strategy_name(strategy));

    simulation_destroy(&simulation);
    return 1;
}

/*
 * execute_scenario_summary - Executa sem despejar cada passo (usado no cenario
 * de estresse): exibe o estado final e as metricas finais.
 */
static int execute_scenario_summary(const Scenario *scenario,
                                    AllocationStrategy strategy)
{
    Simulation simulation;
    Metrics metrics;

    if (!simulation_init(&simulation, scenario->memory_size, strategy)) {
        fprintf(stderr, "Falha ao iniciar a simulacao do cenario.\n");
        return 0;
    }

    visualization_print_scenario_header(scenario->name, scenario->description,
                                        scenario->objective,
                                        scenario->memory_size);
    (void)simulation_execute_sequence(&simulation, scenario->operations,
                                      scenario->operation_count);

    printf("\nHistorico completo com %zu operacoes omitido; exibindo apenas o "
           "estado final.\n",
           simulation.history_count);
    visualization_print_memory_map(&simulation.memory);

    metrics = simulation_metrics(&simulation);
    visualization_print_metrics(&metrics, allocation_strategy_name(strategy));

    simulation_destroy(&simulation);
    return 1;
}

/*
 * print_address_comparison - Cenario 3: monta a tabela "endereco escolhido".
 *
 * As tres simulacoes executaram a MESMA sequencia, entao o registro i de cada
 * historico corresponde ao mesmo passo. A tabela e preenchida com o desfecho
 * real de cada estrategia e entregue a camada de visualizacao.
 */
static void print_address_comparison(const Scenario *scenario,
                                     const Simulation *simulations)
{
    static const char *const names[STRATEGY_COUNT] = {"First Fit", "Best Fit",
                                                      "Worst Fit"};
    AddressComparisonRow *rows;
    size_t row_count = scenario->operation_count;
    size_t index;
    int strategy;

    if (row_count == 0) {
        return;
    }
    if (row_count > (size_t)-1 / sizeof(*rows)) {
        fprintf(stderr, "Tabela de enderecos grande demais.\n");
        return;
    }

    rows = malloc(row_count * sizeof(*rows));
    if (rows == NULL) {
        fprintf(stderr, "Falha ao montar a tabela de enderecos.\n");
        return;
    }

    for (index = 0; index < row_count; index++) {
        const Operation *operation = &scenario->operations[index];

        rows[index].step = (int)(index + 1u);
        if (operation->type == OPERATION_ALLOCATE) {
            snprintf(rows[index].operation, sizeof(rows[index].operation),
                     "ALLOCATE P%d %d KB", operation->process_id,
                     operation->size);
        } else if (operation->type == OPERATION_FREE) {
            snprintf(rows[index].operation, sizeof(rows[index].operation),
                     "FREE P%d", operation->process_id);
        } else {
            snprintf(rows[index].operation, sizeof(rows[index].operation),
                     "OPERACAO");
        }

        for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
            const OperationRecord *record = NULL;

            if (index < simulations[strategy].history_count) {
                record = &simulations[strategy].history[index];
            }

            if (record != NULL &&
                record->status == OPERATION_STATUS_SUCCESS) {
                rows[index].success[strategy] = 1;
                rows[index].address[strategy] = record->start_address;
            } else {
                rows[index].success[strategy] = 0;
                rows[index].address[strategy] = -1;
            }
        }
    }

    visualization_print_address_comparison(names, rows, row_count);
    free(rows);
}

/*
 * compare_scenario - Executa a mesma sequencia em tres simulacoes
 * independentes (uma por estrategia) e exibe a tabela comparativa.
 */
static void compare_scenario(const Scenario *scenario, int show_addresses)
{
    static const char *const names[STRATEGY_COUNT] = {"First Fit", "Best Fit",
                                                      "Worst Fit"};
    Simulation simulations[STRATEGY_COUNT];
    Metrics metrics[STRATEGY_COUNT];
    const Metrics *metric_pointers[STRATEGY_COUNT];
    int initialized[STRATEGY_COUNT] = {0, 0, 0};
    int strategy;
    int all_ok = 1;

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        if (!simulation_init(&simulations[strategy], scenario->memory_size,
                             (AllocationStrategy)strategy)) {
            fprintf(stderr, "Falha ao iniciar a simulacao comparativa.\n");
            all_ok = 0;
            break;
        }
        initialized[strategy] = 1;

        (void)simulation_execute_sequence(
            &simulations[strategy], scenario->operations,
            scenario->operation_count);
        metrics[strategy] = simulation_metrics(&simulations[strategy]);
        metric_pointers[strategy] = &metrics[strategy];
    }

    if (all_ok) {
        visualization_print_scenario_header(
            scenario->name, scenario->description, scenario->objective,
            scenario->memory_size);
        visualization_print_comparison(names, metric_pointers,
                                       STRATEGY_COUNT);

        if (show_addresses) {
            print_address_comparison(scenario, simulations);
        }
    }

    for (strategy = 0; strategy < STRATEGY_COUNT; strategy++) {
        if (initialized[strategy]) {
            simulation_destroy(&simulations[strategy]);
        }
    }
}

/* Executa um cenario com a estrategia escolhida (modo nao interativo). */
static void execute_scenario(const Scenario *scenario, int strategy_choice,
                             int scenario_index)
{
    if (strategy_choice == 4) {
        compare_scenario(scenario, scenario_index == 2);
    } else if (scenario_index == 3) {
        (void)execute_scenario_summary(
            scenario, (AllocationStrategy)(strategy_choice - 1));
    } else {
        (void)execute_scenario_detailed(
            scenario, (AllocationStrategy)(strategy_choice - 1));
    }
}

/* ------------------------------------------------------------------------- */
/* Modo nao interativo                                                        */
/* ------------------------------------------------------------------------- */

static void print_help(const char *program_name)
{
    printf("Uso: %s [opcoes]\n", program_name);
    printf("\n");
    printf("Sem opcoes, inicia o modo interativo.\n");
    printf("\n");
    printf("Opcoes gerais:\n");
    printf("  -h, --help              mostra esta ajuda e sai\n");
    printf("  --scenario=N            executa apenas o cenario N (1..4)\n");
    printf("  --strategy=S            1=First Fit, 2=Best Fit, 3=Worst Fit,\n");
    printf("                          4=comparar todos (padrao)\n");
    printf("\n");
    printf("Parametros do cenario de estresse:\n");
    printf("  --stress-mem=KB         memoria total (padrao %d)\n",
           DEFAULT_STRESS_MEMORY);
    printf("  --stress-procs=N        numero de processos (padrao %d)\n",
           DEFAULT_STRESS_PROCESSES);
    printf("  --stress-min=KB         tamanho minimo (padrao %d)\n",
           DEFAULT_STRESS_MIN_SIZE);
    printf("  --stress-max=KB         tamanho maximo (padrao %d)\n",
           DEFAULT_STRESS_MAX_SIZE);
    printf("  --stress-ops=N          numero de operacoes (padrao %d)\n",
           DEFAULT_STRESS_OPERATIONS);
    printf("  --stress-seed=N         semente do PRNG (padrao %u)\n",
           DEFAULT_STRESS_SEED);
    printf("\n");
    printf("Cenario personalizado:\n");
    printf("  --custom-mem=KB         memoria do cenario personalizado "
           "(padrao %d)\n",
           CUSTOM_MEMORY_SIZE);
    printf("\n");
    printf("Informar --scenario ou --strategy ativa o modo nao interativo.\n");
}

static int run_non_interactive(int scenario_choice, int strategy_choice,
                               const StressConfig *config)
{
    ScenarioSet set;
    int index;

    if (!build_scenarios(&set, config)) {
        fprintf(stderr, "Falha ao montar os cenarios.\n");
        return 1;
    }

    for (index = 0; index < 4; index++) {
        const Scenario *scenario;

        if (scenario_choice != 0 && index != scenario_choice - 1) {
            continue;
        }
        scenario = scenarios_get(&set, (size_t)index);
        if (scenario == NULL) {
            continue;
        }
        execute_scenario(scenario, strategy_choice, index);
    }

    scenarios_destroy(&set);
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Modo interativo: cenario personalizado                                     */
/* ------------------------------------------------------------------------- */

static void custom_help(void)
{
    printf("Comandos disponiveis:\n");
    printf("  ALLOCATE <pid> <tamanho>  solicita um bloco\n");
    printf("  FREE <pid>                libera o processo\n");
    printf("  MAP                       mostra o mapa da memoria\n");
    printf("  HISTORY                   mostra o historico de operacoes\n");
    printf("  METRICS                   mostra as metricas atuais\n");
    printf("  HELP                      mostra esta ajuda\n");
    printf("  QUIT                      encerra o cenario personalizado\n");
}

static void run_custom_scenario(int memory_size)
{
    Simulation simulation;
    char line[LINE_BUFFER];
    long strategy_value;
    int truncated;

    printf("\nCenario personalizado (memoria de %d KB)\n", memory_size);
    printf("Escolha a estrategia (1=First Fit, 2=Best Fit, 3=Worst Fit): ");
    fflush(stdout);

    if (!read_input_line(line, sizeof(line), &truncated)) {
        return;
    }
    if (truncated) {
        printf("Linha muito longa; estrategia ignorada.\n");
        return;
    }
    trim_line(line);
    if (!parse_long_text(line, &strategy_value) || strategy_value < 1 ||
        strategy_value > 3) {
        printf("Estrategia invalida.\n");
        return;
    }

    if (!simulation_init(&simulation, memory_size,
                         (AllocationStrategy)(strategy_value - 1))) {
        fprintf(stderr, "Falha ao iniciar a simulacao.\n");
        return;
    }

    printf("Estrategia: %s. Digite HELP para ver os comandos.\n",
           allocation_strategy_name((AllocationStrategy)(strategy_value - 1)));

    for (;;) {
        char *cursor;
        char *command;
        char *arguments;
        char *scan;

        printf("> ");
        fflush(stdout);
        if (!read_input_line(line, sizeof(line), &truncated)) {
            printf("\n");
            break;
        }
        if (truncated) {
            printf("Linha muito longa; comando ignorado.\n");
            continue;
        }
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        cursor = line;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor)) {
            cursor++;
        }
        if (*cursor != '\0') {
            *cursor = '\0';
            cursor++;
        }
        command = line;
        while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
            cursor++;
        }
        arguments = cursor;

        for (scan = command; *scan != '\0'; scan++) {
            *scan = (char)toupper((unsigned char)*scan);
        }

        if (strcmp(command, "ALLOCATE") == 0) {
            Operation operation;
            int process_id;
            int size;
            size_t before = simulation.history_count;

            if (!parse_pair(arguments, &process_id, &size) ||
                process_id <= 0 || size <= 0) {
                printf("Uso: ALLOCATE <pid> <tamanho> (valores positivos)\n");
                continue;
            }
            operation.type = OPERATION_ALLOCATE;
            operation.process_id = process_id;
            operation.size = size;
            (void)simulation_execute(&simulation, &operation);
            print_last_operation(&simulation, before);
            visualization_print_memory_map(&simulation.memory);
        } else if (strcmp(command, "FREE") == 0) {
            Operation operation;
            int process_id;
            size_t before = simulation.history_count;

            if (!parse_single(arguments, &process_id) || process_id <= 0) {
                printf("Uso: FREE <pid> (processo positivo)\n");
                continue;
            }
            operation.type = OPERATION_FREE;
            operation.process_id = process_id;
            operation.size = 0;
            (void)simulation_execute(&simulation, &operation);
            print_last_operation(&simulation, before);
            visualization_print_memory_map(&simulation.memory);
        } else if (strcmp(command, "MAP") == 0) {
            visualization_print_memory_map(&simulation.memory);
        } else if (strcmp(command, "HISTORY") == 0) {
            visualization_print_history(&simulation);
        } else if (strcmp(command, "METRICS") == 0) {
            Metrics metrics = simulation_metrics(&simulation);

            visualization_print_metrics(&metrics, "Estado atual");
        } else if (strcmp(command, "HELP") == 0) {
            custom_help();
        } else if (strcmp(command, "QUIT") == 0) {
            break;
        } else {
            printf("Comando invalido: %s (digite HELP)\n", command);
        }
    }

    {
        Metrics metrics = simulation_metrics(&simulation);

        visualization_print_metrics(&metrics, "Metricas finais");
    }
    simulation_destroy(&simulation);
}

/* ------------------------------------------------------------------------- */
/* Modo interativo: menu principal                                            */
/* ------------------------------------------------------------------------- */

static void run_interactive(const StressConfig *config)
{
    ScenarioSet set;
    int running = 1;

    if (!build_scenarios(&set, config)) {
        fprintf(stderr, "Falha ao montar os cenarios.\n");
        return;
    }

    visualization_print_banner();
    visualization_print_menu();

    while (running) {
        char line[LINE_BUFFER];
        long option;
        int truncated;

        if (!read_input_line(line, sizeof(line), &truncated)) {
            printf("\n");
            break;
        }
        if (truncated) {
            printf("Entrada muito longa; opcao ignorada.\n");
            visualization_print_menu();
            continue;
        }
        trim_line(line);

        if (!parse_long_text(line, &option)) {
            printf("Opcao invalida. Digite um numero entre 0 e 6.\n");
            visualization_print_menu();
            continue;
        }

        switch (option) {
        case 0:
            printf("Encerrando o simulador.\n");
            running = 0;
            break;
        case 1:
        case 2:
        case 3: {
            AllocationStrategy strategy =
                (AllocationStrategy)(option - 1);
            const Scenario *scenario;
            int index;

            for (index = 0; index < 3; index++) {
                scenario = scenarios_get(&set, (size_t)index);
                if (scenario != NULL) {
                    (void)execute_scenario_detailed(scenario, strategy);
                }
            }
            scenario = scenarios_get(&set, 3u);
            if (scenario != NULL) {
                (void)execute_scenario_summary(scenario, strategy);
            }
            visualization_print_menu();
            break;
        }
        case 4: {
            int index;

            for (index = 0; index < 4; index++) {
                const Scenario *scenario =
                    scenarios_get(&set, (size_t)index);
                if (scenario != NULL) {
                    compare_scenario(scenario, index == 2);
                }
            }
            visualization_print_menu();
            break;
        }
        case 5:
            run_custom_scenario(config->custom_memory_size);
            visualization_print_menu();
            break;
        case 6:
            visualization_print_concepts();
            visualization_print_menu();
            break;
        default:
            printf("Opcao invalida. Digite um numero entre 0 e 6.\n");
            visualization_print_menu();
            break;
        }
    }

    scenarios_destroy(&set);
}

/* ------------------------------------------------------------------------- */
/* Ponto de entrada                                                           */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    StressConfig config;
    int scenario_choice = 0; /* 0 = todos */
    int strategy_choice = 4; /* padrao: comparar todos */
    int non_interactive = 0;
    int i;

    stress_config_defaults(&config);

    for (i = 1; i < argc; i++) {
        const char *argument = argv[i];
        long value;

        if (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0) {
            print_help(argv[0]);
            return 0;
        }

        if (starts_with(argument, "--scenario=")) {
            if (!parse_long_text(argument + strlen("--scenario="), &value) ||
                value < 1 || value > 4) {
                fprintf(stderr, "Valor invalido para --scenario (use 1..4).\n");
                return 1;
            }
            scenario_choice = (int)value;
            non_interactive = 1;
            continue;
        }

        if (starts_with(argument, "--strategy=")) {
            if (!parse_long_text(argument + strlen("--strategy="), &value) ||
                value < 1 || value > 4) {
                fprintf(stderr, "Valor invalido para --strategy (use 1..4).\n");
                return 1;
            }
            strategy_choice = (int)value;
            non_interactive = 1;
            continue;
        }

        if (starts_with(argument, "--stress-mem=")) {
            if (!parse_long_text(argument + strlen("--stress-mem="), &value) ||
                value < 1 || value > 1000000) {
                fprintf(stderr,
                        "Valor invalido para --stress-mem (use 1..1000000).\n");
                return 1;
            }
            config.memory_size = (int)value;
            continue;
        }

        if (starts_with(argument, "--stress-procs=")) {
            if (!parse_long_text(argument + strlen("--stress-procs="),
                                 &value) ||
                value < 1 || value > 100000) {
                fprintf(stderr,
                        "Valor invalido para --stress-procs (use 1..100000).\n");
                return 1;
            }
            config.process_count = (int)value;
            continue;
        }

        if (starts_with(argument, "--stress-min=")) {
            if (!parse_long_text(argument + strlen("--stress-min="), &value) ||
                value < 1 || value > 1000000) {
                fprintf(stderr,
                        "Valor invalido para --stress-min (use 1..1000000).\n");
                return 1;
            }
            config.min_size = (int)value;
            continue;
        }

        if (starts_with(argument, "--stress-max=")) {
            if (!parse_long_text(argument + strlen("--stress-max="), &value) ||
                value < 1 || value > 1000000) {
                fprintf(stderr,
                        "Valor invalido para --stress-max (use 1..1000000).\n");
                return 1;
            }
            config.max_size = (int)value;
            continue;
        }

        if (starts_with(argument, "--stress-ops=")) {
            if (!parse_long_text(argument + strlen("--stress-ops="), &value) ||
                value < 1 || value > 1000000) {
                fprintf(stderr,
                        "Valor invalido para --stress-ops (use 1..1000000).\n");
                return 1;
            }
            config.operation_count = (int)value;
            continue;
        }

        if (starts_with(argument, "--stress-seed=")) {
            if (!parse_long_text(argument + strlen("--stress-seed="), &value) ||
                value < 0 || (unsigned long)value > (unsigned long)UINT_MAX) {
                fprintf(stderr,
                        "Valor invalido para --stress-seed (use 0..%u).\n",
                        UINT_MAX);
                return 1;
            }
            config.seed = (unsigned int)value;
            continue;
        }

        if (starts_with(argument, "--custom-mem=")) {
            if (!parse_long_text(argument + strlen("--custom-mem="), &value) ||
                value < 1 || value > 1000000) {
                fprintf(stderr,
                        "Valor invalido para --custom-mem (use 1..1000000).\n");
                return 1;
            }
            config.custom_memory_size = (int)value;
            continue;
        }

        fprintf(stderr, "Argumento desconhecido: %s\n", argument);
        fprintf(stderr, "Use --help para ver as opcoes.\n");
        return 1;
    }

    if (config.max_size < config.min_size) {
        fprintf(stderr,
                "Valor invalido: --stress-max deve ser maior ou igual a "
                "--stress-min.\n");
        return 1;
    }
    if (config.min_size > config.memory_size) {
        fprintf(stderr,
                "Valor invalido: --stress-min nao pode exceder --stress-mem.\n");
        return 1;
    }

    if (non_interactive) {
        return run_non_interactive(scenario_choice, strategy_choice, &config);
    }

    run_interactive(&config);
    return 0;
}
