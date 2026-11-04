# Roteiro de Apresentação

Sugestão de apresentação de **13 minutos** (faixa aceitável: 10 a 15 minutos),
com margem para perguntas. O roteiro assume que o simulador já foi compilado
com `make`.

## 1. Roteiro com tempos

| Tempo | Slide/seção | Ideia central |
|-------|-------------|---------------|
| 0:00–0:45 | 1. Abertura | O que é o projeto e qual pergunta ele responde. |
| 0:45–1:45 | 2. O problema | Por que gerenciar memória contígua é difícil. |
| 1:45–3:00 | 3. Alocação contígua | Região única, partições variáveis, divisão e liberação. |
| 3:00–4:15 | 4. Fragmentação | Interna x externa; o exemplo de 512 KB livres e 400 KB pedidos. |
| 4:15–5:00 | 5. First Fit | Primeiro bloco adequado; parada antecipada. |
| 5:00–5:45 | 6. Best Fit | Menor bloco adequado; buracos pequenos. |
| 5:45–6:30 | 7. Worst Fit | Maior bloco adequado; consome o maior bloco. |
| 6:30–7:30 | 8. Arquitetura | Camadas, lista encadeada, módulos e testes. |
| 7:30–10:00 | 9. Demonstração | Cenário 3 na linha de comando e cenário 2 na opção 5. |
| 10:00–11:30 | 10. Resultados | Tabelas reais dos cenários 1 a 4. |
| 11:30–12:45 | 11. Métricas e limites | O que os números significam e o que não significam. |
| 12:45–13:30 | 12. Conclusão | Três conclusões e uma limitação. |
| reserva | Perguntas | Respostas da seção 3. |

Se o tempo estiver curto, condensar os slides 5–7 em um único slide
comparativo e manter a demonstração do cenário 3. Se estiver sobrando tempo,
acrescentar o cenário 4 e a tabela de testes.

## 2. Sugestão de slides

### Slide 1 — Título e objetivo (0:00–0:45)

**Título sugerido:** “Simulador didático de gerenciamento de memória contígua:
First Fit, Best Fit e Worst Fit”.

**O que falar.** “O projeto implementa, em C11, um modelo de memória contígua
com partições variáveis e compara três critérios de escolha de bloco sobre a
mesma sequência de operações. O objetivo é tornar visível a fragmentação
externa e medir o custo de busca de cada critério.”

**O que mostrar na tela.** Nome do projeto, tecnologias (C11, GCC, Make) e a
frase “modelo didático, não um kernel real”.

### Slide 2 — O problema (0:45–1:45)

**O que falar.** “Quando vários processos pedem e liberam memória de tamanhos
diferentes, a região livre deixa de ser um bloco único. O sistema precisa
decidir onde colocar cada pedido. Essa decisão altera o estado futuro da
memória: uma escolha aparentemente boa hoje pode criar um buraco inútil
amanhã.”

**O que mostrar.** Um diagrama simples:

```text
[P1][LIVRE][P3][LIVRE][P5]
```

e a pergunta: “cabe um pedido de 400 KB?”.

### Slide 3 — Alocação contígua com partições variáveis (1:45–3:00)

**O que falar.** “A memória é uma região única. Cada processo ocupa um bloco
contíguo. O bloco ocupado tem exatamente o tamanho pedido; a sobra volta a ser
livre. Liberar um processo pode permitir coalescência com vizinhos livres.”

**O que mostrar.** A lista de blocos do modelo:

```text
[0, 300, P1, OCUPADO] -> [300, 100, LIVRE, -] -> [400, 200, P3, OCUPADO] -> ...
```

Destacar: endereços contíguos, soma dos tamanhos = total, um bloco por
processo.

### Slide 4 — Fragmentação interna e externa (3:00–4:15)

**O que falar.** “Fragmentação interna é sobra dentro da partição. Neste
modelo ela é zero, porque o bloco tem o tamanho exato. Fragmentação externa é
memória livre total suficiente, mas não contígua. No cenário 2, após liberar
P2 e P4, há 512 KB livres em dois blocos de 256 KB. O pedido de 400 KB falha.”

**O que mostrar.** O mapa do cenário 2 antes do pedido de 400 KB e a frase:
“512 KB livres, maior bloco 256 KB, pedido 400 KB → falha”.

### Slide 5 — First Fit (4:15–5:00)

**O que falar.** “Percorre a lista em ordem de endereço e escolhe o primeiro
bloco livre com tamanho suficiente. Pode parar cedo. No pior caso é O(n); no
melhor caso, a busca é O(1). A desvantagem é que pode fragmentar o início da
memória.”

**O que mostrar.** Um exemplo com três blocos livres e a seta parando no
primeiro adequado. Se possível, o valor “blocos examinados” da tabela do
cenário 3 (22 contra 26).

### Slide 6 — Best Fit (5:00–5:45)

**O que falar.** “Percorre todos os blocos e escolhe o menor bloco adequado.
Minimiza o remanescente imediato, mas tende a criar buracos pequenos e
inúteis. É Theta(n) na lista simples. O nome ‘Best’ não é uma garantia.”

**O que mostrar.** O bloco de 200 KB escolhido em 400 no cenário 3, enquanto
First Fit escolheu 300 em 0.

### Slide 7 — Worst Fit (5:45–6:30)

**O que falar.** “Percorre todos os blocos e escolhe o maior bloco adequado.
Tenta deixar o maior remanescente, mas consome o maior bloco disponível. Também
é Theta(n). A literatura não recomenda tratá-lo como solução geral.”

**O que mostrar.** O bloco de 700 escolhido para o pedido de 180 no cenário 3 e
a consequência para o pedido seguinte.

### Slide 8 — Arquitetura do simulador (6:30–7:30)

**O que falar.** “O projeto é dividido em camadas: memory, allocation, metrics,
simulation, scenarios, visualization e main. A memória é uma lista encadeada de
blocos. As três estratégias compartilham a mesma primitiva de divisão. Os
testes cobrem as invariantes: contiguidade, soma dos tamanhos, um bloco por
processo e coalescência.”

**O que mostrar.** O diagrama de camadas de `docs/ARQUITETURA.md` e o número
“1062 verificações, 0 falhas”.

### Slide 9 — Demonstração ao vivo (7:30–10:00)

**O que falar.** “Vou mostrar dois recortes: o cenário 3, que evidencia a
diferença de critério, e o cenário 2 no modo personalizado, que evidencia a
fragmentação externa.”

**Comando 1 (cenário 3, tabela de endereços):**

```sh
./build/memory-simulator --scenario=3 --strategy=4
```

Mostrar a linha 9 (`ALLOCATE P6 180 KB`) com 0, 400 e 700, e a linha 10
(`ALLOCATE P7 250 KB`) com 700, 0 e 0.

**Comando 2 (modo interativo, opção 5):**

```sh
./build/memory-simulator
```

Na tela:

```text
5
1
ALLOCATE 1 256
ALLOCATE 2 256
ALLOCATE 3 256
ALLOCATE 4 256
FREE 2
FREE 4
ALLOCATE 5 400
ALLOCATE 6 200
MAP
METRICS
QUIT
```

Esse é o conteúdo de `examples/scenario_02.txt` (sem as linhas de comentário,
que o programa ignora). Mostrar a falha do pedido de 400 KB e o mapa com dois
blocos livres de 256 KB.

**Alternativa sem digitação:** usar a linha de comando do cenário 2:

```sh
./build/memory-simulator --scenario=2 --strategy=1
```

Mostra o passo a passo, a falha e o mapa após cada operação.

**Comando 3 (opcional, se houver tempo):**

```sh
./build/memory-simulator --scenario=4 --strategy=4 \
  --stress-mem=1024 --stress-procs=8 --stress-min=50 --stress-max=300 \
  --stress-ops=200 --stress-seed=42
```

Mostrar a reprodutibilidade: o mesmo comando produz a mesma tabela.

### Slide 10 — Comparação de resultados (10:00–11:30)

**O que falar.** “No cenário 1, as três estratégias empatam porque há um único
bloco livre em cada passo. No cenário 2, todas falham o pedido de 400 KB e o
pico de fragmentação é 50%, embora o valor final seja 17,95%. No cenário 3, os
endereços e os estados finais divergem. No cenário 4, a carga cega produz
falhas diferentes: 68, 74 e 71 alocações atendidas para First, Best e Worst Fit,
respectivamente, com taxas de sucesso de 66,02%, 71,84% e 68,93%.”

**O que mostrar.** Tabela resumida:

| Cenário | First Fit | Best Fit | Worst Fit |
|---------|-----------|----------|-----------|
| 1: fragmentação | 0.00% | 0.00% | 0.00% |
| 2: taxa de sucesso | 83.33% | 83.33% | 83.33% |
| 3: fragmentação | 49.24% | 17.77% | 49.24% |
| 3: blocos examinados | 22 | 26 | 26 |
| 4: taxa de sucesso | 66.02% | 71.84% | 68.93% |
| 4: fragmentação final | 62.16% | 57.03% | 44.59% |
| 4: fragmentação máxima | 63.57% | 59.68% | 67.32% |
| 4: blocos examinados | 460 | 563 | 550 |

**Cuidado ao falar.** “Esses números são desta execução; não são uma
classificação universal.”

### Slide 11 — Métricas e limites (11:30–12:45)

**O que falar.** “A fragmentação é um indicador operacional:
`1 - maior bloco livre / memória livre total`. Ela é indefinida quando não há
memória livre. A fragmentação **final** é um snapshot; por isso o simulador
também mostra a **fragmentação máxima**, o maior valor observado após alguma
operação. ‘Total de solicitações’ conta apenas alocações; ‘blocos examinados’ é
um proxy comparativo do custo de busca, não do custo total. O modelo não tem
paginação, compactação automática nem metadados do SO.”

**O que mostrar.** A fórmula e quatro avisos:

1. memória livre total não garante alocação;
2. fragmentação 0% não significa memória vazia;
3. Best Fit não é “o melhor”;
4. o pior instante pode não ser o estado final (cenário 2: pico 50,00%, final
   17,95%).

### Slide 12 — Conclusão (12:45–13:30)

**O que falar.** “O simulador mostra que a escolha de bloco altera a geometria
futura da memória; que fragmentação externa depende da contiguidade, não do
total livre; e que First Fit tende a examinar menos blocos nesta implementação,
mas não há vencedor universal. A principal limitação é ser um modelo didático
de uma região única, sem paginação nem compactação.”

**O que mostrar.** Três bullets e uma frase de limitação.

### Slide 13 — Perguntas (reserva)

**O que mostrar.** A lista de perguntas prováveis da seção 3, ou apenas o
convite para perguntas.

## 3. Perguntas prováveis do professor (com respostas didáticas)

### 3.1 Por que Best Fit não é necessariamente o melhor algoritmo?

Porque ele minimiza apenas o **remanescente imediato**. Ao escolher sempre o
menor bloco adequado, pode deixar sobras minúsculas que nunca mais serão
usadas; com o tempo, isso pode aumentar a fragmentação externa. Tanenbaum e
Bos (2014, seção 3.2.3) dizem que Best Fit tende a encher a memória com
“buracos pequenos e inúteis”. Além disso, ele examina a lista inteira. No
cenário 3 desta execução, Best Fit teve a menor fragmentação final; no cenário
4, não teve a menor fragmentação final nem o melhor pico. Isso ilustra que o
resultado depende da carga.

### 3.2 Por que First Fit pode apresentar menor custo de busca?

Porque ele pode **parar no primeiro bloco adequado**. Best Fit e Worst Fit
precisam percorrer todos os blocos para comparar tamanhos, enquanto First Fit
interrompe a varredura assim que encontra um candidato. Por isso, neste modelo,
o contador de blocos examinados do First Fit tende a ser menor. Isso não
significa que First Fit seja sempre mais rápido no computo total: a chamada de
alocação também faz verificações auxiliares O(n), e o pior caso do First Fit
continua O(n).

### 3.3 Como ocorre a fragmentação externa?

Ocorre quando a memória livre total é suficiente, mas está dividida em blocos
não contíguos, separados por processos ocupados. No cenário 2, depois de
liberar P2 e P4, restam 512 KB livres em dois blocos de 256 KB. Um pedido de
400 KB precisa de 400 KB **contíguos**; como o maior bloco é 256 KB, a
alocação falha. A fragmentação externa é, portanto, um problema de geometria,
não de quantidade total.

### 3.4 Por que a memória livre total não garante uma alocação bem-sucedida?

Porque o processo precisa de uma região **contígua**. Se houver 512 KB livres em
dois blocos de 256 KB, um pedido de 400 KB não cabe em nenhum deles. O total
livre é uma condição necessária, mas não suficiente. O maior bloco livre é a
métrica que captura essa limitação.

### 3.5 Qual é a importância da coalescência?

Ela une blocos livres adjacentes, criando regiões maiores e reduzindo a
fragmentação externa. Sem coalescência, duas liberações vizinhas continuariam
como dois blocos separados, e um pedido que caberia na soma poderia falhar.
Neste projeto, a coalescência é imediata: após `memory_free`, não existem dois
blocos livres adjacentes. Mas ela não resolve todos os casos: se um processo
ocupado separa dois blocos livres, eles continuam separados. Além disso, sem
boundary tags, localizar o predecessor custa O(n).

### 3.6 Por que todos os algoritmos devem receber a mesma sequência de operações?

Para que as diferenças observadas possam ser atribuídas ao **critério de
escolha**, e não a sequências diferentes. Se cada algoritmo recebesse uma
sequência distinta, não haveria como comparar taxas, fragmentação ou endereços.
O projeto executa a mesma lista em três memórias independentes e reiniciadas;
a igualdade de “Total de solicitações” nas três colunas é uma evidência de que
a comparação é justa.

### 3.7 Qual é a diferença entre alocação contígua e paginação?

Na alocação contígua, cada processo ocupa uma única região consecutiva. Na
paginação, a memória é dividida em páginas e molduras de tamanho fixo, e as
páginas de um processo podem ficar espalhadas; isso elimina a fragmentação
externa clássica, ao custo de fragmentação interna na última página e de
estruturas de tradução. O simulador implementa apenas alocação contígua; não há
páginas, molduras, tabela de páginas nem TLB.

### 3.8 Por que o modelo não compacta automaticamente?

Porque compactar exige mover processos e pressupõe relocação dinâmica; tem
custo e nem sempre é possível. Além disso, a compactação automática esconderia
o fenômeno que o projeto quer mostrar: a falha por fragmentação externa. O
cenário 2 só é didático porque o pedido de 400 KB falha. Em um sistema real, a
compactação pode ser uma opção, mas não é gratuita (Silberschatz et al., 2018,
seção 9.2; Tanenbaum e Bos, 2014, seção 3.2.3).

### 3.9 Como você sabe que o código está correto?

Porque há testes automatizados para as invariantes e para os cenários, e eles
passam com e sem sanitizers. `make test` executa **1062 verificações, com 0
falhas**; `make asan` recompila com AddressSanitizer e UndefinedBehaviorSanitizer
e também passa. Os testes verificam contiguidade de endereços, soma dos
tamanhos, um bloco por processo, ausência de blocos livres adjacentes (inclusive
**qualquer par** adjacente, rejeitado por `memory_validate_ex`), estabilidade em
falhas, o caso indefinido da fragmentação, o pico de fragmentação e o
determinismo do cenário de estresse. Nenhum teste prova “correção absoluta”, mas
cobre as invariantes declaradas do modelo.

### 3.10 Qual é a diferença entre fragmentação interna e externa?

Fragmentação interna é o desperdício **dentro** de uma partição alocada maior
que o pedido; o espaço sobra dentro do bloco e não pode ser usado por outro
processo. Fragmentação externa é o espaço livre **entre** partições: há memória
livre total suficiente, mas não contígua. Neste modelo, a interna é zero por
construção (bloco do tamanho exato) e a externa é o fenômeno do cenário 2.

### 3.11 O que aconteceria se você usasse `malloc` em vez do seu modelo?

`malloc` é um alocador de heap real, com políticas próprias, metadados,
alinhamento e possivelmente boundary tags; ele não expõe de forma direta a
lista de blocos nem permite comparar First/Best/Worst Fit com a mesma
sequência. Além disso, o comportamento de `malloc` varia entre implementações e
plataformas, o que prejudicaria a reprodutibilidade. O modelo explícito existe
para tornar visíveis as decisões e as métricas; `malloc` seria uma caixa-preta
para o objetivo didático.

### 3.12 Por que usar lista encadeada e não um array de tamanho fixo?

Porque o número de partições varia e blocos são inseridos e removidos no meio
da lista (divisão e coalescência). Um array exigiria capacidade máxima e
deslocamento de elementos; uma árvore adicionaria complexidade que não é o foco
didático. A lista permite representar a ordem de endereço naturalmente e
torna o custo O(n) da busca explícito e observável.

### 3.13 O que significa “fragmentação externa 17.95%”? Isso quer dizer que 17.95% da memória está desperdiçada?

Não. O indicador é `1 - maior bloco livre / memória livre total`. No estado
final do cenário 2, há 312 KB livres e o maior bloco tem 256 KB; logo,
`1 - 256/312 = 17.95%`. Ele descreve a dispersão da memória livre, não a fração
de memória total desperdiçada. Além disso, é um indicador operacional, não uma
medida universal.

### 3.14 Por que o cenário 4 tem 200 operações e apenas 103 solicitações?

Porque “Total de solicitações” conta apenas pedidos de **alocação**. Das 200
operações da sequência, 103 são `ALLOCATE` e 97 são `FREE`. Falhas de `FREE`
também não entram nas taxas. Essa convenção está documentada em
`docs/METRICAS.md`.

### 3.15 Por que os blocos examinados do First Fit são menores?

Porque ele pode parar no primeiro bloco adequado. Best Fit e Worst Fit precisam
examinar todos os blocos para comparar tamanhos. No cenário 4, First Fit
examinou 460 blocos, contra 563 e 550. Isso é consistente com a parada
antecipada, mas não é uma garantia para toda carga; e o contador não inclui as
travessias auxiliares da chamada. Ele é um proxy **comparativo** do custo de
busca, não o tempo total da operação.

### 3.16 Este projeto reproduz um kernel real? Quais são as simplificações?

Não. O modelo tem uma única região contígua, processos são apenas
identificadores, não há paginação, segmentação, swap, threads, prioridades,
tabelas de páginas, TLB, compactação automática nem alocação de metadados do
próprio SO. Endereços são offsets em KB de uma região fictícia. Modelos reais
usam paginação/segmentação, free lists, boundary tags, buddy system e slab
allocator, entre outras técnicas.

### 3.17 O que é a “fifty-percent rule” e por que você não pode dizer que ela significa “50% de desperdício”?

É uma afirmação de Knuth (1997, seção 2.5) sobre o número médio de blocos
**livres** em equilíbrio, sob hipóteses fortes de um modelo probabilístico. Ela
não afirma que 50% da memória é desperdiçada, e não se aplica automaticamente a
qualquer carga. Os experimentos deste projeto são determinísticos e curtos;
não testam essa regra.

### 3.18 Por que usar xorshift32 em vez de `rand()`?

Porque `rand()` não garante a mesma sequência entre plataformas e
implementações da biblioteca C. O xorshift32 é pequeno, tem estado local e
produz a mesma sequência para a mesma semente em qualquer plataforma com
inteiros de 32 bits. Isso torna o cenário 4 reproduzível.

### 3.19 Por que os endereços do cenário 3 são 0, 400 e 700?

Porque, após as liberações, os blocos livres têm 300 KB em 0, 200 KB em 400 e
324 KB em 700. O pedido de 180 KB: First Fit pega o primeiro adequado (300 em
0), Best Fit pega o menor adequado (200 em 400) e Worst Fit pega o maior (324
em 700). No pedido seguinte de 250 KB, os estados já divergiram: First Fit usa
700; Best Fit e Worst Fit usam 0.

### 3.20 O que aconteceria se a memória tivesse outro tamanho ou outra semente?

Os resultados mudariam, porque a geometria e a sequência mudam. O simulador
permite variar `--stress-mem`, `--stress-procs`, `--stress-min`,
`--stress-max`, `--stress-ops` e `--stress-seed`; o cenário personalizado aceita
`--custom-mem=KB`. Uma conclusão responsável exigiria repetir o experimento com
várias sementes e tamanhos; este trabalho documenta uma execução reproduzível,
não um estudo estatístico.

### 3.21 Por que o cenário de estresse tem falhas diferentes entre as estratégias?

Porque a sequência de pedidos é a mesma, mas a **geometria da memória** resultante
de cada critério de escolha é diferente. Um pedido pode caber em um bloco
contíguo deixado por Best Fit e não caber no estado produzido por First Fit. Com
o gerador atual — **cego** em relação às estratégias — os tamanhos são sorteados
livremente em `[50, 300]` KB; a carga não é limitada ao que os algoritmos
conseguem atender. Por isso as taxas de sucesso diferem (66,02%, 71,84% e
68,93%) e as falhas são reais. Na versão anterior, o gerador limitava cada
pedido ao menor dos maiores blocos livres das três memórias de sondagem, o que
garantia 100% de sucesso por construção e não permitia diferenciar as
estratégias.

### 3.22 O que é fragmentação máxima e por que ela importa?

É o **maior** valor de fragmentação externa observado após alguma operação da
simulação, e não o valor do estado final. A fragmentação final é um snapshot:
depois do pior momento, uma alocação pode consumir parte da memória livre e
reduzir o indicador, escondendo a pior geometria. No cenário 2, o pico é 50,00%
(512 KB livres em dois blocos de 256 KB, pedido de 400 KB falhando) e o valor
final é 17,95%. Sem o pico, o snapshot final não revelaria a falha. O binário
também imprime uma linha de estado após cada operação, tornando o instante
visível.

### 3.23 Seus testes verificam que a coalescência funciona?

Sim. `tests/test_coalescing.c` cobre a fusão com o vizinho seguinte, com o
anterior, com dois ou três blocos e nas extremidades. Além disso,
`memory_validate_ex` rejeita **qualquer par de blocos livres adjacentes**, com a
mensagem `dois blocos livres adjacentes em %d KB`; como a coalescência é
imediata, um par adjacente indicaria coalescência quebrada. O caso é testado em
`tests/test_memory.c:test_validate_rejects_adjacent_free_blocks`.

### 3.24 Por que o mapa de memória agrega blocos quando há muitos?

Porque a largura do terminal é limitada e desenhar um caractere por bloco
pequeno tornaria o mapa ilegível. O mapa usa uma representação proporcional e,
quando o número de blocos é grande demais, agrega-os e informa
`Mapa simplificado: N blocos agregados`. A agregação é só visual: os valores
numéricos de endereço, tamanho e métrica continuam vindo das estruturas reais. O
mapa também não usa vetores automáticos na pilha (VLAs): os vetores auxiliares
vão para o heap com checagem de falha.

## 4. Checklist de defesa

O estudante deve saber explicar **de cor**, sem consultar o código:

### 4.1 Estrutura de dados

- [ ] O que é `MemoryBlock` e quais são seus campos.
- [ ] O que é `Memory` e o que significam `head`, `total_size` e os três
      contadores.
- [ ] Por que a lista está em ordem crescente de endereço.
- [ ] Por que não há dois blocos livres adjacentes após `FREE`.
- [ ] O que é `NO_PROCESS` e por que ele existe.

### 4.2 Funções

- [ ] O que `memory_allocate_in_block` faz e por que ela é a divisão da
      partição variável.
- [ ] O que `memory_free` faz, incluindo a ordem da coalescência (sucessor,
      depois predecessor).
- [ ] O que `allocation_execute` faz e onde conta `blocks_examined`.
- [ ] O que `metrics_compute` calcula e por que `total_requests` não inclui
      `FREE`.
- [ ] O que `simulation_execute` registra no histórico e por que reserva
      espaço antes de mutar a memória.
- [ ] O que `scenarios_append_stress` faz: gera uma sequência cega em relação às
      estratégias, determinística por semente, e a reaplica às três.

### 4.3 Métricas

- [ ] Fórmula da fragmentação e o caso indefinido.
- [ ] Diferença entre memória livre total e maior bloco livre.
- [ ] O que é “fragmentação máxima” e por que o snapshot final não basta.
- [ ] O que é “blocos examinados” e por que é um proxy comparativo de busca.
- [ ] Por que taxa de sucesso e taxa de falha somam 100% quando há
      solicitações.
- [ ] Por que utilização não mede fragmentação.

### 4.4 Limitações

- [ ] O modelo não é um kernel.
- [ ] Não há paginação, swap, threads, prioridades nem tabela de páginas.
- [ ] Não há compactação automática.
- [ ] A fragmentação interna é zero por construção.
- [ ] Endereços são offsets em KB de uma região fictícia.
- [ ] Os resultados valem para as sequências testadas, não para toda carga.

### 4.5 Experimentos

- [ ] Comando do cenário 2 e por que o pedido de 400 KB falha.
- [ ] Endereços 0/400/700 e 700/0/0 do cenário 3.
- [ ] Comando do cenário 4 com semente 42 e por que ele é reproduzível.
- [ ] Números principais: 1062 verificações, 0 falhas; 103 solicitações no
      cenário 4; 512 KB livres e 256 KB de maior bloco no cenário 2; pico de
      50,00% no cenário 2.
- [ ] Por que as três estratégias recebem a mesma sequência.
- [ ] Por que o gerador do cenário de estresse é cego e determinístico.
- [ ] Por que a validação rejeita dois blocos livres adjacentes.
- [ ] O que o ASan/UBSan acrescenta à verificação.

### 4.6 Literatura

- [ ] Knuth: first-fit, boundary tags, fifty-percent rule.
- [ ] Tanenbaum e Bos: best-fit e os buracos pequenos; worst-fit não é boa
      ideia.
- [ ] Silberschatz et al.: fragmentação interna/externa e compactação.
- [ ] Bryant e O'Hallaron: coalescência e boundary tags.
- [ ] Wilson et al.: revisão crítica e splinters.
- [ ] OSTEP: não há uma abordagem “melhor” universal.
