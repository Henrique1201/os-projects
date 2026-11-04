/*
 * visualization.c - Camada de apresentacao (terminal) do simulador.
 *
 * Este modulo contem APENAS a parte visual. Nao realiza calculos de
 * gerenciamento de memoria, nem decide estrategias, nem inventa valores.
 * Todos os numeros exibidos vem das estruturas passadas por parametro.
 */

#include "visualization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Largura interna do mapa de memoria (dentro das bordas). */
#define MAP_INNER_WIDTH 68

/* Largura minima desejada para um bloco no mapa. */
#define MAP_MIN_BLOCK_WIDTH 4

/*
 * Quando a lista de blocos for maior que esse limite, o mapa e resumido:
 * exibe MAP_HEAD_BLOCKS blocos reais e um bloco agregado com o restante.
 */
#define MAP_MAX_DISPLAY_BLOCKS 16
#define MAP_HEAD_BLOCKS 8

/* Largura da tabela comparativa (metrica + colunas de estrategia). */
#define COMP_METRIC_WIDTH 34
#define COMP_STRATEGY_WIDTH 13

/* Larguras da tabela de comparacao de enderecos. */
#define ADDR_STEP_WIDTH 4
#define ADDR_OP_WIDTH 24
#define ADDR_STRATEGY_WIDTH 13

/* ------------------------------------------------------------------ */
/* Auxiliares                                                         */
/* ------------------------------------------------------------------ */

static void print_horizontal_line(int width)
{
    int i;
    putchar('+');
    for (i = 0; i < width; i++)
        putchar('-');
    putchar('+');
    putchar('\n');
}

static const char *operation_type_name(OperationType type)
{
    switch (type) {
        case OPERATION_ALLOCATE:
            return "ALLOCATE";
        case OPERATION_FREE:
            return "FREE";
        default:
            return "DESCONHECIDO";
    }
}

static const char *operation_status_name(OperationStatus status)
{
    switch (status) {
        case OPERATION_STATUS_SUCCESS:
            return "SUCESSO";
        case OPERATION_STATUS_FAILURE:
            return "FALHA";
        case OPERATION_STATUS_INVALID:
            return "INVALIDO";
        default:
            return "DESCONHECIDO";
    }
}

static void block_label(const MemoryBlock *block, char *out, size_t out_size)
{
    if (block->state == BLOCK_FREE || block->process_id == NO_PROCESS)
        snprintf(out, out_size, "LIVRE");
    else
        snprintf(out, out_size, "P%d", block->process_id);
}

static int count_digits(int value)
{
    int n = 0;
    if (value == 0)
        return 1;
    if (value < 0)
        value = -value;
    while (value > 0) {
        value /= 10;
        n++;
    }
    return n;
}

static void print_padded_label(const char *label, int width)
{
    int len = (int)strlen(label);
    int i;

    if (len <= width) {
        printf("%s", label);
        for (i = 0; i < width - len; i++)
            putchar(' ');
    } else {
        for (i = 0; i < width; i++)
            putchar(label[i]);
    }
}

/* ------------------------------------------------------------------ */
/* Funcoes publicas                                                   */
/* ------------------------------------------------------------------ */

void visualization_print_banner(void)
{
    puts("================================================================");
    puts(" SIMULADOR DE GERENCIAMENTO DE MEMORIA CONTINUA");
    puts(" Alocacao com particoes variaveis: First Fit, Best Fit, Worst Fit");
    puts("================================================================");
}

void visualization_print_menu(void)
{
    puts("Escolha uma opcao:");
    puts("  1 Executar First Fit");
    puts("  2 Executar Best Fit");
    puts("  3 Executar Worst Fit");
    puts("  4 Comparar todos os algoritmos");
    puts("  5 Executar cenario personalizado");
    puts("  6 Visualizar conceitos teoricos");
    puts("  0 Sair");
    printf("Opcao: ");
    fflush(stdout);
}

void visualization_print_concepts(void)
{
    puts("Conceitos teoricos");
    puts("");
    puts("Alocacao continua com particoes variaveis:");
    puts("  A memoria e um unico intervalo. Cada processo recebe um bloco");
    puts("  do tamanho exato do pedido, sem tamanhos fixos pre-definidos.");
    puts("");
    puts("Criterios de escolha do bloco livre:");
    puts("  First Fit: percorre do inicio e para no primeiro bloco livre");
    puts("             com tamanho suficiente.");
    puts("  Best Fit:  percorre todos e escolhe o MENOR bloco livre com");
    puts("             tamanho suficiente.");
    puts("  Worst Fit: percorre todos e escolhe o MAIOR bloco livre com");
    puts("             tamanho suficiente.");
    puts("");
    puts("Fragmentacao EXTERNA:");
    puts("  Ocorre quando a memoria livre total e suficiente, mas nenhum");
    puts("  pedaco contiguo e grande o bastante para o proximo pedido.");
    puts("");
    puts("Fragmentacao INTERNA:");
    puts("  E o desperdicio DENTRO de uma particao alocada maior que o");
    puts("  processo. Neste modelo didatico a fragmentacao interna e zero,");
    puts("  porque o bloco alocado tem exatamente o tamanho solicitado e");
    puts("  nao existe cabecalho de bloco nem alinhamento. Em particoes");
    puts("  variaveis reais ela se reduz ao overhead de cabecalho/alinhamento.");
    puts("");
    puts("Blocos examinados:");
    puts("  E o numero de blocos percorridos na BUSCA DE SELECAO do bloco");
    puts("  livre. First Fit pode encerrar antes; Best Fit e Worst Fit");
    puts("  percorrem a lista inteira. Nao inclui percursos auxiliares de");
    puts("  manutencao, que sao identicos nas tres estrategias.");
    puts("");
    puts("Fragmentacao maxima:");
    puts("  Maior valor de fragmentacao observado em algum passo da execucao.");
    puts("  Util para enxergar o instante em que uma solicitacao falha,");
    puts("  situacao que o snapshot final pode nao mostrar.");
    puts("");
    puts("Coalescencia:");
    puts("  Ao liberar um processo, blocos livres vizinhos sao unidos,");
    puts("  criando regioes maiores e reduzindo a fragmentacao externa.");
    puts("");
    puts("Comparacao justa:");
    puts("  Todas as estrategias recebem a mesma sequencia de operacoes,");
    puts("  de modo que as diferencas de desempenho decorrem apenas do");
    puts("  criterio de escolha de bloco.");
    puts("");
    puts("Aviso: este e um modelo didatico, nao um kernel real.");
}

void visualization_print_memory_map(const Memory *memory)
{
    int total;
    size_t actual_n;
    size_t display_n;
    size_t i;
    size_t summary_index;
    const MemoryBlock *b;
    const MemoryBlock **blocks = NULL;
    int *widths = NULL;
    int *cumulative = NULL;
    int total_digits;
    int content_width;
    int separators;
    int available;
    int sum;
    int reduced;
    int simplified;
    int cur;
    int target;
    int hidden_size;
    char label[32];
    char summary_label[64];
    MemoryBlock summary_block;

    if (memory == NULL) {
        puts("Mapa de memoria invalido.");
        return;
    }

    total = memory->total_size;
    if (total <= 0 || memory->head == NULL) {
        puts("Memoria vazia.");
        return;
    }

    actual_n = 0;
    b = memory->head;
    while (b != NULL) {
        actual_n++;
        b = b->next;
    }
    if (actual_n == 0) {
        puts("Memoria vazia.");
        return;
    }

    blocks = (const MemoryBlock **)malloc(actual_n * sizeof(*blocks));
    widths = (int *)malloc(actual_n * sizeof(*widths));
    cumulative = (int *)malloc((actual_n + 1) * sizeof(*cumulative));
    if (blocks == NULL || widths == NULL || cumulative == NULL) {
        puts("Erro ao preparar o mapa de memoria.");
        free(blocks);
        free(widths);
        free(cumulative);
        return;
    }

    b = memory->head;
    for (i = 0; i < actual_n && b != NULL; i++) {
        blocks[i] = b;
        b = b->next;
    }
    actual_n = i;

    total_digits = count_digits(total);
    content_width = MAP_INNER_WIDTH - total_digits;
    if (content_width < 8)
        content_width = 8;

    simplified = 0;
    summary_index = 0;
    hidden_size = 0;
    summary_label[0] = '\0';

    if (actual_n > MAP_MAX_DISPLAY_BLOCKS) {
        int head_sum = 0;

        simplified = 1;
        display_n = MAP_HEAD_BLOCKS + 1;
        summary_index = display_n - 1;

        for (i = 0; i < MAP_HEAD_BLOCKS; i++)
            head_sum += blocks[i]->size;
        hidden_size = total - head_sum;

        snprintf(summary_label, sizeof(summary_label), "+%zu",
                 actual_n - MAP_HEAD_BLOCKS);

        summary_block.process_id = NO_PROCESS;
        summary_block.start_address = blocks[MAP_HEAD_BLOCKS]->start_address;
        summary_block.size = hidden_size;
        summary_block.state = BLOCK_FREE;
        summary_block.next = NULL;
        blocks[summary_index] = &summary_block;
    } else {
        display_n = actual_n;
    }

    /*
     Larguras proporcionais com protecao para blocos pequenos.
     Se couber, deixa um espaco entre os blocos para separar rotulos.
    */
    separators = (display_n > 1) ? 1 : 0;
    available = content_width;
    if (separators)
        available = content_width - ((int)display_n - 1);
    if (available < (int)display_n) {
        separators = 0;
        available = content_width;
    }

    sum = 0;
    reduced = 0;
    for (i = 0; i < display_n; i++) {
        int raw = (int)((long long)blocks[i]->size * available / total);
        if (raw < 1)
            raw = 1;
        widths[i] = raw;
        sum += raw;
    }

    for (i = 0; i < display_n; i++) {
        if (widths[i] < MAP_MIN_BLOCK_WIDTH) {
            widths[i] = MAP_MIN_BLOCK_WIDTH;
            reduced = 1;
        }
    }

    if (reduced) {
        sum = 0;
        for (i = 0; i < display_n; i++)
            sum += widths[i];
        if (sum > available) {
            double scale = (double)available / (double)sum;
            sum = 0;
            for (i = 0; i < display_n; i++) {
                int w = (int)(widths[i] * scale);
                if (w < 1)
                    w = 1;
                widths[i] = w;
                sum += w;
            }
        } else {
            reduced = 0;
        }
    }

    while (sum < available) {
        int largest = 0;
        for (i = 1; i < display_n; i++) {
            if (widths[i] > widths[largest])
                largest = i;
        }
        widths[largest]++;
        sum++;
    }

    while (sum > available) {
        int largest = 0;
        int stopped = 0;
        for (i = 0; i < display_n; i++) {
            if (widths[i] > widths[largest])
                largest = i;
        }
        if (widths[largest] > 1) {
            widths[largest]--;
            sum--;
        } else {
            stopped = 1;
        }
        if (stopped)
            break;
    }

    cumulative[0] = 0;
    for (i = 1; i <= display_n; i++) {
        cumulative[i] = cumulative[i - 1] + widths[i - 1];
        if (separators && i < display_n)
            cumulative[i]++;
    }

    /* Moldura superior. */
    print_horizontal_line(content_width);

    /* Linha dos blocos. */
    putchar('|');
    for (i = 0; i < display_n; i++) {
        if (simplified && i == summary_index) {
            print_padded_label(summary_label, widths[i]);
        } else {
            block_label(blocks[i], label, sizeof(label));
            print_padded_label(label, widths[i]);
        }
        if (separators && i + 1 < display_n)
            putchar(' ');
    }
    puts("|");

    /* Moldura inferior. */
    print_horizontal_line(content_width);

    /* Linha de enderecos. */
    putchar('|');
    cur = 0;
    for (i = 0; i < display_n; i++) {
        target = cumulative[i];
        if (target < cur)
            target = cur + 1;
        while (cur < target) {
            putchar(' ');
            cur++;
        }
        printf("%d", blocks[i]->start_address);
        cur += count_digits(blocks[i]->start_address);
    }
    target = content_width - total_digits;
    if (target < cur)
        target = cur;
    while (cur < target) {
        putchar(' ');
        cur++;
    }
    printf("%d", total);
    puts("|");

    /* Tamanhos dos blocos, com quebra de linha se necessario. */
    printf("Tamanhos (KB):");
    cur = 14;
    for (i = 0; i < display_n; i++) {
        char piece[64];
        int piece_len;

        if (simplified && i == summary_index) {
            piece_len = snprintf(piece, sizeof(piece), " | %s=%d",
                                 summary_label, hidden_size);
        } else {
            block_label(blocks[i], label, sizeof(label));
            if (i == 0)
                piece_len = snprintf(piece, sizeof(piece), " %s=%d",
                                     label, blocks[i]->size);
            else
                piece_len = snprintf(piece, sizeof(piece), " | %s=%d",
                                     label, blocks[i]->size);
        }

        if (i > 0 && cur + piece_len > 74) {
            printf("\n              ");
            cur = 14;
        }
        printf("%s", piece);
        cur += piece_len;
    }
    printf("\n");

    if (simplified) {
        printf("Mapa simplificado: %zu blocos agregados; "
               "veja o historico.\n", actual_n);
    } else if (reduced) {
        puts("Escala: aproximada (+) devido a blocos muito pequenos.");
    }

    free(blocks);
    free(widths);
    free(cumulative);
}

void visualization_print_operation(const OperationRecord *record)
{
    if (record == NULL) {
        puts("Operacao invalida.");
        return;
    }

    printf("Operacao: %-8s | Processo: P%-3d | Tamanho: ",
           operation_type_name(record->type), record->process_id);

    if (record->type == OPERATION_ALLOCATE)
        printf("%d KB\n", record->size);
    else
        printf("-\n");

    printf("Situacao: %-8s | Endereco: %d | Blocos examinados: %d\n",
           operation_status_name(record->status), record->start_address,
           record->blocks_examined);

    if (record->message[0] != '\0')
        printf("Mensagem: %s\n", record->message);
}

void visualization_print_metrics(const Metrics *metrics, const char *title)
{
    if (metrics == NULL) {
        puts("Metricas invalidas.");
        return;
    }

    printf("--- %s ---\n", title != NULL ? title : "Metricas");
    printf("%-22s : %ld\n", "Total de solicitacoes", metrics->total_requests);
    printf("%-22s : %ld\n", "Alocacoes realizadas",
           metrics->successful_allocations);
    printf("%-22s : %ld\n", "Alocacoes com falha",
           metrics->failed_allocations);
    printf("%-22s : %d KB\n", "Memoria ocupada", metrics->used_memory);
    printf("%-22s : %d KB\n", "Memoria livre", metrics->free_memory);
    printf("%-22s : %d KB\n", "Maior bloco livre", metrics->largest_free_block);
    printf("%-22s : %d\n", "Blocos livres", metrics->free_block_count);

    if (metrics->fragmentation_defined)
        printf("%-22s : %.2f%%\n", "Fragmentacao externa",
               metrics->fragmentation * 100.0);
    else
        printf("%-22s : indefinida\n", "Fragmentacao externa");

    if (metrics->peak_fragmentation_defined)
        printf("%-22s : %.2f%%\n", "Fragmentacao maxima",
               metrics->peak_fragmentation * 100.0);
    else
        printf("%-22s : indefinida\n", "Fragmentacao maxima");

    printf("%-22s : %ld\n", "Blocos examinados",
           metrics->total_blocks_examined);
    printf("%-22s : %.2f%%\n", "Utilizacao", metrics->utilization_percent);
    printf("%-22s : %.2f%%\n", "Taxa de sucesso",
           metrics->success_rate_percent);
    printf("%-22s : %.2f%%\n", "Taxa de falha",
           metrics->failure_rate_percent);
}

void visualization_print_comparison(const char *const *strategy_names,
                                    const Metrics *const *metrics,
                                    size_t count)
{
    size_t i;

    if (strategy_names == NULL || metrics == NULL || count == 0) {
        puts("Sem dados para comparacao.");
        return;
    }

    printf("%-*s", COMP_METRIC_WIDTH, "Metrica");
    for (i = 0; i < count; i++)
        printf(" %*s", COMP_STRATEGY_WIDTH,
               strategy_names[i] != NULL ? strategy_names[i] : "-");
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "----------------------------------");
    for (i = 0; i < count; i++)
        printf(" %*s", COMP_STRATEGY_WIDTH, "-------------");
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Total de solicitacoes");
    for (i = 0; i < count; i++)
        printf(" %*ld", COMP_STRATEGY_WIDTH, metrics[i]->total_requests);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Alocacoes realizadas");
    for (i = 0; i < count; i++)
        printf(" %*ld", COMP_STRATEGY_WIDTH,
               metrics[i]->successful_allocations);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Alocacoes com falha");
    for (i = 0; i < count; i++)
        printf(" %*ld", COMP_STRATEGY_WIDTH, metrics[i]->failed_allocations);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Taxa de sucesso (%)");
    for (i = 0; i < count; i++)
        printf(" %*.2f%%", COMP_STRATEGY_WIDTH - 1,
               metrics[i]->success_rate_percent);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Taxa de falha (%)");
    for (i = 0; i < count; i++)
        printf(" %*.2f%%", COMP_STRATEGY_WIDTH - 1,
               metrics[i]->failure_rate_percent);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Memoria ocupada (KB)");
    for (i = 0; i < count; i++)
        printf(" %*d", COMP_STRATEGY_WIDTH, metrics[i]->used_memory);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Memoria livre (KB)");
    for (i = 0; i < count; i++)
        printf(" %*d", COMP_STRATEGY_WIDTH, metrics[i]->free_memory);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Maior bloco livre (KB)");
    for (i = 0; i < count; i++)
        printf(" %*d", COMP_STRATEGY_WIDTH, metrics[i]->largest_free_block);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Blocos livres");
    for (i = 0; i < count; i++)
        printf(" %*d", COMP_STRATEGY_WIDTH, metrics[i]->free_block_count);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Fragmentacao externa (%)");
    for (i = 0; i < count; i++) {
        if (metrics[i]->fragmentation_defined)
            printf(" %*.2f%%", COMP_STRATEGY_WIDTH - 1,
                   metrics[i]->fragmentation * 100.0);
        else
            printf(" %*s", COMP_STRATEGY_WIDTH, "indefinida");
    }
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Fragmentacao maxima (%)");
    for (i = 0; i < count; i++) {
        if (metrics[i]->peak_fragmentation_defined)
            printf(" %*.2f%%", COMP_STRATEGY_WIDTH - 1,
                   metrics[i]->peak_fragmentation * 100.0);
        else
            printf(" %*s", COMP_STRATEGY_WIDTH, "indefinida");
    }
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Blocos examinados");
    for (i = 0; i < count; i++)
        printf(" %*ld", COMP_STRATEGY_WIDTH,
               metrics[i]->total_blocks_examined);
    printf("\n");

    printf("%-*s", COMP_METRIC_WIDTH, "Utilizacao (%)");
    for (i = 0; i < count; i++)
        printf(" %*.2f%%", COMP_STRATEGY_WIDTH - 1,
               metrics[i]->utilization_percent);
    printf("\n");
}

void visualization_print_address_comparison(const char *const *strategy_names,
                                            const AddressComparisonRow *rows,
                                            size_t row_count)
{
    size_t i;
    int j;
    const char *default_names[VISUALIZATION_STRATEGY_COLUMNS] = {
        "First Fit", "Best Fit", "Worst Fit"
    };
    const char *names[VISUALIZATION_STRATEGY_COLUMNS];

    if (rows == NULL || row_count == 0) {
        puts("Nenhuma operacao para comparar enderecos.");
        return;
    }

    for (j = 0; j < VISUALIZATION_STRATEGY_COLUMNS; j++) {
        if (strategy_names != NULL && strategy_names[j] != NULL)
            names[j] = strategy_names[j];
        else
            names[j] = default_names[j];
    }

    puts("ENDERECO ESCOLHIDO POR CADA ESTRATEGIA");
    puts("(comparacao justa, passo a passo)");

    printf("%-*s %-*s", ADDR_STEP_WIDTH, "#", ADDR_OP_WIDTH, "Operacao");
    for (j = 0; j < VISUALIZATION_STRATEGY_COLUMNS; j++)
        printf(" %*s", ADDR_STRATEGY_WIDTH, names[j]);
    printf("\n");

    for (i = 0; i < row_count; i++) {
        printf("%*d %-*.*s", ADDR_STEP_WIDTH, rows[i].step,
               ADDR_OP_WIDTH, ADDR_OP_WIDTH, rows[i].operation);
        for (j = 0; j < VISUALIZATION_STRATEGY_COLUMNS; j++) {
            if (rows[i].success[j])
                printf(" %*d", ADDR_STRATEGY_WIDTH, rows[i].address[j]);
            else
                printf(" %*s", ADDR_STRATEGY_WIDTH, "falhou");
        }
        printf("\n");
    }

    puts("Nota: a mesma operacao foi aplicada as tres estrategias,");
    puts("cada uma com sua propria memoria independente.");
}

void visualization_print_history(const Simulation *simulation)
{
    size_t i;

    if (simulation == NULL) {
        puts("Simulacao invalida.");
        return;
    }

    printf(" #   %-10s %-6s %-8s %-10s %-10s %-10s\n",
           "Tipo", "Proc", "Tamanho", "Situacao", "Endereco", "Blocos");

    for (i = 0; i < simulation->history_count; i++) {
        const OperationRecord *r = &simulation->history[i];

        printf(" %-3zu %-10s P%-5d ", i + 1,
               operation_type_name(r->type), r->process_id);

        if (r->type == OPERATION_ALLOCATE)
            printf("%-8d ", r->size);
        else
            printf("%-8s ", "-");

        printf("%-10s %-10d %-10d\n",
               operation_status_name(r->status),
               r->start_address, r->blocks_examined);
    }

    if (simulation->history_count == 0)
        puts("Nenhuma operacao registrada.");
}

void visualization_print_scenario_header(const char *name,
                                         const char *description,
                                         const char *objective,
                                         int memory_size)
{
    printf("Cenario: %s\n", name != NULL ? name : "-");
    printf("Descricao: %s\n",
           description != NULL ? description : "-");
    printf("Objetivo: %s\n", objective != NULL ? objective : "-");
    printf("Memoria total: %d KB\n", memory_size);
    print_horizontal_line(60);
}
