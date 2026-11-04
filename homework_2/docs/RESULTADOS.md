# Resultados

Este documento separa explicitamente duas coisas:

1. **RESULTADOS MEDIDOS (simulações deste projeto)** — números lidos da saída
   real do binário `build/memory-simulator` deste repositório.
2. **INTERPRETAÇÃO TEÓRICA (hipóteses da literatura)** — o que a literatura
   afirma e como ela ajuda a interpretar os números, sem transformar hipótese
   em medição.

> **Aviso.** Todos os números abaixo são resultados **deste simulador**, não de
> um kernel real. O modelo usa uma única região contígua fictícia, endereços em
> KB, partições de tamanho exato e nenhuma compactação automática. Não é
> possível extrapolar taxas, fragmentação ou tempos para um sistema
> operacional de produção.

## 1. Metodologia

### 1.1 Ambiente da captura

- Sistema: Linux `7.0.0-38-generic` x86_64.
- Compilador: `gcc (Ubuntu 13.3.0-6ubuntu2~24.04.1) 13.3.0`.
- Data da captura: 2026-11-03.
- Compilação: `make` (C11, `-Wall -Wextra -Wpedantic -O2`).
- Saídas de referência: `examples/saida_cenario_01.txt`,
  `examples/saida_cenario_02.txt`, `examples/saida_cenario_03.txt` e
  `examples/saida_cenario_04_stress.txt`.

### 1.2 Comandos exatos

```sh
make
make test
make asan

./build/memory-simulator --scenario=1 --strategy=4
./build/memory-simulator --scenario=2 --strategy=4
./build/memory-simulator --scenario=3 --strategy=4
./build/memory-simulator --scenario=4 --strategy=4 \
  --stress-mem=1024 --stress-procs=8 --stress-min=50 --stress-max=300 \
  --stress-ops=200 --stress-seed=42
```

As tabelas da seção 2 foram capturadas com `--strategy=4`, que executa a
**mesma sequência** em três simulações independentes (uma por estratégia). Os
cenários 1, 2 e 3 também foram inspecionados com `--strategy=1` para conferir o
mapa detalhado e as linhas de estado após cada operação; os arquivos
`examples/saida_cenario_01.txt` e `examples/saida_cenario_02.txt` registram
essas capturas detalhadas.

### 1.3 Comparação justa

Cada estratégia recebe a mesma lista de `Operation`, na mesma ordem, em uma
`Memory` própria, reiniciada com o mesmo tamanho total. Não há estado global
compartilhado entre as simulações. A igualdade da linha “Total de solicitacoes”
nas três colunas é uma verificação de que as três processaram a mesma
quantidade de pedidos de alocação — mesmo quando o número de **sucessos** difere,
como no cenário 4 atual.

Isso permite atribuir as diferenças observadas ao **critério de escolha de
bloco**, e não a sequências diferentes. Não permite atribuir as diferenças a
“qualidade intrínseca” de cada algoritmo: a carga é um cenário específico.

### 1.4 Semente e geração do cenário de estresse

O cenário 4 usa `--stress-seed=42`. A mesma semente produz exatamente a mesma
sequência de operações, porque o gerador é xorshift32 com estado local
(`src/scenarios.c`). A sequência tem 200 operações, 8 processos e tamanhos
sorteados em `[50, 300]` KB.

O gerador é **cego em relação às estratégias**: ele não consulta nenhuma
memória simulada para decidir o tamanho de um pedido. Os tamanhos são sorteados
livremente no intervalo e a sequência é gerada **uma única vez**; cada
estratégia a reaplica em uma memória nova. Isso preserva a comparação justa
(mesma sequência para as três) e permite que uma estratégia atenda um pedido
que outra recusa. A seção 2.4.1 detalha por que a versão anterior do gerador
era circular.

### 1.5 Validação do código

| Verificação | Comando | Resultado observado |
|-------------|---------|---------------------|
| Testes automatizados | `make test` | 1062 verificações, 0 falhas; `TODOS OS TESTES PASSARAM` |
| Sanitizers | `make asan` | 1062 verificações, 0 falhas; `TESTES COM SANITIZER PASSARAM` |

Detalhamento dos testes por binário:

| Binário | Verificações | Falhas |
|---------|--------------|--------|
| `build/test_allocation` | 173 | 0 |
| `build/test_coalescing` | 150 | 0 |
| `build/test_memory` | 252 | 0 |
| `build/test_metrics` | 86 | 0 |
| `build/test_simulation` | 401 | 0 |
| **Total** | **1062** | **0** |

---

## 2. RESULTADOS MEDIDOS (simulações deste projeto)

### 2.1 Cenário 1 — Alocações sequenciais

Sequência: `ALLOCATE P1 100`, `ALLOCATE P2 200`, `ALLOCATE P3 150`,
`ALLOCATE P4 300`. Memória total: 1024 KB.

Tabela capturada de `./build/memory-simulator --scenario=1 --strategy=4`:

| Métrica | First Fit | Best Fit | Worst Fit |
|---------|-----------|----------|-----------|
| Total de solicitações | 4 | 4 | 4 |
| Alocações realizadas | 4 | 4 | 4 |
| Alocações com falha | 0 | 0 | 0 |
| Taxa de sucesso (%) | 100.00% | 100.00% | 100.00% |
| Taxa de falha (%) | 0.00% | 0.00% | 0.00% |
| Memória ocupada (KB) | 750 | 750 | 750 |
| Memória livre (KB) | 274 | 274 | 274 |
| Maior bloco livre (KB) | 274 | 274 | 274 |
| Blocos livres | 1 | 1 | 1 |
| Fragmentação externa (%) | 0.00% | 0.00% | 0.00% |
| Fragmentação máxima (%) | 0.00% | 0.00% | 0.00% |
| Blocos examinados | 10 | 10 | 10 |
| Utilização (%) | 73.24% | 73.24% | 73.24% |

Estado final observado: `P1=100` em 0, `P2=200` em 100, `P3=150` em 300,
`P4=300` em 450, `LIVRE=274` em 750. A memória ocupada é
`100 + 200 + 150 + 300 = 750` KB; a livre é `1024 - 750 = 274` KB.

As três estratégias escolhem os mesmos endereços porque, em cada passo, existe
um único bloco livre (o remanescente ao final) e ele é o primeiro candidato.
Os 10 blocos examinados correspondem à soma `1 + 2 + 3 + 4` das buscas dos
quatro pedidos (o First Fit para no primeiro bloco; Best e Worst examinam a
lista, que também tem um único bloco livre adequado em cada passo).

### 2.2 Cenário 2 — Fragmentação externa

Sequência: quatro alocações de 256 KB, `FREE P2`, `FREE P4`,
`ALLOCATE P5 400`, `ALLOCATE P6 200`. Memória total: 1024 KB.

Tabela capturada de `./build/memory-simulator --scenario=2 --strategy=4`:

| Métrica | First Fit | Best Fit | Worst Fit |
|---------|-----------|----------|-----------|
| Total de solicitações | 6 | 6 | 6 |
| Alocações realizadas | 5 | 5 | 5 |
| Alocações com falha | 1 | 1 | 1 |
| Taxa de sucesso (%) | 83.33% | 83.33% | 83.33% |
| Taxa de falha (%) | 16.67% | 16.67% | 16.67% |
| Memória ocupada (KB) | 712 | 712 | 712 |
| Memória livre (KB) | 312 | 312 | 312 |
| Maior bloco livre (KB) | 256 | 256 | 256 |
| Blocos livres | 2 | 2 | 2 |
| Fragmentação externa (%) | 17.95% | 17.95% | 17.95% |
| Fragmentação máxima (%) | 50.00% | 50.00% | 50.00% |
| Blocos examinados | 16 | 18 | 18 |
| Utilização (%) | 69.53% | 69.53% | 69.53% |

**O ponto central do cenário.** Depois de alocar quatro blocos de 256 KB e
liberar P2 e P4, o mapa mostra `P1=256`, `LIVRE=256`, `P3=256`, `LIVRE=256`.
Nesse instante:

- memória livre total observada: `256 + 256 = 512` KB;
- maior bloco livre observado: `256` KB;
- fragmentação externa no instante: `1 - 256/512 = 50.00%`;
- pedido de 400 KB: **falhou** (`Sem espaco contiguo suficiente para 400 KB`);
- blocos examinados na falha: `4` (First Fit), `4` (Best Fit) e `4` (Worst Fit),
  porque a busca percorreu a lista inteira sem achar bloco adequado.

O traçado detalhado do binário imprime esse instante explicitamente. Após as
duas liberações, a linha de estado é:

```text
Estado: livre=512 KB | maior bloco=256 KB | fragmentacao=50.00% | blocos livres=2
```

A mesma linha aparece após a tentativa de `ALLOCATE P5 400`, que falha: o estado
da memória não muda. Isso torna **visível** o instante crítico, em vez de deixar
o valor apenas como conta derivada.

Depois da falha, `ALLOCATE P6 200` é atendido no bloco livre de 256 KB em 256,
deixando um remanescente de 56 KB. O estado final tem
`P1=256`, `P6=200`, `LIVRE=56`, `P3=256`, `LIVRE=256`; portanto:

- memória livre final: `56 + 256 = 312` KB;
- maior bloco livre final: `256` KB;
- fragmentação final impressa: `17.95%` (`1 - 256/312`);
- fragmentação máxima da execução: **50.00%**;
- blocos examinados: `16` (First Fit) e `18` (Best e Worst Fit).

**A lição do pico.** O snapshot final do cenário mostra `17.95%`, mas o pior
instante da execução foi `50.00%`. O pedido de 200 KB consumiu parte de um bloco
livre depois da falha; com isso, o total livre caiu de 512 para 312 KB e o
indicador final ficou menor, embora a memória não tenha ficado “melhor” para
pedidos grandes. A **fragmentação máxima** é o indicador que expõe o instante da
falha; a fragmentação final, sozinha, não o expõe.

**Leitura dos números.** O total livre (512 KB) era suficiente para o pedido de
400 KB, mas a maior região contígua (256 KB) não era. Esse é o fenômeno de
fragmentação externa. A diferença de blocos examinados ocorre porque Best e
Worst Fit precisam percorrer a lista inteira para comparar candidatos, enquanto
First Fit para no primeiro bloco adequado do pedido de 200 KB.

### 2.3 Cenário 3 — Critérios de escolha de bloco

Sequência:

```text
ALLOCATE P1 300
ALLOCATE P2 100
ALLOCATE P3 200
ALLOCATE P4 100
ALLOCATE P5 300
FREE P1
FREE P3
FREE P5
ALLOCATE P6 180
ALLOCATE P7 250
```

Memória total: 1024 KB.

Tabela capturada de `./build/memory-simulator --scenario=3 --strategy=4`:

| Métrica | First Fit | Best Fit | Worst Fit |
|---------|-----------|----------|-----------|
| Total de solicitações | 7 | 7 | 7 |
| Alocações realizadas | 7 | 7 | 7 |
| Alocações com falha | 0 | 0 | 0 |
| Taxa de sucesso (%) | 100.00% | 100.00% | 100.00% |
| Taxa de falha (%) | 0.00% | 0.00% | 0.00% |
| Memória ocupada (KB) | 630 | 630 | 630 |
| Memória livre (KB) | 394 | 394 | 394 |
| Maior bloco livre (KB) | 200 | 324 | 200 |
| Blocos livres | 3 | 3 | 3 |
| Fragmentação externa (%) | 49.24% | 17.77% | 49.24% |
| Fragmentação máxima (%) | 60.68% | 60.68% | 60.68% |
| Blocos examinados | 22 | 26 | 26 |
| Utilização (%) | 61.52% | 61.52% | 61.52% |

Após as liberações de P1, P3 e P5, o mapa detalhado (capturado com
`--scenario=3 --strategy=1`) mostra três blocos livres:

- `LIVRE=300` em 0;
- `LIVRE=200` em 400;
- `LIVRE=324` em 700.

O bloco de 324 KB em 700 resulta da liberação de P5 (300 KB) mais o
remanescente livre de 24 KB que já existia ao final da memória.

Tabela de endereços escolhidos, capturada de
`./build/memory-simulator --scenario=3 --strategy=4`:

| # | Operação | First Fit | Best Fit | Worst Fit |
|---|----------|-----------|----------|-----------|
| 1 | ALLOCATE P1 300 KB | 0 | 0 | 0 |
| 2 | ALLOCATE P2 100 KB | 300 | 300 | 300 |
| 3 | ALLOCATE P3 200 KB | 400 | 400 | 400 |
| 4 | ALLOCATE P4 100 KB | 600 | 600 | 600 |
| 5 | ALLOCATE P5 300 KB | 700 | 700 | 700 |
| 6 | FREE P1 | 0 | 0 | 0 |
| 7 | FREE P3 | 400 | 400 | 400 |
| 8 | FREE P5 | 700 | 700 | 700 |
| 9 | **ALLOCATE P6 180 KB** | **0** | **400** | **700** |
| 10 | **ALLOCATE P7 250 KB** | **700** | **0** | **0** |

**Destaques.** No pedido de 180 KB (operação 9), First Fit escolhe o bloco de
300 KB em 0, Best Fit escolhe o menor bloco adequado de 200 KB em 400 e Worst
Fit escolhe o maior bloco de 324 KB em 700. No pedido de 250 KB (operação 10),
os três estados já divergiram: First Fit usa o bloco de 324 KB em 700; Best Fit
e Worst Fit usam o bloco de 300 KB em 0.

O estado final é diferente nas três estratégias. Em First Fit e Worst Fit, o
maior bloco livre é 200 KB e a fragmentação é 49.24%; em Best Fit, o maior
bloco livre é 324 KB e a fragmentação é 17.77%. Os blocos examinados são 22 no
First Fit e 26 nas outras duas, coerente com a parada antecipada do First Fit
em algumas buscas.

A **fragmentação máxima** é `60.68%` nas três colunas. Isso é possível mesmo com
estados finais diferentes: o pico é observado após cada operação e as operações
1 a 8 são comuns às três estratégias, de modo que o pico ocorre nesse trecho
compartilhado, antes de as escolhas divergirem. Ele não substitui o valor final;
complementa a leitura mostrando o pior momento da execução.

### 2.4 Cenário 4 — Estresse pseudoaleatório

Comando:

```sh
./build/memory-simulator --scenario=4 --strategy=4 \
  --stress-mem=1024 --stress-procs=8 --stress-min=50 --stress-max=300 \
  --stress-ops=200 --stress-seed=42
```

Tabela capturada:

| Métrica | First Fit | Best Fit | Worst Fit |
|---------|-----------|----------|-----------|
| Total de solicitações | 103 | 103 | 103 |
| Alocações realizadas | 68 | 74 | 71 |
| Alocações com falha | 35 | 29 | 32 |
| Taxa de sucesso (%) | 66.02% | 71.84% | 68.93% |
| Taxa de falha (%) | 33.98% | 28.16% | 31.07% |
| Memória ocupada (KB) | 654 | 654 | 654 |
| Memória livre (KB) | 370 | 370 | 370 |
| Maior bloco livre (KB) | 140 | 159 | 205 |
| Blocos livres | 3 | 3 | 3 |
| Fragmentação externa (%) | 62.16% | 57.03% | 44.59% |
| Fragmentação máxima (%) | 63.57% | 59.68% | 67.32% |
| Blocos examinados | 460 | 563 | 550 |
| Utilização (%) | 63.87% | 63.87% | 63.87% |

**Leitura dos números.** A sequência tem 200 operações, mas apenas 103 são
pedidos de alocação; as outras 97 são liberações. Por isso “Total de
solicitações” é 103, e não 200. Nesta execução, as estratégias **não** têm o
mesmo desempenho:

- First Fit atende 68 pedidos e falha 35 (66.02% / 33.98%);
- Best Fit atende 74 e falha 29 (71.84% / 28.16%);
- Worst Fit atende 71 e falha 32 (68.93% / 31.07%).

As falhas são reais e diferentes entre as estratégias: a mesma sequência de
pedidos é aplicada às três, mas a geometria resultante de cada critério de
escolha pode ou não comportar um pedido. A memória ocupada final coincide
(654 KB) e a livre também (370 KB) nesta execução; isso é um resultado da
sequência, não uma propriedade garantida. A utilização final é `63.87%` nas três.
Como o gerador é cego, um `ALLOCATE` pode falhar em uma estratégia e o `FREE`
correspondente, mais tarde, não encontrar o processo **naquela** estratégia;
essa falha de `FREE` aparece no histórico, mas não entra nas taxas (a convenção
está em [`METRICAS.md`](METRICAS.md)).

A geometria final também difere: maior bloco livre de 140, 159 e 205 KB; três
blocos livres em todas; fragmentação final de 62.16%, 57.03% e 44.59%. A
**fragmentação máxima** foi 63.57%, 59.68% e 67.32% — em todas as colunas o pico
é maior que o valor final, o que reforça que o snapshot final não conta a
história inteira.

O First Fit examinou 460 blocos; Best e Worst Fit examinaram 563 e 550. A
diferença é esperada no modelo: First Fit pode parar cedo, enquanto Best e Worst
precisam varrer a lista inteira para comparar todos os candidatos. Como o
gerador é cego, algumas buscas do First Fit terminam cedo e outras não; o total é
um proxy comparativo, não uma medida de tempo.

#### 2.4.1 Correção metodológica do cenário de estresse

A versão anterior do gerador **não** era cega em relação às estratégias. Ela
mantinha três memórias de sondagem (uma por estratégia) e limitava o tamanho de
cada pedido ao menor dos maiores blocos livres das três. Com isso, todo
`ALLOCATE` era atendido nas três simulações **por construção**, e os `FREE`
sempre encontravam o processo. O resultado era uma comparação estéril:

| Aspecto | Gerador anterior (circular) | Gerador atual (cego) |
|---------|-----------------------------|----------------------|
| Total de solicitações | 104 | 103 |
| Alocações realizadas | 104 / 104 / 104 | 68 / 74 / 71 |
| Alocações com falha | 0 / 0 / 0 | 35 / 29 / 32 |
| Taxa de sucesso | 100.00% nas três | 66.02% / 71.84% / 68.93% |
| Utilização | 77.83% nas três | 63.87% nas três |
| Memória ocupada / livre | 797 / 227 KB | 654 / 370 KB |
| Maior bloco livre | 81 / 75 / 91 KB | 140 / 159 / 205 KB |
| Blocos livres | 6 / 5 / 4 | 3 / 3 / 3 |
| Fragmentação final | 64.32% / 66.96% / 59.91% | 62.16% / 57.03% / 44.59% |
| Fragmentação máxima | (indicador ainda não existia) | 63.57% / 59.68% / 67.32% |
| Blocos examinados | 495 / 801 / 811 | 460 / 563 / 550 |

Com o gerador anterior, taxa de sucesso, taxa de falha e utilização eram
**idênticas** nas três colunas: o gerador garantia que todos os pedidos
coubessem, então essas métricas não podiam diferenciar as estratégias. Restava
comparar apenas a geometria final e o custo de busca. Pior: a carga era
**derivada do comportamento dos algoritmos testados** — o tamanho de cada
pedido dependia dos maiores blocos livres das três memórias de sondagem. Isso é
circular: o experimento usava o resultado dos algoritmos para construir a
entrada que deveria testá-los.

O gerador atual sorteia os tamanhos **livremente** em `[min_size, max_size]`,
sem consultar nenhuma memória simulada. A sequência é gerada uma única vez e
guardada no cenário; as três estratégias a reaplicam em memórias novas. A
comparação justa é preservada (mesma sequência), mas agora o desfecho de cada
pedido pode diferir. Por isso as taxas de sucesso e falha finalmente diferenciam
os algoritmos, e a carga é cientificamente mais válida: ela **não** é derivada
do comportamento dos algoritmos testados.

A mudança também alterou o número de solicitações (de 104 para 103) porque a
sequência gerada mudou. O determinismo foi preservado: a mesma semente produz
exatamente a mesma sequência, e sementes diferentes produzem sequências
diferentes (verificado em `tests/test_simulation.c`, teste
`test_estresse_deterministico`).

### 2.5 O que os números desta execução mostram

- Os três cenários controlados fazem o que pretendem: o cenário 1 empaca
  sequencialmente; o cenário 2 produz uma falha por falta de contiguidade com
  memória livre total suficiente; o cenário 3 faz as três estratégias
  escolherem blocos diferentes e produz estados finais distintos.
- A fragmentação externa é uma propriedade da **geometria** da memória livre,
  não do total livre. O cenário 2 é a demonstração mais direta.
- First Fit examinou menos blocos nos cenários 2, 3 e 4 desta execução, o que é
  consistente com a parada antecipada. Isso não é uma garantia para toda carga.
- A fragmentação final é um snapshot: no cenário 2, o valor final (17.95%) é
  menor que o pico (50.00%), e no cenário 4 o pico supera o valor final nas três
  estratégias. A “Fragmentação máxima” é o indicador que preserva o instante
  crítico.
- No cenário 4, a carga cega produz falhas reais e diferentes entre as
  estratégias: Best Fit atendeu mais pedidos (74), seguido de Worst Fit (71) e
  First Fit (68). Worst Fit teve a menor fragmentação final (44.59%), mas o
  maior pico (67.32%); Best Fit teve o menor pico (59.68%). Não há vencedor
  universal.
- A escolha de bloco altera a geometria futura: no cenário 3, a escolha do
  pedido de 180 KB muda o estado disponível para o pedido de 250 KB.

### 2.6 O que os números NÃO permitem generalizar

- **Não** permitem concluir que Best Fit é sempre pior ou sempre melhor. Nesta
  execução ele teve a menor fragmentação final no cenário 3, mas não teve a
  menor fragmentação final nem o melhor pico no cenário 4.
- **Não** permitem concluir que First Fit tem sempre o menor custo total. O
  contador mede a busca da política; há travessias auxiliares O(n) não
  contabilizadas.
- **Não** permitem estimar o desempenho de um kernel real: não há páginas,
  swap, concorrência, caches, TLB, metadados do SO nem compactação.
- **Não** permitem afirmar que 100% de sucesso é uma propriedade do algoritmo:
  é uma propriedade de uma sequência com uma memória. No cenário 4 atual, por
  exemplo, as taxas de sucesso foram 66.02%, 71.84% e 68.93%.
- **Não** permitem tratar 17.95%, 49.24% ou 50.00% como “notas” universais de
  fragmentação. O indicador é operacional e depende do instante da medição.
- **Não** permitem afirmar que a fragmentação final é o pior momento: no
  cenário 2 o pico é 50.00% e no cenário 4 o pico supera o valor final nas três
  estratégias.
- **Não** há repetição estatística com várias sementes neste documento. O
  cenário 4 usa uma semente fixa (42) para ser reproduzível; uma conclusão
  estatística exigiria um estudo com muitas sementes, que não foi feito.

---

## 3. INTERPRETAÇÃO TEÓRICA (hipóteses da literatura)

Esta seção não apresenta medições novas. Ela conecta os resultados acima ao que
a literatura afirma.

### 3.1 Por que o cenário 2 é fragmentação externa

Silberschatz, Galvin e Gagne (2018, seção 9.2) definem fragmentação externa
como a situação em que há espaço livre total suficiente, mas não contíguo. O
cenário 2 é exatamente isso: 512 KB livres em dois blocos de 256 KB e um pedido
de 400 KB. A falha não é falta de memória; é falta de **contiguidade**.

Bryant e O'Hallaron (2016, seção 9.9) descrevem o mesmo fenômeno no contexto de
alocadores de heap: a memória livre pode estar fragmentada em blocos menores
que o pedido.

### 3.2 Por que Best Fit pode não ser o melhor

Tanenbaum e Bos (2014, seção 3.2.3) observam que Best Fit “tends to fill up
memory with tiny, useless holes”. Wilson et al. (1995) discutem remanescentes
pequenos (“splinters”) e o pior caso de Best Fit. O cenário 3 mostra Best Fit
com a menor fragmentação final nesta execução, mas isso não contradiz a
literatura: o resultado depende da sequência. O nome “Best” não é uma garantia.

### 3.3 Por que Worst Fit também não é garantia

Tanenbaum e Bos (2014, seção 3.2.3) afirmam que Worst Fit “is not a very good
idea either”. A ideia de “preservar blocos grandes” não se sustenta
automaticamente: Worst Fit escolhe o maior bloco disponível para a alocação
atual, consumindo-o. No cenário 3, Worst Fit escolheu 700 KB para o pedido de
180 KB e depois ficou com um bloco de 200 KB como maior livre, o mesmo que
First Fit; no cenário 4, terminou com o maior bloco livre (205 KB) e a menor
fragmentação final (44.59%), mas com o maior pico de fragmentação (67.32%).
Isso é um resultado pontual, não uma garantia.

### 3.4 Por que First Fit pode ter menor custo de busca

Knuth (1997, seção 2.5) apresenta o first-fit como Algoritmo A e discute sua
busca sequencial. A parada antecipada é a razão teórica para o menor número de
blocos examinados: assim que um bloco adequado é encontrado, a busca termina.
Os cenários 2, 3 e 4 desta execução exibem esse efeito (16 contra 18; 22 contra
26; 460 contra 563/550). Isso **não** significa que First Fit seja O(1) no pior
caso; significa apenas que ele pode parar cedo.

### 3.5 Por que a coalescência importa

Bryant e O'Hallaron (2016, pp. 886–889) explicam a coalescência de blocos
livres e o uso de boundary tags para torná-la eficiente. Knuth (1997, seção
2.5) descreve o Algoritmo C com boundary tags para liberação em tempo
praticamente constante. Neste projeto, a coalescência é imediata e mantém a
invariante “sem blocos livres adjacentes”; porém, como a lista é simplesmente
encadeada, encontrar o predecessor custa O(n). A coalescência reduz a
fragmentação, mas não elimina blocos livres separados por processos ocupados.

### 3.6 Por que o modelo não compacta

Silberschatz et al. (2018, seção 9.2) e Tanenbaum e Bos (2014, seção 3.2.3)
discutem compactação e seu custo. Compactar exige mover processos e pressupõe
relocação dinâmica. O simulador não faz isso: ele prefere mostrar a falha por
fragmentação a “escondê-la” com movimentação de blocos. Essa é uma decisão
didática, não uma afirmação de que compactação seja inviável em sistemas reais.

### 3.7 O que a “fifty-percent rule” não diz

Knuth (1997, seção 2.5) discute a “fifty-percent rule” sob hipóteses fortes de
um modelo probabilístico. Ela se refere ao número médio de blocos **livres** em
equilíbrio, não a “50% da memória desperdiçada”. Nenhum número deste documento
deve ser interpretado como confirmação ou refutação dessa regra; os
experimentos aqui são determinísticos e de curta duração.

## 4. Conclusão dos resultados

Dentro do escopo deste simulador, os experimentos mostram que:

1. a mesma sequência pode produzir estados finais diferentes conforme o
   critério de escolha de bloco;
2. memória livre total não garante atendimento de um pedido;
3. First Fit tende a examinar menos blocos nesta implementação, por parar cedo;
4. Best Fit e Worst Fit examinam a lista inteira e podem produzir geometrias
   melhores ou piores dependendo da carga;
5. a fragmentação externa é um indicador operacional útil para descrever a
   geometria, mas não uma medida universal de qualidade;
6. com uma carga cega em relação às estratégias, as taxas de sucesso e falha
   diferenciam os algoritmos, o que não acontecia quando a carga era construída
   a partir do estado das próprias simulações;
7. a fragmentação máxima pode ser bem maior que a fragmentação final; medir
   apenas o snapshot final subestima o instante crítico (cenário 2: 50.00% de
   pico contra 17.95% final).

Essas conclusões valem para as sequências e os parâmetros aqui registrados.
Generalizações exigiriam experimentos com muitas sementes, diferentes tamanhos
de memória, diferentes distribuições de tamanhos e, idealmente, comparação com
alocadores reais — o que está fora do escopo deste trabalho.
