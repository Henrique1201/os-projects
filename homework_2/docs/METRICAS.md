# Métricas

Este documento define cada métrica exibida pelo simulador, sua fórmula, sua
unidade, onde ela é calculada no código, o que ela significa e o que ela **não**
significa. Os valores medidos nos experimentos estão em
[`RESULTADOS.md`](RESULTADOS.md).

## 1. Convenções gerais

### 1.1 Unidades

- Tamanhos de memória e de blocos: **KB**.
- Endereços: **KB** a partir do início da região fictícia (offset).
- Taxas e percentuais: **%**.
- Fragmentação: número adimensional em `[0, 1]`, exibido multiplicado por 100
  como percentual.
- Contagens: números inteiros.

### 1.2 “Total de solicitações”

`Total de solicitacoes` (`total_requests`) conta **apenas solicitações de
alocação**. Formalmente:

```text
total_requests = successful_allocations + failed_allocations
```

Uma tentativa de `FREE` que falha (processo não encontrado) **não** entra nesse
total, porque não é um pedido de alocação. Ela aparece no histórico como
`FALHA`, mas não altera `failed_allocations`. Essa convenção está implementada
em `src/metrics.c:metrics_compute` e em `src/simulation.c:simulation_execute`
(comentário explícito no ramo de `OPERATION_FREE`).

Consequência prática: no cenário 4 com 200 operações e os parâmetros padrão, o
total de solicitações é **103**, não 200, porque as outras 97 operações são
liberações.

Uma falha de `FREE` (processo não encontrado) aparece no histórico como
`FALHA`, mas **não** entra em `failed_allocations` nem nas taxas de sucesso e
falha. Esse é um caso possível no cenário 4: como o gerador é cego em relação
às estratégias, um `ALLOCATE` pode falhar em uma estratégia e, mais tarde, o
`FREE` correspondente não encontrar o processo **naquela** estratégia. A falha
de `FREE` não é uma solicitação de alocação recusada; por isso fica fora das
taxas.

### 1.3 “Blocos examinados”

A convenção está declarada explicitamente em `src/allocation.h`:
`blocks_examined` é a quantidade de blocos **percorridos na busca de seleção do
bloco livre**, incluindo blocos ocupados atravessados antes do bloco escolhido.

- **First Fit** pode encerrar a busca no primeiro bloco adequado; os blocos
  posteriores não são contados.
- **Best Fit** e **Worst Fit** percorrem a lista inteira para comparar todos os
  candidatos.
- Uma falha por ausência de bloco adequado conta todos os blocos percorridos
  até o fim da lista.
- Casos de curto-circuito (`processo já alocado`, `pedido maior que a memória
  total`) resultam em `blocks_examined = 0`, porque nenhuma busca é feita.
- Operações de `FREE` têm `blocks_examined = 0` no registro, pois não fazem
  busca de bloco livre.
- O acumulado da simulação é `total_blocks_examined`, somado em
  `src/simulation.c:simulation_execute` a partir de
  `AllocationResult.blocks_examined` produzido em
  `src/allocation.c:allocation_execute`.

A contagem **não inclui** os percursos auxiliares de manutenção: localizar o
processo (`memory_find_process`), conferir a pertinência do bloco à lista em
`memory_allocate_in_block` e recalcular os contadores (`recount_blocks`). Esses
percursos são idênticos nas três estratégias e, por isso, ficam fora do
contador. A consequência é que `total_blocks_examined` é um **proxy
comparativo do custo de busca da política**, e **não** o tempo total da
operação nem o custo total da chamada de alocação.

**Relação com a complexidade.** Em uma lista simplesmente encadeada:

- First Fit: O(n) no pior caso, O(1) no melhor caso para a busca.
- Best Fit: Theta(n).
- Worst Fit: Theta(n).

O contador é o **proxy empírico** desse custo de busca. Ele não mede o custo
total da chamada de alocação, pois `allocation_execute` também chama
`memory_find_process` (O(n) quando o processo não existe) e
`memory_allocate_in_block` verifica a pertinência do bloco à lista (outra
travessia O(n)). Portanto, `blocks_examined` deve ser lido como “custo da
política de escolha”, não como “custo total do programa”.

### 1.4 Fragmentação: indicador operacional

A convenção adotada é:

```text
fragmentation = 1 - (largest_free_block / free_memory)
```

com `fragmentation` restrita a `[0, 1]` por `clamp_unit`
(`src/metrics.c`). A interface exibe `fragmentation * 100` como percentual.

- `fragmentation = 0` quando toda a memória livre está em um único bloco
  contíguo.
- `fragmentation` tende a `1` quando a memória livre está muito espalhada em
  blocos pequenos.
- Se `free_memory = 0`, o indicador é **INDEFINIDO**: `fragmentation_defined =
  0` e `fragmentation = 0.0`. A interface mostra `indefinida`, em vez de
  inventar um valor.
- Se `free_memory < 0` (situação impossível no modelo, mas tratada
  defensivamente), `metrics_fragmentation` também devolve `0.0`.

**O que esse indicador significa.** É uma medida da **geometria da memória
livre** no instante da medição: quanto o maior bloco livre representa do total
livre. Ele é útil para comparar estados e para ilustrar o cenário 2.

**Atenção ao instante.** O campo `fragmentation` descreve apenas o **estado
final** da simulação — um snapshot. O instante crítico pode ter ocorrido antes e
não aparecer nesse snapshot. Por isso a simulação mantém também o **pico** de
fragmentação (`peak_fragmentation`), descrito na seção 3.8.

**O que ele NÃO significa.**

- Não é uma medida universal de fragmentação externa adotada por todos os
  sistemas operacionais.
- Não mede fragmentação interna (que neste modelo é zero por construção).
- Não é monotônico em relação à “qualidade” da memória: ao consumir parte de um
  bloco livre grande, o total livre diminui e o indicador pode até diminuir,
  mesmo que a memória esteja mais apertada. Exemplo: no cenário 2, no instante
  da falha do pedido de 400 KB, há 512 KB livres em dois blocos de 256 KB; pela
  fórmula, o indicador é `1 - 256/512 = 50%`, e o binário imprime essa linha de
  estado (`Estado: livre=512 KB | maior bloco=256 KB | fragmentacao=50.00% |
  blocos livres=2`). Ao final, depois de alocar 200 KB, restam 312 KB livres, o
  maior bloco tem 256 KB e o binário imprime **17.95%**, embora ainda exista um
  buraco de 56 KB. O número final é menor, mas a memória não ficou “melhor” para
  pedidos grandes. O snapshot final esconde o pico; a seção 3.8 trata disso.
- Não substitui a análise da distribuição dos blocos livres. Dois estados com o
  mesmo percentual podem ter geometrias diferentes.

## 2. Métricas de solicitações e taxas

### 2.1 Total de solicitações (`total_requests`)

- **Definição.** Quantidade de pedidos de alocação processados.
- **Fórmula.** `successful_allocations + failed_allocations`.
- **Unidade.** Contagem.
- **Código.** `src/metrics.c:metrics_compute`.
- **Significado.** Denominador das taxas de sucesso e falha.
- **Não significa.** Não inclui `FREE`; não é o número total de operações da
  sequência.
- **Limite de interpretação.** Uma sequência com muitas liberações pode ter
  `total_requests` bem menor que o número de operações (cenário 4: 103
  solicitações em 200 operações).

### 2.2 Alocações realizadas (`successful_allocations`)

- **Definição.** Pedidos de alocação atendidos.
- **Fórmula.** Contador incrementado em `simulation_execute` quando
  `AllocationResult.success == 1`.
- **Unidade.** Contagem.
- **Código.** `src/simulation.c:simulation_execute`; lido em
  `src/metrics.c:metrics_compute`.
- **Significado.** Quantidade de pedidos que receberam um bloco.
- **Não significa.** Não indica que o processo permaneceu alocado até o fim;
  uma alocação bem-sucedida pode ter sido liberada depois.
- **Limite.** Não mede qualidade da escolha de endereço.

### 2.3 Alocações com falha (`failed_allocations`)

- **Definição.** Pedidos de alocação não atendidos.
- **Fórmula.** Contador incrementado quando a tentativa é processada e
  `success == 0` (tipicamente `MEMORY_ERROR_NO_SPACE`).
- **Unidade.** Contagem.
- **Código.** `src/simulation.c:simulation_execute`; lido em
  `src/metrics.c:metrics_compute`.
- **Significado.** Pedidos que não encontraram bloco contíguo suficiente.
- **Não significa.** Não inclui falhas de `FREE`; não inclui argumentos
  inválidos que retornam `-1` antes de qualquer tentativa.
- **Limite.** Uma falha pode ocorrer por fragmentação externa mesmo com memória
  livre total suficiente.

### 2.4 Taxa de sucesso (`success_rate_percent`)

- **Definição.** Percentual de pedidos de alocação atendidos.
- **Fórmula.** `successful_allocations / total_requests * 100`.
- **Unidade.** %.
- **Código.** `src/metrics.c:metrics_compute`.
- **Significado.** Fração dos pedidos que receberam memória.
- **Não significa.** Não mede a qualidade do endereço escolhido nem a
  fragmentação resultante.
- **Limite.** Se `total_requests = 0`, o valor é `0.0` (divisão por zero
  evitada); isso não é “0% de sucesso”, é ausência de solicitações.
- **Exemplo medido.** Cenário 1: `100.00%`; cenário 2: `83.33%`; cenário 3:
  `100.00%`; cenário 4: `66.02%`, `71.84%` e `68.93%` (First, Best e Worst
  Fit).

### 2.5 Taxa de falha (`failure_rate_percent`)

- **Definição.** Percentual de pedidos de alocação não atendidos.
- **Fórmula.** `failed_allocations / total_requests * 100`.
- **Unidade.** %.
- **Código.** `src/metrics.c:metrics_compute`.
- **Significado.** Fração dos pedidos que falharam.
- **Não significa.** Não é “percentual de memória desperdiçada”.
- **Limite.** Se `total_requests = 0`, o valor é `0.0`.
- **Relação.** `success_rate_percent + failure_rate_percent = 100` quando há
  solicitações.

## 3. Métricas de memória

### 3.1 Memória total (`total_memory`)

- **Definição.** Tamanho total da região simulada.
- **Fórmula.** `memory->total_size`.
- **Unidade.** KB.
- **Código.** `src/metrics.c:metrics_compute`.
- **Significado.** Capacidade da região fictícia.
- **Não significa.** Não é a memória física da máquina que executa o programa.
- **Limite.** É fixa durante uma simulação; `memory_reset` a preserva.

### 3.2 Memória ocupada (`used_memory`)

- **Definição.** Soma dos tamanhos dos blocos ocupados.
- **Fórmula.** `memory_total_used(memory)`.
- **Unidade.** KB.
- **Código.** `src/memory.c:memory_total_used`, chamada por
  `src/metrics.c:metrics_compute`.
- **Significado.** Quanto da região está reservado a processos.
- **Não significa.** Não inclui metadados da lista (os nós alocados com
  `malloc`), que não fazem parte do modelo.
- **Limite.** Pode ser menor que a soma dos pedidos originais se processos
  foram liberados.

### 3.3 Memória livre (`free_memory`)

- **Definição.** Soma dos tamanhos de todos os blocos livres.
- **Fórmula.** `memory_total_free(memory)`.
- **Unidade.** KB.
- **Código.** `src/memory.c:memory_total_free`, chamada por
  `src/metrics.c:metrics_compute`.
- **Significado.** Total de memória disponível, **independentemente de
  contiguidade**.
- **Não significa.** Memória livre total suficiente **não** garante que um
  pedido será atendido. O cenário 2 mostra 512 KB livres e um pedido de 400 KB
  falhando porque o maior bloco contíguo é 256 KB.
- **Limite.** É a base do denominador da fragmentação; quando vale zero, o
  indicador é indefinido.
- **Relação.** `used_memory + free_memory = total_memory` (invariante
  verificada por `memory_validate_ex`).

### 3.4 Maior bloco livre (`largest_free_block`)

- **Definição.** Tamanho do maior bloco livre contíguo.
- **Fórmula.** `max(size)` entre os blocos com `state == BLOCK_FREE`.
- **Unidade.** KB.
- **Código.** `src/memory.c:memory_largest_free_block`, chamada por
  `src/metrics.c:metrics_compute`.
- **Significado.** Maior pedido que poderia ser atendido imediatamente por um
  bloco contíguo (desconsiderando políticas).
- **Não significa.** Não é a memória livre total; não indica quantos blocos
  livres existem.
- **Limite.** Se não há blocos livres, vale `0`.
- **Exemplo medido.** Cenário 2: 256 KB; cenário 3: 200, 324 e 200 KB para
  First, Best e Worst Fit; cenário 4: 140, 159 e 205 KB.

### 3.5 Blocos livres (`free_block_count`)

- **Definição.** Quantidade de regiões livres.
- **Fórmula.** Contagem de blocos com `state == BLOCK_FREE`.
- **Unidade.** Contagem.
- **Código.** `src/memory.c:memory_count_free_blocks`, chamada por
  `src/metrics.c:metrics_compute`.
- **Significado.** Grau de dispersão da memória livre.
- **Não significa.** Sozinho, não mede fragmentação: três blocos livres do
  mesmo tamanho e três blocos livres de tamanhos muito diferentes têm a mesma
  contagem.
- **Limite.** Após coalescência imediata, não há dois blocos livres
  adjacentes; ainda assim pode haver muitos blocos separados por ocupados.
- **Exemplo medido.** Cenário 3: 3 blocos livres nas três estratégias; cenário
  4: 3, 3 e 3.

### 3.6 Blocos ocupados (`occupied_block_count`)

- **Definição.** Quantidade de regiões ocupadas.
- **Fórmula.** Contagem de blocos com `state == BLOCK_OCCUPIED`.
- **Unidade.** Contagem.
- **Código.** `src/memory.c:memory_count_occupied_blocks`, chamada por
  `src/metrics.c:metrics_compute`.
- **Significado.** Número de processos atualmente alocados, já que o modelo
  garante um bloco por processo.
- **Não significa.** Não é o número de processos criados ao longo da
  simulação; processos liberados não contam.
- **Limite.** No modelo, coincide com o número de processos vivos.

### 3.7 Fragmentação externa (`fragmentation` e `fragmentation_defined`)

- **Definição.** Indicador operacional da dispersão da memória livre.
- **Fórmula.** `clamp_unit(1 - largest_free_block / free_memory)`, quando
  `free_memory > 0`; caso contrário, `0.0` com `fragmentation_defined = 0`.
- **Unidade.** Adimensional; exibido como %.
- **Código.** `src/metrics.c:metrics_fragmentation` e
  `src/metrics.c:metrics_compute`.
- **Significado.** Quanto da memória livre está “presa” em blocos menores que o
  maior bloco. Quanto mais próximo de 100%, mais espalhada está a memória livre.
- **Não significa.** Não é uma medida universal; não mede fragmentação
  interna; não é monotônica em relação ao aperto de memória; não é “percentual
  de memória desperdiçada”.
- **Limite.** Indefinida quando `free_memory = 0` (a interface mostra
  `indefinida`). O valor pode ser 0% tanto para uma memória inteiramente livre
  quanto para uma memória com um único bloco livre pequeno (se `largest ==
  free_total`).
- **Exemplo medido.** Cenário 1: 0.00%; cenário 2: 17.95%; cenário 3: 49.24%,
  17.77% e 49.24%; cenário 4: 62.16%, 57.03% e 44.59%.

### 3.8 Fragmentação máxima (`peak_fragmentation` e `peak_fragmentation_defined`)

- **Definição.** O **maior valor de fragmentação externa observado após alguma
  operação** da simulação. É o pico ao longo da execução, e não o valor do
  estado final.
- **Fórmula.** `max` de `1 - largest_free_block / free_memory` sobre todos os
  passos em que `free_memory > 0`, usando a mesma fórmula da seção 3.7. O estado
  após **cada** tentativa de `ALLOCATE` ou `FREE` é observado pela camada de
  simulação, em `src/simulation.c` (função interna `simulation_observe`).
- **Unidade.** Adimensional; exibido como %.
- **Código.** Observado em `src/simulation.c:simulation_observe`, armazenado em
  `Simulation.peak_fragmentation` / `Simulation.peak_fragmentation_defined` e
  copiado para `Metrics` por `src/metrics.c:metrics_compute`.
- **Significado.** Mostra o **instante crítico** da execução. A fragmentação
  final é apenas um snapshot: depois do pico, uma alocação pode consumir parte
  da memória livre e reduzir o indicador, escondendo a pior geometria. O pico
  preserva esse instante.
- **Relação com o cenário de fragmentação externa.** No cenário 2, o pico é
  **50.00%**: após liberar P2 e P4, há 512 KB livres em dois blocos de 256 KB e
  o pedido de 400 KB falha. A fragmentação final é **17.95%**, porque o pedido
  seguinte de 200 KB consome parte de um bloco livre. O valor final, sozinho,
  não revelaria a falha; o pico sim. No cenário 4, o pico também é maior que o
  valor final em todas as estratégias.
- **Não significa.** Não é o valor final, nem o valor “médio”, nem uma garantia
  de que aquele instante foi o único problemático. Não mede fragmentação
  interna. Como o indicador de fragmentação, não é uma medida universal.
- **Limite (caso indefinido).** Se a memória nunca teve `free_memory > 0` em
  nenhum passo observado (por exemplo, memória totalmente ocupada desde o
  início), o pico é **INDEFINIDO**: `peak_fragmentation_defined = 0`,
  `peak_fragmentation = 0.0`, e a interface mostra `indefinida`. Um passo sem
  memória livre não zera o pico anterior: ele é apenas ignorado, e o maior valor
  definido já observado é preservado.
- **Exemplo medido.** Cenário 1: 0.00%; cenário 2: 50.00%; cenário 3: 60.68% nas
  três estratégias; cenário 4: 63.57%, 59.68% e 67.32% (First, Best e Worst
  Fit).

## 4. Métricas de ocupação e custo

### 4.1 Utilização (`utilization_percent`)

- **Definição.** Percentual da memória total ocupado.
- **Fórmula.** `used_memory / total_memory * 100`.
- **Unidade.** %.
- **Código.** `src/metrics.c:metrics_compute`.
- **Significado.** Fração da região reservada a processos no estado final.
- **Não significa.** Não mede eficiência da alocação nem fragmentação; dois
  estados com a mesma utilização podem ter geometrias completamente diferentes.
- **Limite.** Se `total_memory = 0`, o valor é `0.0`.
- **Exemplo medido.** Cenário 1: 73.24%; cenário 2: 69.53%; cenário 3:
  61.52%; cenário 4: 63.87% nas três estratégias.

### 4.2 Blocos examinados (`total_blocks_examined`)

- **Definição.** Soma dos blocos percorridos nas buscas de seleção do bloco
  livre, ao longo da simulação.
- **Fórmula.** Soma de `AllocationResult.blocks_examined` ao longo da
  simulação.
- **Unidade.** Contagem de blocos.
- **Código.** Acumulado em `src/simulation.c:simulation_execute`; calculado em
  `src/allocation.c:allocation_execute`; exposto por
  `src/metrics.c:metrics_compute`.
- **Significado.** Proxy comparativo do custo de busca da política de escolha.
- **Não significa.** Não é tempo de execução, não é número de comparações de
  tamanho, não inclui as travessias auxiliares O(n) de localização do processo,
  de conferência de pertinência do bloco e de recálculo dos contadores (que são
  idênticas nas três estratégias) e não conta operações de `FREE`.
- **Limite.** First Fit pode parecer muito mais barato porque para cedo; isso é
  uma propriedade real da busca, mas não necessariamente do custo total da
  chamada. Compare com a seção 1.3.
- **Exemplo medido.** Cenário 2: 16, 18 e 18; cenário 3: 22, 26 e 26; cenário 4:
  460, 563 e 550.

## 5. Resumo das métricas

| Métrica | Fórmula | Unidade | Código |
|---------|---------|---------|--------|
| `total_requests` | sucessos + falhas de alocação | contagem | `metrics.c:metrics_compute` |
| `successful_allocations` | contador de alocações atendidas | contagem | `simulation.c:simulation_execute` |
| `failed_allocations` | contador de alocações não atendidas | contagem | `simulation.c:simulation_execute` |
| `total_memory` | `memory->total_size` | KB | `metrics.c:metrics_compute` |
| `used_memory` | soma dos blocos ocupados | KB | `memory.c:memory_total_used` |
| `free_memory` | soma dos blocos livres | KB | `memory.c:memory_total_free` |
| `largest_free_block` | máximo dos blocos livres | KB | `memory.c:memory_largest_free_block` |
| `free_block_count` | contagem de blocos livres | contagem | `memory.c:memory_count_free_blocks` |
| `occupied_block_count` | contagem de blocos ocupados | contagem | `memory.c:memory_count_occupied_blocks` |
| `fragmentation` | `1 - maior_livre / livre_total` | adimensional | `metrics.c:metrics_fragmentation` |
| `fragmentation_defined` | 1 se `free_memory > 0`, senão 0 | booleano | `metrics.c:metrics_compute` |
| `peak_fragmentation` | máximo de `1 - maior_livre / livre_total` sobre os passos observados | adimensional | `simulation.c:simulation_observe`; copiado em `metrics.c:metrics_compute` |
| `peak_fragmentation_defined` | 1 se houve algum passo com `free_memory > 0`, senão 0 | booleano | `simulation.c:simulation_observe`; copiado em `metrics.c:metrics_compute` |
| `utilization_percent` | `usado / total * 100` | % | `metrics.c:metrics_compute` |
| `success_rate_percent` | `sucessos / solicitacoes * 100` | % | `metrics.c:metrics_compute` |
| `failure_rate_percent` | `falhas / solicitacoes * 100` | % | `metrics.c:metrics_compute` |
| `total_blocks_examined` | soma de `blocks_examined` | contagem | `simulation.c:simulation_execute` |

## 6. Erros de interpretação a evitar

1. **“Há 512 KB livres, então um pedido de 400 KB deveria caber.”** Não
   necessariamente: o pedido precisa de 400 KB **contíguos**. Esse é o cenário
   2.
2. **“Fragmentação de 0% significa memória vazia.”** Não. Significa que toda a
   memória livre existente está em um único bloco. Pode haver 1 KB livre e o
   indicador ser 0%.
3. **“Fragmentação de 100% significa memória cheia.”** Não. Significa que o
   maior bloco livre é desprezível em relação ao total livre; ainda pode haver
   memória livre. Memória cheia é o caso **indefinido**, não 100%.
4. **“Best Fit tem menos fragmentação, então é sempre melhor.”** Não. O
   indicador é instantâneo e não captura a evolução futura; Best Fit pode criar
   fragmentação adicional ao longo do tempo.
5. **“Menos blocos examinados significa algoritmo mais rápido em tudo.”** Não.
   O contador mede a busca da política; há travessias auxiliares não
   contabilizadas e o custo total depende também de `FREE` e da estrutura de
   dados.
6. **“Taxa de sucesso de 100% significa que a estratégia é ótima.”** Não.
   Significa apenas que, para aquela sequência, todos os pedidos foram
   atendidos.
7. **“A fragmentação final é o pior momento da execução.”** Não
   necessariamente. Ela é um snapshot; o pior instante pode ter ocorrido antes
   e ser maior. Por isso existe a “Fragmentação máxima”, que observa o estado
   após cada operação. No cenário 2, o pico é 50.00% e o valor final é 17.95%.
