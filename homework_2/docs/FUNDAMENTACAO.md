# Fundamentação Teórica

Este documento apresenta a base conceitual do simulador. O objetivo não é
reproduzir um kernel, mas construir um **modelo didático** no qual os conceitos
de alocação contígua, fragmentação, coalescência e critérios de escolha de
bloco possam ser observados e medidos. Todas as afirmações sobre o
comportamento do programa se referem ao código deste repositório; as
afirmações sobre a literatura são atribuídas às referências listadas ao final.

## 1. Alocação contígua com partições variáveis

Na alocação contígua, cada processo recebe uma **região consecutiva** de
memória. Em um esquema de **partições variáveis**, o tamanho da partição é
decidido no momento da alocação, e não fixado previamente. O modelo deste
projeto representa a memória como uma única região de `total_size` KB,
inicialmente livre, descrita por uma lista de blocos em ordem crescente de
endereço.

Cada bloco tem:

- um endereço inicial (`start_address`), em KB, medido a partir do início da
  região;
- um tamanho (`size`), em KB;
- um estado: `BLOCK_FREE` ou `BLOCK_OCCUPIED`;
- um identificador de processo (`process_id`), que vale `NO_PROCESS` quando o
  bloco está livre;
- um ponteiro para o próximo bloco (`next`).

A soma dos tamanhos dos blocos é sempre igual ao tamanho total da região, e os
endereços são contíguos a partir de zero. Essas são as invariantes centrais do
modelo, verificadas por `memory_validate_ex` em `src/memory.c`.

A alocação é feita por **divisão de um bloco livre**: se o bloco tem tamanho
maior que o pedido, a parte inicial vira a partição ocupada e o restante
continua livre; se o tamanho coincide com o pedido, o bloco inteiro é ocupado.
Essa é a primitiva `memory_allocate_in_block` e corresponde exatamente à ideia
de partição variável: não há desperdício interno porque o bloco tem o tamanho
exato solicitado.

> **Diferença importante.** Em sistemas reais, o gerenciador costuma arredondar
> o pedido para múltiplos de alinhamento ou de página, o que produz
> fragmentação interna. Aqui o bloco tem exatamente o tamanho pedido; por isso
> a fragmentação interna é, **neste modelo didático**, zero: não há cabeçalho
> de bloco nem alinhamento. Em partições variáveis reais, essa fragmentação se
> reduz ao overhead de cabeçalho/alinhamento e **não** é zero. A simplificação é
> uma escolha do modelo, não uma propriedade universal da alocação contígua.

## 2. Fragmentação interna e fragmentação externa

### 2.1 Fragmentação interna

É o desperdício **dentro** de uma partição alocada, quando a partição é maior
que o pedido do processo. O espaço sobra dentro do bloco ocupado e não pode ser
usado por outro processo.

Neste simulador, **não há fragmentação interna** nas partições, porque
`memory_allocate_in_block` cria o bloco ocupado com exatamente `size` KB. A
sobra, quando existe, é devolvida como um bloco livre separado. A fragmentação
interna é zero **neste modelo didático**, que não usa cabeçalho de bloco nem
alinhamento; em partições variáveis reais ela se reduz ao overhead e não é
zero. O único caso em que se poderia falar de “sobra” é o do arredondamento de
tamanho, que o modelo não faz.

### 2.2 Fragmentação externa

É a situação em que existe **memória livre total suficiente** para atender a um
pedido, mas não existe uma **região contígua** grande o bastante. O espaço
livre está espalhado em vários blocos pequenos, separados por blocos ocupados.

O cenário 2 do projeto é o exemplo canônico: quatro blocos de 256 KB ocupam
1024 KB; ao liberar os processos 2 e 4, restam 512 KB livres, porém em dois
blocos de 256 KB. Um pedido de 400 KB falha, embora haja 512 KB livres no
total, porque o maior bloco contíguo tem apenas 256 KB.

Formalmente, o simulador adota o indicador operacional:

```text
fragmentacao_externa = 1 - (maior bloco livre / memoria livre total)
```

Ele vale 0 quando toda a memória livre está em um único bloco e tende a 1
quando a memória livre está muito espalhada. Esse indicador é **operacional**:
ele resume a geometria da lista no instante da medição, mas não é uma medida
universal de “qualidade” de um algoritmo. A definição completa, os limites e
os casos indefinidos estão em [`METRICAS.md`](METRICAS.md).

Como o valor final é apenas um **snapshot**, o simulador também observa o
estado após cada operação e guarda a **fragmentação máxima** da execução. No
cenário 2, o pico é 50.00% no instante em que o pedido de 400 KB falha (512 KB
livres em dois blocos de 256 KB); o valor final, depois de uma alocação de
200 KB, cai para 17.95%. Medir apenas o final esconderia o instante crítico.

## 3. Coalescência

Quando um processo libera sua partição, o bloco volta ao estado livre. Se o
vizinho anterior ou o vizinho seguinte também estiver livre, os blocos podem
ser **unidos** em uma única região livre maior. Essa operação é a
**coalescência** (ou fusão) de blocos livres adjacentes.

No projeto, a coalescência é **imediata**: `memory_free` marca o bloco como
livre e, na mesma chamada, tenta fundi-lo primeiro com o sucessor e depois com
o predecessor. Ao final, não existem dois blocos livres adjacentes na lista.
Isso reduz a fragmentação externa e aumenta a chance de atender pedidos
grandes.

A coalescência **não elimina toda a fragmentação externa**. Ela só pode unir
blocos que já são adjacentes. Se um bloco ocupado separa duas regiões livres,
elas continuam separadas. Além disso, encontrar o predecessor de um bloco em
uma lista simplesmente encadeada custa O(n); a liberação em tempo praticamente
constante exige uma estrutura adicional, como os **boundary tags** descritos
por Knuth (1997, seção 2.5) e por Bryant e O'Hallaron (2016, pp. 887–889).

## 4. Compactação

Compactação é mover blocos ocupados para reunir todo o espaço livre em uma
única região contígua. Ela pode resolver fragmentação externa, mas tem custos e
requisitos:

- é preciso copiar dados de processos;
- é preciso **relocação dinâmica**: os endereços dos processos mudam;
- o custo cresce com a quantidade de memória movida;
- a operação não é gratuita nem sempre possível (por exemplo, se um processo
  não puder ser movido).

Silberschatz, Galvin e Gagne (2018, seção 9.2) observam que a compactação só é
possível com relocação dinâmica em tempo de execução. Tanenbaum e Bos (2014,
seção 3.2.3) também discutem o custo de compactar a memória.

Neste projeto, **não há compactação automática**. O modelo apenas divide,
libera e coalesce blocos. Quando um pedido não cabe em nenhum bloco livre, ele
falha, mesmo que uma compactação pudesse resolver o problema. Essa é uma
decisão didática: o objetivo é observar a fragmentação sem a “solução mágica”
da movimentação de blocos.

## 5. Os três algoritmos de escolha de bloco

As três estratégias compartilham o mesmo modelo de memória e a mesma primitiva
de divisão. O que muda é **qual bloco livre é escolhido** quando há mais de um
candidato.

Em todas elas, a lista é percorrida em ordem crescente de endereço. A diferença
está no critério de parada e no critério de comparação.

### 5.1 First Fit

1. **Problema que resolve.** Escolher rapidamente um bloco livre adequado sem
   examinar toda a memória, mantendo baixo o custo de busca.
2. **Como funciona.** Percorre a lista do início ao fim e escolhe o **primeiro**
   bloco livre cujo tamanho seja maior ou igual ao pedido.
3. **Como percorre os blocos.** Em ordem de endereço. Pode parar no primeiro
   candidato adequado; blocos posteriores não são examinados.
4. **Critério de escolha.** Primeiro bloco livre com `size >= requested_size`.
   Não compara tamanhos; não busca o menor nem o maior.
5. **Vantagens.** Simples; tende a ser rápido porque a busca pode terminar
   cedo; não precisa percorrer toda a lista quando encontra um bloco adequado
   no início; é o critério usado por Knuth como Algoritmo A (1997, seção 2.5) e
   aparece como primeira opção em Tanenbaum e Bos (2014, seção 3.2.3).
6. **Desvantagens.** Pode fragmentar o início da memória com blocos pequenos;
   a qualidade da escolha depende da ordem histórica das operações; não há
   garantia de que o bloco escolhido seja o “melhor” para o futuro.
7. **Complexidade da busca.** O(n) no pior caso, porque pode percorrer todos os
   blocos; O(1) no melhor caso, quando o primeiro bloco já é adequado. Neste
   código, a função de estratégia para no primeiro candidato, mas a chamada
   completa de alocação ainda faz verificações auxiliares O(n) (ver seção 7).
8. **Comportamento diante da fragmentação externa.** Pode criar fragmentação
   externa ao ocupar o início de blocos grandes e deixar remanescentes
   pequenos. Não tem mecanismo para evitá-la. Knuth relata experimentos em que
   First Fit foi superior a Best Fit em determinadas cargas; isso **não**
   significa superioridade universal.

### 5.2 Best Fit

1. **Problema que resolve.** Minimizar o desperdício imediato, escolhendo o
   bloco livre que deixa o menor remanescente possível.
2. **Como funciona.** Percorre todos os blocos e escolhe o **menor** bloco
   livre com tamanho suficiente. Em empate, permanece o primeiro encontrado,
   que é o de menor endereço inicial.
3. **Como percorre os blocos.** Percorre a lista inteira, porque precisa
   comparar todos os blocos livres adequados.
4. **Critério de escolha.** Menor `size` entre os blocos livres com
   `size >= requested_size`; empate pelo menor `start_address`.
5. **Vantagens.** Deixa remanescentes pequenos; em uma única alocação, o
   desperdício imediato é mínimo; pode preservar blocos grandes para pedidos
   futuros.
6. **Desvantagens.** Tende a encher a memória com buracos pequenos e inúteis
   (“tiny, useless holes”, Tanenbaum e Bos, 2014, seção 3.2.3); o exame
   completo custa mais; o remanescente minúsculo pode nunca mais ser usado
   (Wilson et al., 1995, discutem “splinters” e o pior caso de Best Fit).
7. **Complexidade da busca.** Theta(n) na lista simples: todos os blocos são
   visitados. Não é O(log n) nesta estrutura; uma árvore balanceada ou uma
   estrutura de busca ordenada poderia melhorar a consulta, mas não é o que o
   projeto implementa.
8. **Comportamento diante da fragmentação externa.** Justamente por preferir o
   menor bloco adequado, Best Fit pode produzir muitos remanescentes pequenos,
   isto é, **aumentar** a fragmentação externa ao longo do tempo. Ele não é
   “o melhor algoritmo”; o nome é histórico, não uma garantia.

### 5.3 Worst Fit

1. **Problema que resolve.** Tentar deixar o maior remanescente possível,
   preservando espaço contíguo para futuras alocações.
2. **Como funciona.** Percorre todos os blocos e escolhe o **maior** bloco
   livre com tamanho suficiente. Em empate, permanece o primeiro encontrado,
   de menor endereço.
3. **Como percorre os blocos.** Percorre a lista inteira, porque precisa
   comparar todos os blocos livres adequados.
4. **Critério de escolha.** Maior `size` entre os blocos livres com
   `size >= requested_size`; empate pelo menor `start_address`.
5. **Vantagens.** Evita, em princípio, criar remanescentes minúsculos; o bloco
   escolhido é grande, e a sobra tende a ser grande o bastante para novos
   pedidos.
6. **Desvantagens.** Consome os maiores blocos livres; pode destruir a
   capacidade de atender um pedido grande futuro; Tanenbaum e Bos (2014,
   seção 3.2.3) observam que Worst Fit “is not a very good idea either”.
7. **Complexidade da busca.** Theta(n) na lista simples: todos os blocos são
   visitados.
8. **Comportamento diante da fragmentação externa.** Pode reduzir a
   fragmentação em alguns instantes, mas não há garantia. Ao fragmentar
   repetidamente os maiores blocos, pode piorar a capacidade de atender
   pedidos grandes. “Preservar blocos grandes” não é uma propriedade
   automática de Worst Fit: ele escolhe o maior bloco **disponível**, não
   preserva um bloco grande intocado.

## 6. Alocação contígua, paginação e memória virtual

É comum confundir conceitos que pertencem a camadas diferentes:

| Conceito | O que é | Neste projeto |
|----------|---------|---------------|
| Alocação contígua | Cada processo ocupa uma região consecutiva de memória física. | É o modelo implementado. |
| Paginação | A memória é dividida em páginas e molduras de tamanho fixo; páginas de um processo podem ficar espalhadas. | Não existe. Não há páginas, molduras nem tabela de páginas. |
| Segmentação | O espaço de endereçamento é dividido em segmentos lógicos de tamanhos variados. | Não existe. A referência para segmentação é Tanenbaum e Bos (2014, seção 3.7), pois a 10ª edição de Silberschatz et al. removeu a seção de segmentação. |
| Memória virtual | Abstração que permite endereços virtuais maiores que a memória física e usa tradução de endereços. | Não existe. Os endereços do simulador são deslocamentos em uma região física fictícia. |
| Estratégia de alocação | Critério para escolher um bloco livre (First/Best/Worst Fit). | É o foco do projeto. |
| Algoritmo de substituição de páginas | Critério para escolher qual página remover da memória (FIFO, LRU, etc.). | Não se aplica: não há paginação. |

O simulador trabalha com **memória física contígua** e endereços que são
offsets em KB. Não há tradução de endereços, TLB, swap ou proteção de memória.

## 7. Complexidade e o proxy “blocos examinados”

Na lista simplesmente encadeada deste projeto:

- **First Fit**: busca O(n) no pior caso, com parada antecipada; O(1) no melhor
  caso para a busca em si.
- **Best Fit**: Theta(n), porque precisa examinar todos os blocos para
  comparar tamanhos.
- **Worst Fit**: Theta(n), pelo mesmo motivo.

O contador `blocks_examined` é o **proxy empírico** do custo de busca no modelo:
ele conta quantos blocos foram percorridos pela **busca de seleção do bloco
livre**, incluindo blocos ocupados atravessados. First Fit pode parar antes;
Best Fit e Worst Fit varrem a lista inteira. A convenção está declarada em
`src/allocation.h`: a contagem **não** inclui os percursos auxiliares de
manutenção (localização do processo, conferência de pertinência do bloco e
recálculo dos contadores), que são idênticos nas três estratégias. Portanto, é
um proxy **comparativo** do custo de busca, não o tempo total da operação.

Duas ressalvas importantes:

1. `blocks_examined` mede apenas a **busca por bloco adequado**, não todas as
   travessias da chamada de alocação. Em `src/allocation.c`,
   `allocation_execute` também chama `memory_find_process` para rejeitar
   processo já alocado e `memory_allocate_in_block` verifica se o bloco pertence
   à lista. Esses percursos são O(n) e não entram no contador. Por isso o custo
   total de uma chamada pode ser O(n) mesmo quando a busca da estratégia para no
   primeiro bloco. O contador é útil para comparar **critérios de busca**, não
   para medir o custo total do programa.
2. A complexidade assintótica não diz qual estratégia terá melhor taxa de
   sucesso em uma carga concreta. Ela diz apenas como o custo de busca cresce
   com o número de blocos.

## 8. Comparação justa

Para atribuir diferenças de desempenho ao critério de escolha, todas as
estratégias precisam receber a **mesma sequência de operações**, partindo da
**mesma configuração inicial** e com **memórias independentes**. O projeto faz
isso em `compare_scenario` (`src/main.c`): cria três simulações, uma por
estratégia, cada uma com sua própria `Memory`, reiniciada em 1024 KB livres, e
executa a mesma lista de operações nas três.

Isso é verificável na saída: a linha “Total de solicitacoes” é idêntica nas
três colunas, porque as três processaram a mesma sequência. Se uma simulação
influenciasse a outra, os totais ou os estados finais poderiam divergir por
motivos alheios ao algoritmo. No cenário de estresse, a sequência é gerada
**cega** em relação às estratégias (os tamanhos são sorteados sem consultar
nenhuma memória), mas é gerada **uma única vez** e reaplicada às três; assim, a
comparação continua justa e, ao mesmo tempo, o desfecho de cada pedido pode
diferir — inclusive com falhas reais em algumas estratégias e não em outras.

## 9. O que a literatura diz

- **Knuth (1997, seção 2.5, pp. 435–456)** formaliza a reserva e a liberação de
  blocos, apresenta o Algoritmo A (first-fit), o Algoritmo B (lista ordenada
  por endereço) e o Algoritmo C (boundary tags, que permite liberação em tempo
  praticamente constante). Também discute a “fifty-percent rule” e experimentos
  em que first-fit foi superior a best-fit.
- **Tanenbaum e Bos (2014, seção 3.2.3, pp. 190–194)** comparam first-fit,
  next-fit, best-fit e worst-fit. Observam que best-fit “tends to fill up
  memory with tiny, useless holes” e que worst-fit “is not a very good idea
  either”. Discutem o custo de compactação.
- **Silberschatz, Galvin e Gagne (2018, seção 9.2, pp. 356–359; seção 9.3)**
  tratam do dynamic storage-allocation problem, da fragmentação interna e
  externa e da compactação apenas com relocação dinâmica. A 10ª edição removeu
  a seção de segmentação; para segmentação, usa-se Tanenbaum, seção 3.7.
- **Bryant e O'Hallaron (2016, seção 9.9, pp. 875–900)** explicam fragmentação
  interna e externa, coalescência de blocos livres (p. 886) e coalescência com
  boundary tags (pp. 887–889), no contexto de um alocador de heap.
- **Wilson et al. (1995)** fazem uma revisão crítica da alocação dinâmica de
  armazenamento, discutindo best-fit exaustivo, remanescentes pequenos e
  inúteis, pior caso de best-fit e “splinters”.
- **Arpaci-Dusseau e Arpaci-Dusseau (2023, capítulo 17, seção 17.3)** apresentam
  estratégias de gerenciamento de espaço livre e afirmam explicitamente que
  “we will not describe a ‘best’ approach”.
- **Robson (1977)** e **Shore (1975)** são referências clássicas para
  fragmentação de pior caso em first-fit e best-fit. Neste trabalho, apenas os
  metadados bibliográficos foram verificados; o texto completo não foi lido.

## 10. CUIDADOS: o que não se deve afirmar

Esta seção existe para evitar conclusões que a teoria e os experimentos deste
projeto **não** sustentam.

**(a) Best Fit NÃO é o melhor algoritmo.** O nome não é uma garantia. Best Fit
minimiza o remanescente imediato, mas pode criar muitos buracos pequenos e
inúteis, aumentando a fragmentação externa ao longo do tempo (Tanenbaum e Bos,
2014; Wilson et al., 1995).

**(b) First Fit NÃO é sempre superior.** Knuth relata cargas em que first-fit
foi superior a best-fit, mas isso não é uma regra universal. O desempenho
depende da sequência de operações, dos tamanhos e da geometria da memória.

**(c) Worst Fit NÃO é bom por “preservar blocos grandes”.** Ele escolhe o maior
bloco disponível para a alocação atual; isso consome o bloco grande, não o
preserva. Tanenbaum e Bos (2014) afirmam que worst-fit “is not a very good idea
either”.

**(d) First Fit NÃO é O(1) no pior caso.** A busca pode percorrer toda a lista
quando não há bloco adequado ou quando o único bloco adequado está no fim. O
melhor caso é O(1) para a busca, mas não o pior.

**(e) Best Fit NÃO é O(log n) na lista simples.** Comparar todos os candidatos
em uma lista simplesmente encadeada é Theta(n). Uma estrutura de busca
ordenada poderia dar outra complexidade, mas não é a implementada aqui.

**(f) Fragmentação externa NÃO é “memória livre insuficiente”.** O cenário 2
mostra exatamente o contrário: há 512 KB livres, mas o pedido de 400 KB falha
porque não há 400 KB **contíguos**. O problema é a geometria da memória livre,
não o total.

**(g) Coalescência NÃO elimina toda a fragmentação externa e NÃO é O(1) sem
boundary tags.** Ela só une blocos adjacentes; blocos separados por um processo
ocupado continuam separados. Além disso, localizar o predecessor em uma lista
simplesmente encadeada custa O(n); a liberação em tempo praticamente constante
requer estruturas como boundary tags (Knuth, 1997; Bryant e O'Hallaron, 2016).

**(h) Compactação NÃO é gratuita nem sempre possível.** Ela exige mover dados e
relocação dinâmica; pode ser cara e pode não ser permitida. O simulador não a
faz.

**(i) O projeto NÃO reproduz um kernel.** A memória é uma região contígua
fictícia, os processos são identificadores, não há paginação, swap, threads,
prioridades, tabelas de páginas nem alocação de metadados do sistema
operacional. Modelos reais usam paginação/segmentação, heap com free lists,
boundary tags, buddy system (Linux) e slab allocator, entre outras técnicas.

**(j) A “fifty-percent rule” NÃO significa “50% da memória desperdiçada”.** Ela
é uma afirmação sobre o número médio de blocos **livres** em equilíbrio, sob
hipóteses fortes de um modelo probabilístico (Knuth, 1997, seção 2.5). Não é
uma medida de desperdício nem se aplica automaticamente a qualquer carga real.

## 11. Referências

BRYANT, Randal E.; O'HALLARON, David R. **Computer Systems: A Programmer's
Perspective**. 3. ed. Pearson, 2016. Seção 9.9, pp. 875–900.

ARPACI-DUSSEAU, Remzi H.; ARPACI-DUSSEAU, Andrea C. **Operating Systems: Three
Easy Pieces**. v. 1.10, 2023. Capítulo 17, seção 17.3.

KNUTH, Donald E. **The Art of Computer Programming, Vol. 1: Fundamental
Algorithms**. 3. ed. Addison-Wesley, 1997. Seção 2.5, pp. 435–456.

ROBSON, J. M. Worst case fragmentation of first fit and best fit storage
allocation strategies. **The Computer Journal**, v. 20, n. 3, p. 242–244,
1977. DOI: 10.1093/comjnl/20.3.242. (Metadados verificados; texto não lido.)

SHORE, J. E. On the external storage fragmentation produced by first-fit and
best-fit allocation strategies. **Communications of the ACM**, v. 18, n. 8,
p. 433–440, 1975. DOI: 10.1145/360933.360949. (Metadados verificados; texto não
lido.)

SILBERSCHATZ, Abraham; GALVIN, Peter B.; GAGNE, Greg. **Operating System
Concepts**. 10. ed. Wiley, 2018. Seção 9.2, pp. 356–359; seção 9.3.

TANENBAUM, Andrew S.; BOS, Herbert. **Modern Operating Systems**. 4. ed.
Pearson, 2014. Seção 3.2.3, pp. 190–194; seção 3.7.

WILSON, Paul R. et al. Dynamic Storage Allocation: A Survey and Critical
Review. In: **International Workshop on Memory Management (IWMM)**, 1995,
LNCS 986, Springer, 1995, pp. 1–116. DOI: 10.1007/3-540-60368-9_19.

### 11.1 Recursos eletrônicos

- OSTEP, capítulo 17 (PDF oficial):
  <https://pages.cs.wisc.edu/~remzi/OSTEP/vm-freespace.pdf>
- Slides oficiais do capítulo 9 de Silberschatz et al.:
  <https://www.os-book.com/OS10/slide-dir/index.html>
- Wilson et al. (PDF):
  <https://www.cs.jhu.edu/~huang/cs318/fall21/readings/wilson.pdf>
- Tanenbaum e Bos, 4. ed. (PDF de terceiros):
  <https://linux.harshkapadia.me/files/books/tanenbaum-modern-operating-systems-4th-edition.pdf>
- CS:APP, página oficial da 3. ed.:
  <https://csapp.cs.cmu.edu/3e/home.html>
- Documentação do alocador de memória do kernel Linux:
  <https://www.kernel.org/doc/html/latest/core-api/memory-allocation.html>
- Knuth, Vol. 1 (cópia de terceiros; prefira a edição impressa):
  <https://ia803207.us.archive.org/8/items/compatitive-programming/Books/The_Art_of_Computer_Programming%20-%20Vol%201_text.pdf>

> **Nota de honestidade bibliográfica.** Os números de página e seção acima
> seguem as edições indicadas. Os textos de Robson (1977) e Shore (1975) não
> foram lidos integralmente; suas referências foram usadas apenas como
> indicação de literatura clássica sobre fragmentação de pior caso. A cópia
> eletrônica de Knuth é de terceiros e pode divergir da edição impressa; a
> referência primária é a edição de 1997.
