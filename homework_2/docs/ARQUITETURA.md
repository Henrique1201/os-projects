# Arquitetura do Simulador

O projeto é organizado em camadas com dependências em uma única direção. O
modelo de memória não conhece estratégias; as estratégias não conhecem a
interface; a interface não calcula regras de gerenciamento. Essa separação
permite testar cada conceito isoladamente e deixa explícito o que é modelo, o
que é algoritmo e o que é apresentação.

## 1. Visão geral em camadas

```text
                       +-----------------------------+
                       |        src/main.c           |
                       |  CLI, menu, cenarios        |
                       +--------------+--------------+
                                      |
             +------------------------+-------------------------+
             |                        |                         |
             v                        v                         v
   +-------------------+   +---------------------+   +---------------------+
   | src/scenarios.c   |   | src/visualization.c |   |     tests/          |
   | cenarios e PRNG   |   | apresentacao        |   | verificacoes        |
   +---------+---------+   +----------+----------+   +---------------------+
             |                        ^
             v                        |
   +-------------------+              |
   | src/simulation.c  |--------------+
   | orquestracao      |
   +----+---------+----+
        |         |
        v         v
+--------------+  +------------------+
| src/memory.c |<--| src/allocation.c |
| modelo       |  | estrategias      |
+--------------+  +------------------+
        ^
        |
+----------------+
| src/metrics.c  |
| indicadores    |
+----------------+
```

Dependências de inclusão:

- `memory` não inclui nenhum outro módulo do projeto.
- `allocation` inclui `memory`.
- `metrics` inclui `memory`.
- `simulation` inclui `allocation`, `memory` e `metrics`.
- `scenarios` inclui `simulation`.
- `visualization` inclui `memory`, `metrics` e `simulation`.
- `main` inclui todos os módulos públicos.
- `tests` incluem os módulos que exercitam, com `test_utils.h` para o micro
  framework.

## 2. Módulos

### 2.1 `src/memory.h` / `src/memory.c` — modelo da região

**Responsabilidade.** Representar a memória física fictícia como uma lista
simplesmente encadeada de blocos e oferecer as primitivas de reserva, divisão,
liberação, coalescência, consulta e validação. Não decide qual bloco escolher.

**Estrutura de dados.** `MemoryBlock` guarda `process_id`, `start_address`,
`size`, `state` e `next`. `Memory` guarda `total_size`, `head` e três
contadores (`block_count`, `free_block_count`, `occupied_block_count`).

```text
Memory
  total_size = 1024
  head ------+
             |
             v
        +----------------+     +----------------+     +----------------+
        | P1             |     | LIVRE          |     | P3             |
        | start=0        |     | start=300      |     | start=400      |
        | size=300       |     | size=100       |     | size=200       |
        | OCCUPIED       |     | FREE           |     | OCCUPIED       |
        | next ----------+---->| next ----------+---->| next = NULL    |
        +----------------+     +----------------+     +----------------+

        [0, 300) ocupado      [300, 400) livre      [400, 600) ocupado
```

A lista cobre toda a região: o endereço de cada bloco é a soma dos tamanhos
dos blocos anteriores. Um `LIVRE` sempre tem `process_id == NO_PROCESS`.

**Funções públicas e papel no gerenciamento:**

| Função | Papel | Relação com o conceito teórico |
|--------|-------|-------------------------------|
| `memory_init` | Cria a região com um único bloco livre de `total_size` KB em `0`. | Estado inicial da memória física; base para qualquer política de alocação. |
| `memory_destroy` | Libera todos os nós e zera a estrutura. | Encerramento do modelo; não é uma operação de SO. |
| `memory_reset` | Reconstrói um único bloco livre preservando o tamanho. | Reinicialização entre experimentos; garante comparação justa. |
| `memory_allocate` | Atalho de First Fit usando `memory_allocate_in_block`. | Alocação contígua simples; usada em testes e no gerenciamento básico. |
| `memory_allocate_in_block` | Divide um bloco livre específico: ocupa os primeiros `size` KB e devolve o restante como livre. | **Divisão da partição variável**; é o coração da alocação contígua. |
| `memory_free` | Marca o bloco do processo como livre e coalesce com vizinhos livres. | **Reaproveitamento de região** e **coalescência imediata**. |
| `memory_find_process` | Localiza o bloco ocupado por um processo. | Um processo não pode ter dois blocos; consulta O(n). |
| `memory_block_is_free` | Predicado único de bloco livre: `state == BLOCK_FREE` e `process_id == NO_PROCESS`. | Evita duplicar a definição de “livre” entre os módulos; usado por `src/allocation.c`. |
| `memory_validate_ex` | Verifica contiguidade, tamanhos, estados, duplicatas, contadores e adjacência de blocos livres, com mensagem. | Verificação das invariantes do modelo. |
| `memory_validate` | Versão sem mensagem de `memory_validate_ex`. | Usada nos testes e em verificações rápidas. |
| `memory_total_free` | Soma os blocos livres em KB. | Memória livre total; não garante contiguidade. |
| `memory_total_used` | Soma os blocos ocupados em KB. | Memória ocupada. |
| `memory_largest_free_block` | Maior bloco livre contíguo em KB. | Numerador do indicador de fragmentação externa. |
| `memory_count_free_blocks` | Quantidade de regiões livres. | Mede a dispersão da memória livre. |
| `memory_count_occupied_blocks` | Quantidade de regiões ocupadas. | Mede a fragmentação do lado ocupado. |
| `memory_block_count` | Total de blocos (livres + ocupados). | Tamanho da lista; custo de varredura. |
| `memory_block_at` | Acesso ao i-ésimo bloco por ordem de endereço. | Apoio a testes e visualização. |
| `memory_status_message` | Traduz códigos de retorno em mensagens didáticas sem acentos. | Apresentação de erros. |

**Funções internas relevantes.** `create_block` aloca um nó; `free_block_list`
libera a lista; `recount_blocks` recalcula os contadores a partir da lista;
`is_free_block` verifica a coerência de um bloco livre; `set_message` copia
mensagens com limite de tamanho. A API pública `memory_block_is_free` expõe o
mesmo predicado para os demais módulos (em especial `src/allocation.c`), de modo
que a definição de bloco livre fica em um único lugar.

**Invariantes garantidas pelo módulo.**

- Endereços contíguos a partir de `0`, sem lacunas nem sobreposição.
- Soma dos tamanhos dos blocos igual a `total_size`.
- Nenhum bloco com tamanho menor ou igual a zero.
- Bloco livre tem `process_id == NO_PROCESS`; bloco ocupado tem processo
  válido.
- Nenhum processo aparece em dois blocos.
- Contadores sempre recalculados após mutação (`recount_blocks`).
- Após `memory_free`, não há dois blocos livres adjacentes. A validação também
  rejeita **qualquer par de blocos livres adjacentes**, mesmo que não tenha
  surgido de uma liberação: como a coalescência é imediata, um par adjacente
  indicaria coalescência quebrada. A mensagem é
  `dois blocos livres adjacentes em %d KB`.

**Quem verifica.** `memory_validate_ex` em `src/memory.c`; os testes usam
`TEST_CHECK_MEMORY_VALID` em todos os binários. A adjacência de blocos livres é
coberta por `tests/test_memory.c` (validação de estado inválido) e por
`tests/test_coalescing.c` (coalescência em todas as posições). A
correspondência detalhada está na seção 4.

### 2.2 `src/allocation.h` / `src/allocation.c` — estratégias

**Responsabilidade.** Escolher **qual** bloco livre será usado. Todas as
estratégias usam o mesmo modelo e a mesma primitiva de divisão; muda apenas o
critério de seleção.

**Funções públicas e papel:**

| Função | Papel | Relação com o conceito teórico |
|--------|-------|-------------------------------|
| `allocation_run` | Despacha para a estratégia indicada; rejeita estratégia inválida. | Ponto único de comparação justa. |
| `allocation_first_fit` | Escolhe o primeiro bloco livre adequado. | First Fit; busca com parada antecipada. |
| `allocation_best_fit` | Escolhe o menor bloco livre adequado. | Best Fit; critério guloso de menor remanescente. |
| `allocation_worst_fit` | Escolhe o maior bloco livre adequado. | Worst Fit; critério guloso de maior remanescente. |
| `allocation_strategy_name` | Devolve `"First Fit"`, `"Best Fit"` ou `"Worst Fit"`. | Rótulo para tabelas e histórico. |
| `allocation_strategy_description` | Descreve o critério em texto didático. | Apoio à apresentação. |

**Função interna central.** `allocation_execute` faz a validação de argumentos,
a rejeição de processo duplicado, o curto-circuito para pedidos maiores que a
memória total, a varredura da lista, a contagem de `blocks_examined`, a escolha
do bloco e a chamada a `memory_allocate_in_block`.

**Invariantes e convenções.**

- Em empate de tamanho, Best Fit e Worst Fit mantêm o primeiro candidato, que é
  o de menor endereço.
- First Fit para no primeiro candidato.
- Best Fit e Worst Fit percorrem a lista inteira.
- `blocks_examined` conta os blocos percorridos **na busca de seleção do bloco
  livre**, inclusive os blocos ocupados atravessados. A convenção explícita está
  em `src/allocation.h`: a contagem **não** inclui os percursos auxiliares de
  manutenção (localização do processo, conferência de pertinência do bloco e
  recálculo dos contadores), que são idênticos nas três estratégias. Por isso é
  um proxy **comparativo** do custo de busca, não o tempo total da operação.
- A definição de “bloco livre” é centralizada em `memory_block_is_free`
  (`src/memory.h`), usada por `src/allocation.c`, em vez de comparar `state`
  diretamente.
- Uma falha por falta de espaço não altera a memória.
- Processo já alocado e pedido maior que a memória total têm
  `blocks_examined = 0` (curto-circuito).
- Retorno `-1` significa argumento inválido e não altera o estado; retorno `0`
  significa tentativa processada, com sucesso ou falha registrada em
  `AllocationResult`.

**Quem verifica.** `tests/test_allocation.c`, especialmente
`test_blocks_examined_convention`, `test_same_layout_three_independent_simulations`,
`test_failure_external_fragmentation_keeps_memory` e
`test_process_already_allocated`; `tests/test_memory.c:test_block_is_free_predicate`
verifica o predicado.

### 2.3 `src/metrics.h` / `src/metrics.c` — indicadores

**Responsabilidade.** Calcular métricas a partir do estado final da memória e
dos contadores acumulados. O módulo é somente leitura: não altera a memória.

**Funções públicas:**

| Função | Papel | Relação com o conceito teórico |
|--------|-------|-------------------------------|
| `metrics_compute` | Preenche a estrutura `Metrics` com taxas, memória, blocos, fragmentação final, fragmentação máxima e utilização. | Síntese quantitativa de uma simulação. |
| `metrics_fragmentation` | Calcula `1 - maior_livre / livre_total`, com proteção para denominador zero. | Indicador operacional de fragmentação externa. |

**Função interna.** `clamp_unit` restringe o resultado ao intervalo `[0, 1]`.

**Invariantes e convenções.**

- `total_requests = successful_allocations + failed_allocations`; portanto,
  apenas pedidos de alocação entram nas taxas.
- Falha de `FREE` não incrementa `failed_allocations`.
- Divisões por zero produzem `0.0`, não valores inventados.
- Se `free_memory == 0`, `fragmentation_defined = 0` e `fragmentation = 0.0`;
  a interface mostra `indefinida`.
- `peak_fragmentation` e `peak_fragmentation_defined` descrevem o **maior**
  valor de fragmentação externa observado após alguma operação. Eles não são
  calculados pelo próprio `metrics.c`: a camada de simulação os observa a cada
  passo e os repassa a `metrics_compute`. Se nenhum passo teve memória livre, o
  pico é indefinido e a interface mostra `indefinida`.
- O módulo não modifica o estado da memória.

**Quem verifica.** `tests/test_metrics.c` (taxas, distribuição, limites do
indicador, caso indefinido, utilização, ausência de divisão por zero e pico de
fragmentação, inclusive um estado alcançável de 50%).

### 2.4 `src/simulation.h` / `src/simulation.c` — orquestração

**Responsabilidade.** Amarrar memória, estratégia e contadores; registrar o
histórico de operações; manter a simulação independente das demais.

**Funções públicas:**

| Função | Papel | Relação com o conceito teórico |
|--------|-------|-------------------------------|
| `simulation_init` | Cria memória livre, define estratégia, zera histórico e contadores. | Início de um experimento controlado. |
| `simulation_destroy` | Libera histórico e memória. | Encerramento. |
| `simulation_reset` | Reinicia memória, histórico e contadores, preservando estratégia e tamanho. | Repetição de experimentos com a mesma configuração. |
| `simulation_execute` | Executa `ALLOCATE` ou `FREE`, registra o resultado e atualiza contadores. | Uma operação do SO simulado. |
| `simulation_execute_sequence` | Executa uma lista de operações na ordem. | Replay da mesma sequência nas três estratégias. |
| `simulation_metrics` | Produz um snapshot de `Metrics`. | Alimenta a tabela comparativa. |

**Funções internas.** `history_reserve` cresce o vetor por dobragem e reserva
**antes** de alterar a memória; `history_append` copia o registro;
`simulation_observe` observa o estado da memória após cada operação e atualiza o
pico de fragmentação externa.

**Indicador de pico.** `simulation_observe` é chamada ao final de
`simulation_execute` sempre que uma tentativa de `ALLOCATE` ou `FREE` foi
processada. Ela calcula `1 - maior_bloco_livre / memória_livre_total` no estado
resultante e guarda o maior valor visto em `peak_fragmentation`; se o passo não
tem memória livre (`free_memory <= 0`), o passo é ignorado e o pico anterior é
preservado. Quando ao menos um passo definido foi observado,
`peak_fragmentation_defined = 1`. É esse valor que torna visível, por exemplo, o
instante de 50.00% do cenário 2, que a fragmentação final de 17.95% não expõe.

**Invariantes e convenções.**

- O histórico é um vetor dinâmico com capacidade inicial 16 e crescimento
  amortizado O(1).
- A reserva de histórico acontece antes da mutação, para que falha de memória
  do próprio vetor não deixe estado e histórico inconsistentes.
- Uma falha de `FREE` não é contabilizada como falha de alocação.
- Cada simulação tem sua própria `Memory`; não há estado global.
- O histórico preserva a ordem das operações e permite reconstruir a execução.
- O pico de fragmentação é observado após toda tentativa processada, não apenas
  após sucessos; por isso uma falha por fragmentação também pode fixar o pico.

**Quem verifica.** `tests/test_simulation.c` (init/reset, sucesso, falha,
free existente e inexistente, crescimento do histórico, métricas coerentes,
sequência completa, independência, cenário 3, pico do cenário 2 e determinismo
do cenário de estresse).

### 2.5 `src/scenarios.h` / `src/scenarios.c` — cenários e PRNG

**Responsabilidade.** Montar as sequências controladas e o cenário de estresse
reproduzível.

**Funções públicas:**

| Função | Papel | Relação com o conceito teórico |
|--------|-------|-------------------------------|
| `scenarios_create_controlled` | Cria os cenários 1, 2 e 3 com sequências fixas. | Isola um fenômeno por cenário. |
| `scenarios_append_stress` | Cria o cenário 4 com sequência pseudoaleatória reprodutível. | Carga mista controlada por semente. |
| `scenarios_get` | Acesso por índice. | Apoio à CLI. |
| `scenarios_destroy` | Libera cenários e sequências. | Encerramento. |

**Funções internas.** `scenario_set_reserve` e `scenario_set_add` gerem o vetor
de cenários; `rng_next` implementa xorshift32; `random_below` faz amostragem
por rejeição para sortear sem viés de módulo; `random_size` sorteia o tamanho
em `[min_size, max_size]`; `pick_unallocated` e `pick_allocated` escolhem
processos ainda não alocados ou atualmente alocados.

**Invariantes e convenções.**

- As três estratégias recebem exatamente a mesma sequência.
- O gerador usa semente explícita e estado local; a semente zero é substituída
  por `0x9E3779B9`.
- **O gerador é cego em relação às estratégias.** O tamanho de cada pedido é
  sorteado livremente em `[min_size, max_size]`; o gerador **não** consulta
  nenhuma memória simulada e **não** usa o estado das estratégias para limitar
  pedidos. A sequência é gerada uma única vez e guardada no cenário; cada
  estratégia a reaplica em uma memória nova. Assim, a mesma sequência pode ser
  atendida por uma estratégia e recusada por outra, o que permite que taxa de
  sucesso, taxa de falha e utilização diferenciem os algoritmos.
- A decisão de liberar ocorre com probabilidade aproximada de 40% quando há
  processos vivos e menos que `process_count` alocados; se não há vivos, força
  alocação; se todos estão alocados, força liberação.
- `FREE` só é emitido para um processo que recebeu `ALLOCATE` antes na
  sequência. O desfecho em tempo de execução, porém, depende da estratégia: em
  uma estratégia em que aquele `ALLOCATE` falhou, o `FREE` correspondente não
  encontra o processo. Isso é esperado e não conta como falha de alocação.

**Por que a versão anterior era circular.** Antes, o gerador mantinha três
memórias de sondagem (uma por estratégia) e limitava cada pedido ao menor dos
maiores blocos livres das três. Isso garantia, **por construção**, que todo
`ALLOCATE` fosse atendido nas três simulações; taxa de sucesso e utilização
ficavam idênticas e incapazes de diferenciar as estratégias. Além disso, a carga
era derivada do comportamento dos algoritmos que deveriam ser testados: o
experimento usava o resultado das estratégias para construir a entrada. A
geração cega elimina essa circularidade.

**Quem verifica.** O cenário 3 é verificado por
`test_simulation.c:test_cenario3_escolhas_distintas`. O pico do cenário 2 é
verificado por `test_simulation.c:test_fragmentacao_pico_cenario2`. O
determinismo do cenário de estresse é verificado por
`test_simulation.c:test_estresse_deterministico` (mesma semente produz a mesma
sequência; semente diferente produz sequência diferente). O gerador também é
exercitado pela CLI e pelos arquivos de exemplo.

### 2.6 `src/visualization.h` / `src/visualization.c` — apresentação

**Responsabilidade.** Exibir banner, menu, conceitos, mapa de memória,
resultado de operação, métricas, tabela comparativa, tabela de endereços e
histórico. Não toma decisões de gerenciamento nem inventa números.

**Funções públicas:**

| Função | Papel |
|--------|-------|
| `visualization_print_banner` | Cabeçalho do programa. |
| `visualization_print_menu` | Menu de opções. |
| `visualization_print_concepts` | Texto didático sobre os conceitos. |
| `visualization_print_memory_map` | Mapa proporcional dos blocos, com endereços e tamanhos reais. |
| `visualization_print_operation` | Resultado de uma operação (situação, endereço, blocos examinados). |
| `visualization_print_metrics` | Bloco de métricas de uma simulação. |
| `visualization_print_comparison` | Tabela lado a lado das três estratégias. |
| `visualization_print_address_comparison` | Tabela “endereço escolhido” passo a passo. |
| `visualization_print_history` | Histórico completo de operações. |
| `visualization_print_scenario_header` | Cabeçalho de cenário. |

**Convenções.** O mapa usa largura proporcional ao tamanho, com largura mínima
para blocos pequenos e aviso de escala aproximada. A moldura, a linha de blocos
e a linha de endereços têm a mesma largura. Os valores exibidos vêm sempre das
estruturas recebidas. O mapa **não** usa vetores automáticos na pilha (VLAs):
os vetores auxiliares são alocados no heap, com checagem de falha. Quando há
blocos demais para uma representação legível, o mapa é agregado e exibe
`Mapa simplificado: N blocos agregados`.

**Conceitos exibidos.** O texto de `visualization_print_concepts` descreve
Worst Fit como “MAIOR bloco livre **com tamanho suficiente**” e qualifica a
fragmentação interna: zero **neste modelo didático**, porque não há cabeçalho de
bloco nem alinhamento; em partições variáveis reais ela se reduz ao overhead e
não é zero. Também explica “Fragmentação máxima” e a convenção de “Blocos
examinados”.

**Invariantes.** O módulo não altera a memória nem os contadores; toda saída
numérica é derivada das estruturas passadas por parâmetro. A formatação pode
arredondar a largura visual dos blocos, mas os valores textuais de endereço,
tamanho e métrica são os valores reais. **Quem verifica.** Não há teste de
snapshot da saída; a garantia é por inspeção do código e pela ausência de
escrita nas estruturas de entrada.

### 2.7 `src/main.c` — CLI e orquestração de apresentação

**Responsabilidade.** Interpretar argumentos, montar cenários, executar
cenários detalhados ou resumidos, comparar estratégias, rodar o cenário
personalizado e o menu interativo.

**Funções internas principais:**

| Função | Papel |
|--------|-------|
| `stress_config_defaults` | Valores padrão do cenário 4. |
| `read_input_line` | Lê uma linha e **drena** o excedente quando ela não cabe no buffer, para que o trecho descartado não vire um comando espúrio. |
| `parse_long_text`, `next_long`, `parse_pair`, `parse_single`, `starts_with`, `trim_line` | Leitura e validação de entrada. |
| `build_scenarios` | Cria cenários controlados e acrescenta o de estresse. |
| `execute_scenario_detailed` | Executa passo a passo e mostra mapa após cada operação. |
| `execute_scenario_summary` | Executa sem despejar cada passo (usado no cenário 4 e no modo interativo). |
| `compare_scenario` | Cria três simulações independentes e mostra a tabela comparativa. |
| `print_address_comparison` | Monta a tabela de endereços do cenário 3. |
| `execute_scenario` | Seleciona detalhado, resumido ou comparativo. |
| `run_non_interactive` | Modo `--scenario` / `--strategy`. |
| `run_custom_scenario` | Cenário personalizado com comandos; o tamanho vem de `--custom-mem` (padrão 1024 KB). |
| `run_interactive` | Menu principal. |
| `main` | Ponto de entrada e validação de argumentos. |

**Regras de seleção.** `--strategy=1..3` executa uma estratégia; `--strategy=4`
compara as três. No modo não interativo, o cenário 4 é sempre resumido; os
cenários 1 a 3 são detalhados quando uma estratégia é escolhida e comparativos
quando `--strategy=4`. O cenário 4 aceita `--stress-mem`, `--stress-procs`,
`--stress-min`, `--stress-max`, `--stress-ops` e `--stress-seed`. O cenário
personalizado (opção 5 do menu) aceita `--custom-mem=KB`, que define o tamanho
total da memória (padrão 1024 KB; faixa válida 1..1000000).

**Invariantes.** Os argumentos são validados antes de qualquer simulação
(`--scenario` em 1..4, `--strategy` em 1..4, parâmetros de estresse dentro dos
limites, `--custom-mem` em 1..1000000). Linhas de entrada longas são drenadas e
geram uma única mensagem, sem comando espúrio. No modo comparativo, cada estratégia tem sua própria `Simulation` e
o histórico é indexado pelo mesmo passo da sequência. **Quem verifica.**
`test_simulation.c:test_comparacao_independente` e
`test_simulation.c:test_cenario3_escolhas_distintas`; a validação de argumentos
é exercitada manualmente pela CLI.

### 2.8 `tests/` — verificação automatizada

**Responsabilidade.** Verificar as invariantes e os comportamentos dos módulos
com um micro framework próprio.

**Arquivos e foco:**

| Arquivo | Foco |
|---------|------|
| `tests/test_utils.h` | Macros `TEST_CHECK`, `TEST_CHECK_EQ_INT`, `TEST_CHECK_EQ_DOUBLE`, `TEST_CHECK_EQ_STR`, `TEST_CHECK_MEMORY_VALID`, `TEST_RUN` e `TEST_SUMMARY`. |
| `tests/test_memory.c` | Inicialização, divisão, liberação, consultas, reset, validação, rejeição de blocos livres adjacentes e predicado `memory_block_is_free` (252 verificações). |
| `tests/test_coalescing.c` | Coalescência com dois, três ou mais blocos, nas extremidades, com vizinho ocupado e validação de adjacência (150 verificações). |
| `tests/test_allocation.c` | Critérios de First/Best/Worst Fit, empates, falhas, argumentos inválidos e convenção de `blocks_examined` (173 verificações). |
| `tests/test_metrics.c` | Taxas, distribuição de memória, fórmula da fragmentação, caso indefinido, divisão por zero, pico de fragmentação e estado alcançável de 50% (86 verificações). |
| `tests/test_simulation.c` | Integração, histórico, contadores, independência, cenário 3, pico do cenário 2, operação com tipo inválido e determinismo do estresse (401 verificações). |

A soma é **1062 verificações, 0 falhas** (173 + 150 + 252 + 86 + 401).

**Invariantes.** Cada binário de teste é formado por um único `.c` que inclui
`test_utils.h`; as variáveis globais do framework têm uma definição por
binário. `TEST_SUMMARY()` devolve 0 apenas se não houve falhas. **Quem
verifica.** O próprio `make test`, que executa todos os binários e falha o alvo
se qualquer um retornar diferente de zero; `make asan` repete com sanitizers.
Saída final observada: cada binário imprime seu total e, ao final,
`TODOS OS TESTES PASSARAM`.

## 3. Fluxo de uma operação até a tabela de comparação

```text
main / run_non_interactive
        |
        |  escolhe Scenario e AllocationStrategy
        v
simulation_execute(&sim, &operation)
        |
        |  history_reserve (antes de mutar)
        v
allocation_run(strategy, &sim.memory, pid, size, &result)
        |
        v
allocation_execute
        |
        +--> memory_find_process(pid)  -> rejeita duplicata (blocks_examined=0)
        +--> size > total_size?        -> falha imediata (blocks_examined=0)
        |
        +--> varre lista em ordem de endereco (usa memory_block_is_free)
        |       First Fit: para no primeiro bloco adequado
        |       Best Fit: menor bloco adequado
        |       Worst Fit: maior bloco adequado
        |
        v
memory_allocate_in_block(&memory, chosen, pid, size)
        |
        +--> cria bloco remanescente, se houver
        +--> marca bloco escolhido como ocupado
        +--> recount_blocks
        |
        v
simulation_execute registra OperationRecord
        |
        +--> total_blocks_examined += result.blocks_examined
        +--> successful_allocations++ ou failed_allocations++
        +--> simulation_observe: atualiza o pico de fragmentacao
        |
        v
simulation_metrics(&sim) -> metrics_compute
        |
        v
visualization_print_comparison / print_address_comparison
```

## 4. Invariantes do modelo e testes que as validam

| # | Invariante | Onde é garantida | Teste(s) que valida(m) |
|---|------------|------------------|------------------------|
| 1 | Endereços contíguos a partir de `0`, sem lacunas nem sobreposição | `memory_validate_ex`; `memory_allocate_in_block`; `memory_free` | `test_memory.c:test_init`, `test_allocate`, `test_integrity_after_operations`; `test_coalescing.c` (todos) |
| 2 | Soma dos tamanhos igual a `total_size` | `memory_validate_ex` | `test_memory.c:test_init`, `test_split_exact_fit`, `test_integrity_after_operations` |
| 3 | Todo bloco tem tamanho `> 0` | `memory_validate_ex`; validações de entrada em `memory_init` e `memory_allocate_in_block` | `test_memory.c:test_init`, `test_direct_split` |
| 4 | Bloco livre tem `NO_PROCESS`; bloco ocupado tem processo válido | `memory_validate_ex`; `memory_block_is_free` | `test_memory.c:test_allocate`, `test_free`, `test_block_is_free_predicate`; `test_coalescing.c:test_no_coalesce_with_occupied` |
| 5 | Um processo não ocupa dois blocos | `memory_find_process` em `memory_allocate_in_block`; `memory_validate_ex` | `test_memory.c:test_allocate_duplicate`; `test_allocation.c:test_process_already_allocated` |
| 6 | Contadores `block_count`, `free_block_count`, `occupied_block_count` coerentes com a lista | `recount_blocks` após cada mutação; `memory_validate_ex` | Todos os testes que usam `TEST_CHECK_MEMORY_VALID`; `test_memory.c:test_queries` |
| 7 | **Nenhum par de blocos livres adjacentes** (não só após liberação) | `memory_free` (coalescência imediata); `memory_validate_ex` rejeita qualquer par adjacente com a mensagem `dois blocos livres adjacentes em %d KB` | `test_coalescing.c:test_coalesce_two_blocks`, `test_coalesce_three_blocks`, `test_coalesce_at_start`, `test_coalesce_at_end`, `test_coalesced_size_and_address`; `test_memory.c:test_validate_rejects_adjacent_free_blocks` |
| 8 | Um processo ocupa no máximo uma região | `memory_find_process`; validação em `memory_allocate_in_block` | `test_memory.c:test_allocate_duplicate`; `test_allocation.c:test_process_already_allocated` |
| 9 | Falha de alocação não altera a memória | `allocation_execute` só chama `memory_allocate_in_block` após escolher; `memory_allocate_in_block` valida antes de mutar | `test_memory.c:test_no_space_contiguous`; `test_allocation.c:test_failure_external_fragmentation_keeps_memory`, `test_request_larger_than_total` |
| 10 | Todas as estratégias processam a mesma quantidade de solicitações | `compare_scenario` executa a mesma sequência em três simulações | `test_simulation.c:test_comparacao_independente`, `test_cenario3_escolhas_distintas` |
| 11 | Histórico e estado da memória permanecem consistentes | `history_reserve` antes de mutar; `history_append` após o resultado | `test_simulation.c:test_historico_cresce_em_ordem`, `test_alocacao_bem_sucedida`, `test_free_existente` |
| 12 | Fragmentação é indefinida quando não há memória livre | `metrics_compute` marca `fragmentation_defined = 0`; `metrics_fragmentation` devolve `0.0` | `test_metrics.c:test_zero_free_memory_is_undefined`, `test_empty_memory_does_not_divide_by_zero` |
| 13 | O pico de fragmentação é o maior valor observado após alguma operação; passos sem memória livre não o zeram | `simulation_observe` após cada tentativa; `metrics_compute` copia o pico | `test_simulation.c:test_fragmentacao_pico_cenario2`; `test_metrics.c` (casos de pico, incluindo 0,5 alcançável) |
| 14 | A carga do cenário 4 é cega (não deriva do estado das estratégias) e determinística por semente | `scenarios_append_stress` gera a sequência uma única vez, sem consultar memórias | `test_simulation.c:test_estresse_deterministico` |

**Cobertura não unitária.** O caminho de falha de `history_reserve` (falta de
memória do próprio vetor) não é forçado nos testes, pois exigiria simular falha
de `realloc`. O gerador de estresse passou a ter teste de determinismo em
`test_simulation.c:test_estresse_deterministico`, mas o **conteúdo** exato da
sequência (quais processos e tamanhos em cada passo) não é fixado em um vetor de
referência: os testes verificam reprodutibilidade, não um snapshot byte a byte
da sequência.

## 5. Decisões de projeto e alternativas rejeitadas

| Decisão | Justificativa | Alternativa rejeitada |
|---------|---------------|------------------------|
| Lista simplesmente encadeada de blocos | O número de partições varia; divisão e fusão inserem/removem nós no meio; não há limite fixo; a ordem de endereço é natural. | Array de tamanho fixo exigiria capacidade máxima e deslocamento de elementos; árvore adicionaria complexidade que não é o foco didático. |
| Busca sequencial O(n) | É o custo real do modelo e é fácil de explicar; `blocks_examined` torna o custo observável. | Índices ou árvores poderiam acelerar a busca, mas esconderiam o custo que se quer ensinar. |
| Best Fit e Worst Fit examinam todos os blocos | Para saber o menor/maior bloco adequado é preciso comparar todos os candidatos. | Parada antecipada não é possível sem uma estrutura ordenada por tamanho. |
| Coalescência imediata | Mantém a invariante “sem blocos livres adjacentes”, reduz fragmentação e simplifica o raciocínio sobre o estado. | Coalescência tardia permitiria mais fragmentação e exigiria uma varredura posterior. |
| Sem compactação | Compactar moveria processos, exigiria relocação dinâmica e esconderia o fenômeno que o simulador quer mostrar. | Compactação automática resolveria alguns pedidos, mas tornaria o cenário 2 trivial e menos didático. |
| Histórico em vetor dinâmico | O número de operações não é conhecido a priori; crescimento por dobragem dá inserção amortizada O(1). | Vetor de tamanho fixo limitaria o número de operações ou desperdiçaria memória. |
| PRNG xorshift32 com semente explícita | É pequeno, sem estado global e produz a mesma sequência em qualquer plataforma com inteiros de 32 bits. | `rand()` da libc não garante a mesma sequência entre plataformas/implementações. |
| Gerador de estresse **cego** em relação às estratégias | A entrada não depende do comportamento dos algoritmos testados; a mesma sequência pode falhar em uma estratégia e ser atendida em outra, permitindo diferenciar taxas de sucesso. | Limitar cada pedido ao menor dos maiores blocos livres das três memórias de sondagem (versão anterior) garantia 100% de sucesso por construção e tornava o experimento circular; taxa e utilização não diferenciavam nada. |
| Pico de fragmentação observado a cada passo | A fragmentação final é só um snapshot; o instante crítico pode ocorrer antes. O pico preserva esse instante (cenário 2: 50.00% contra 17.95% final). | Medir apenas o estado final esconderia a falha por fragmentação que o cenário quer demonstrar. |
| Mapa de memória com vetores no heap | Evita VLA na pilha, cujo tamanho depende do número de blocos e pode estourar a pilha; permite checar falha de alocação e agregar blocos quando há muitos. | VLAs automáticos eram frágeis para entradas grandes e não tinham tratamento de falha. |
| Partições com tamanho exato | Modelo didático de partições variáveis; fragmentação interna zero por construção. | Arredondar para páginas/alinhamento introduziria fragmentação interna e exigiria justificar o tamanho da página. |
| Endereços em KB inteiros | Simplicidade e legibilidade; os números da saída são fáceis de conferir. | Bytes ou páginas exigiriam escala e aritmética adicionais sem ganho conceitual. |
| Um bloco por processo | Permite identificar o processo apenas pelo `pid` e simplifica `FREE`. | Múltiplos blocos por processo exigiriam uma tabela de segmentos por processo. |

**Custo de divisão e de coalescência.** Depois que o bloco é localizado, a
divisão é O(1): basta ajustar o tamanho do bloco escolhido e inserir um novo nó
para o remanescente logo após ele. A coalescência também é O(1) depois que o
bloco liberado e o predecessor são conhecidos: ajustam-se tamanho e ponteiros
e libera-se o nó absorvido. O custo dominante está em **localizar** o bloco:
a busca por um bloco adequado é O(n) (First Fit pode parar cedo), e a própria
liberação precisa varrer a lista para achar o processo e seu predecessor. É por
isso que a escolha da lista encadeada troca “inserção/remoção simples” por
“busca sequencial O(n)”, uma troca adequada ao objetivo didático.

## 6. Como reproduzir

```sh
make          # compila com -std=c11 -Wall -Wextra -Wpedantic -O2
make test     # compila e executa os cinco binários de teste
make asan     # recompila e executa com ASan + UBSan
```

O `Makefile` compila o simulador com:

```text
-std=c11 -Wall -Wextra -Wpedantic -O2 -Isrc -Itests
```

e os testes com as mesmas opções, ligando cada `tests/test_*.c` às fontes de
`src/` (exceto `main.c`).

### 6.1 O que o AddressSanitizer verifica

O alvo `asan` usa:

```text
-std=c11 -Wall -Wextra -Wpedantic -g -O1 \
-fsanitize=address,undefined -fno-omit-frame-pointer
```

- **AddressSanitizer (ASan)**: acessos fora dos limites de buffers, uso após
  `free`, double free, vazamentos de memória e erros de pilha/globo.
- **UndefinedBehaviorSanitizer (UBSan)**: comportamento indefinido, como
  estouro aritmético com sinal, deslocamentos inválidos, divisão por zero,
  conversões inválidas e desreferência de ponteiro nulo.

Resultado observado neste repositório: **1062 verificações, 0 falhas** em
`make test` (173 + 150 + 252 + 86 + 401) e `TESTES COM SANITIZER PASSARAM` em
`make asan`.

## 7. Reprodutibilidade

- Os cenários 1 a 3 têm sequências fixas no código (`src/scenarios.c`).
- O cenário 4 depende apenas dos parâmetros de linha de comando e da semente;
  a mesma semente produz a mesma sequência.
- A comparação usa três memórias independentes, inicializadas com o mesmo
  tamanho e a mesma estratégia por simulação.
- A saída não depende de relógio, números aleatórios do sistema ou ordem de
  iteração de estruturas não determinísticas.
