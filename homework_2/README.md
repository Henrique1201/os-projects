# Simulador Didático de Gerenciamento de Memória Contígua

Simulador em C11 que representa uma região de memória física fictícia como uma
lista encadeada de partições variáveis. O programa compara três estratégias
clássicas de escolha de bloco livre — **First Fit**, **Best Fit** e
**Worst Fit** — sobre a mesma sequência de operações e apresenta os endereços
escolhidos, o mapa da memória e um conjunto de métricas.

O projeto tem finalidade **didática**: ele foi escrito para tornar visíveis,
passo a passo, os conceitos de alocação contígua, divisão de partições,
fragmentação externa, coalescência e critérios gulosos de escolha de bloco.

## O que este projeto é

- Um modelo de **alocação contígua com partições variáveis**.
- Um ambiente controlado para comparar First Fit, Best Fit e Worst Fit sob a
  **mesma sequência** de `ALLOCATE` e `FREE`.
- Um gerador de cenários reproduzíveis, incluindo um cenário de estresse com
  semente explícita.
- Um programa com testes automatizados (1062 verificações), interface de
  terminal e modo não interativo por linha de comando.
- Métricas de fragmentação externa em dois recortes: o **valor final** (snapshot
  do último estado) e a **fragmentação máxima** (maior valor observado após
  alguma operação), que revela o instante crítico que o snapshot pode esconder.

## O que este projeto NÃO é

- **Não é um kernel real** e não reproduz o comportamento de um sistema
  operacional de produção.
- A memória é uma **única região contígua**; não há paginação, segmentação,
  swap, threads, prioridades, tabelas de páginas nem moldura de página.
- Os processos são apenas **identificadores inteiros** (`P1`, `P2`, ...). Não
  há código, pilha, heap por processo ou contexto de execução.
- Os endereços são **deslocamentos em KB** dentro de uma região fictícia; não
  são endereços físicos reais.
- A **compactação não é automática**. O modelo não move blocos ocupados.
- Não há alocação de metadados do próprio sistema operacional: a lista de
  blocos usa `malloc` da biblioteca C, e esse custo não é contabilizado como
  memória do modelo.
- Não há fragmentação interna nas partições: cada bloco ocupado tem exatamente
  o tamanho solicitado. Isso é uma escolha do modelo, não uma regra geral de
  sistemas reais.

> Observação sobre acentuação: toda a documentação em Markdown usa português
> acentuado (UTF-8). O código-fonte e as mensagens exibidas no terminal evitam
> acentos por compatibilidade com terminais e locales diversos; por isso a
> saída do binário aparece sem acentos.

## Requisitos

- Compilador **GCC** com suporte a C11.
- **GNU Make**.
- Ambiente POSIX/Linux (o `Makefile` usa `mkdir`, `rm`, `for` e `test`).
- Opcional: `gcc` com **AddressSanitizer** e **UndefinedBehaviorSanitizer**
  para o alvo `make asan`.

Não há dependências externas de bibliotecas.

## Compilação

```sh
make
```

O binário é gerado em `build/memory-simulator`.

Outros alvos:

```sh
make run    # compila e abre o menu interativo
make test   # compila e executa todos os testes automatizados
make asan   # compila e executa os testes com ASan + UBSan
make clean  # remove build/
make help   # lista os alvos
```

## Execução

### Modo interativo (menu)

```sh
./build/memory-simulator
```

Menu:

```text
1 Executar First Fit
2 Executar Best Fit
3 Executar Worst Fit
4 Comparar todos os algoritmos
5 Executar cenario personalizado
6 Visualizar conceitos teoricos
0 Sair
```

A opção **4** executa os quatro cenários e mostra, para cada um, a tabela
comparativa das três estratégias. No cenário 3 ela também exibe a tabela de
endereços escolhidos passo a passo. A opção **5** abre um cenário personalizado
(1024 KB por padrão, ajustável com `--custom-mem=KB`), no qual o usuário digita
`ALLOCATE`, `FREE`, `MAP`, `HISTORY`, `METRICS`, `HELP` e `QUIT`.

### Modo não interativo (reprodutível)

```sh
./build/memory-simulator --scenario=1 --strategy=4
./build/memory-simulator --scenario=2 --strategy=4
./build/memory-simulator --scenario=3 --strategy=4
./build/memory-simulator --scenario=4 --strategy=4 \
  --stress-mem=1024 --stress-procs=8 --stress-min=50 --stress-max=300 \
  --stress-ops=200 --stress-seed=42
```

- `--strategy=1` First Fit, `2` Best Fit, `3` Worst Fit, `4` comparar todos.
- `--scenario=N` restringe a execução ao cenário `N` (1 a 4).
- Para o cenário 4, os parâmetros de estresse têm valores padrão:
  memória 1024 KB, 8 processos, tamanhos entre 50 e 300 KB, 200 operações,
  semente 42.
- `--custom-mem=KB` define o tamanho de memória do cenário personalizado
  (opção 5 do menu; padrão 1024 KB, faixa 1..1000000).
- `./build/memory-simulator --help` mostra todas as opções.

No traçado detalhado dos cenários 1 a 3 (quando se escolhe uma estratégia), o
programa imprime, após cada operação, uma linha compacta como
`Estado: livre=512 KB | maior bloco=256 KB | fragmentacao=50.00% | blocos livres=2`.
É essa linha que torna visível o instante da falha do cenário 2.

### Cenários de exemplo

A pasta `examples/` contém dois tipos de arquivo:

- `examples/scenario_01.txt` a `examples/scenario_04_stress.txt`: **entradas**
  para o cenário personalizado (opção 5). Elas podem ser digitadas ou coladas
  no prompt `>`.
- `examples/saida_cenario_01.txt` a
  `examples/saida_cenario_04_stress.txt`: **saídas reais** capturadas do
  binário, com o comando exato no cabeçalho.

Exemplo de uso de uma entrada no modo personalizado:

```sh
./build/memory-simulator
# escolha a opcao 5, informe a estrategia 1 e cole o conteudo de
# examples/scenario_02.txt
```

Exemplo de captura direta de uma saída:

```sh
./build/memory-simulator --scenario=3 --strategy=4 > /tmp/saida_cenario_03.txt
# depois acrescente ao arquivo o cabeçalho com o comando exato,
# como nos arquivos examples/saida_cenario_0*.txt
```

Os arquivos `examples/saida_cenario_0*.txt` contêm, além do cabeçalho de
comentário, exatamente a saída produzida pelos comandos ali indicados.

## Cenários

| # | Nome | Memória | Sequência | Fenômeno observado |
|---|------|---------|-----------|--------------------|
| 1 | Alocações sequenciais | 1024 KB | 4 alocações, sem liberações | Empacotamento inicial; as três estratégias escolhem os mesmos endereços |
| 2 | Fragmentação externa | 1024 KB | 4×256 KB, libera P2 e P4, pede 400 KB e 200 KB | Há 512 KB livres, mas o maior bloco contíguo é 256 KB: o pedido de 400 KB falha |
| 3 | Critérios de escolha de bloco | 1024 KB | Alocações separadas por P2/P4, três liberações, pedidos de 180 e 250 KB | First/Best/Worst escolhem blocos diferentes; endereços 0/400/700 e 700/0/0 |
| 4 | Estresse pseudoaleatório | 1024 KB | 200 operações, 8 processos, semente 42 | Carga mista reproduzível e **cega em relação às estratégias**: as taxas de sucesso/falha podem diferir, porque a carga não é limitada pelo que os algoritmos conseguem atender |

A correção metodológica do cenário 4 está em
[`docs/RESULTADOS.md`](docs/RESULTADOS.md): antes, o gerador limitava cada pedido
ao menor dos maiores blocos livres das três simulações, garantindo 100% de
sucesso por construção; agora os tamanhos são sorteados livremente em `[50, 300]`
KB, a sequência é a mesma para as três estratégias e as falhas passam a
diferenciá-las. Detalhes experimentais e tabelas reais no mesmo documento.

## Estrutura de diretórios

| Caminho | Descrição |
|---------|-----------|
| `Makefile` | Compilação, testes, sanitizers e limpeza. |
| `src/memory.h`, `src/memory.c` | Modelo da região de memória: lista de blocos, divisão, liberação, coalescência e validação. |
| `src/allocation.h`, `src/allocation.c` | Critérios de escolha de bloco: First Fit, Best Fit e Worst Fit. |
| `src/metrics.h`, `src/metrics.c` | Cálculo das métricas, do indicador de fragmentação externa e do pico de fragmentação. |
| `src/simulation.h`, `src/simulation.c` | Orquestração de uma simulação: memória + estratégia + histórico + contadores. |
| `src/scenarios.h`, `src/scenarios.c` | Cenários controlados e gerador pseudoaleatório reproduzível (xorshift32). |
| `src/visualization.h`, `src/visualization.c` | Apresentação no terminal: mapa, tabelas, métricas e histórico. |
| `src/main.c` | Interface de linha de comando, menu interativo e cenário personalizado. |
| `tests/test_utils.h` | Micro framework de testes (`TEST_CHECK`, `TEST_RUN`, `TEST_SUMMARY`). |
| `tests/test_memory.c` | Testes de inicialização, alocação, liberação, consultas, invariantes, rejeição de blocos livres adjacentes e predicado `memory_block_is_free`. |
| `tests/test_coalescing.c` | Testes de coalescência entre blocos adjacentes e nas extremidades. |
| `tests/test_allocation.c` | Testes dos critérios de escolha e da convenção de blocos examinados. |
| `tests/test_metrics.c` | Testes das métricas, incluindo o caso indefinido e o pico de fragmentação. |
| `tests/test_simulation.c` | Testes de integração, histórico, contadores, independência, pico do cenário 2 e determinismo do cenário de estresse. |
| `examples/scenario_0*.txt` | Entradas de exemplo para o cenário personalizado. |
| `examples/saida_cenario_0*.txt` | Saídas reais capturadas do binário. |
| `docs/` | Documentação acadêmica do projeto. |
| `build/` | Artefatos de compilação (ignorado pelo Git). |

## Testes

Os testes ficam em `tests/` e são compilados a partir de `tests/test_*.c`.

```sh
make test
```

Resultado esperado: **1062 verificações, 0 falhas** (soma dos cinco binários de
teste: 173 + 150 + 252 + 86 + 401), com a mensagem final
`TODOS OS TESTES PASSARAM`.

```sh
make asan
```

O alvo `asan` recompila os testes com
`-fsanitize=address,undefined -fno-omit-frame-pointer` e os executa. A mesma
suíte de **1062 verificações** passa sob ASan + UBSan. O AddressSanitizer
detecta acessos fora dos limites, uso após liberação e vazamentos; o
UndefinedBehaviorSanitizer detecta comportamento indefinido, como estouro
aritmético com sinal, deslocamentos inválidos e ponteiros nulos em contextos
indevidos.

## Documentação

- [`docs/FUNDAMENTACAO.md`](docs/FUNDAMENTACAO.md) — base teórica, os três
  algoritmos, complexidade e cuidados sobre o que não se deve afirmar.
- [`docs/ARQUITETURA.md`](docs/ARQUITETURA.md) — camadas, módulos, funções,
  invariantes, decisões de projeto e reprodutibilidade.
- [`docs/METRICAS.md`](docs/METRICAS.md) — definição, fórmula, unidade e
  limites de cada métrica.
- [`docs/RESULTADOS.md`](docs/RESULTADOS.md) — experimentos reais, tabelas
  capturadas e interpretação separada da teoria.
- [`docs/APRESENTACAO.md`](docs/APRESENTACAO.md) — roteiro de apresentação,
  sugestão de slides, demonstração e perguntas prováveis.

## Referências principais

As referências completas, com os trechos usados e as ressalvas de leitura,
estão em [`docs/FUNDAMENTACAO.md`](docs/FUNDAMENTACAO.md). Os autores centrais
são Knuth (1997), Tanenbaum e Bos (2014), Silberschatz, Galvin e Gagne (2018),
Bryant e O'Hallaron (2016), Wilson et al. (1995), Arpaci-Dusseau e
Arpaci-Dusseau (2023), além dos artigos de Robson (1977) e Shore (1975).
