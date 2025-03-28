# Decibelímetro com BitDogLab

<p align="center">
  <img src="images/VirtusCC.png" alt="Virtus">
</p>

## Autores

- [Aryelson Gonçalves](https://github.com/aryelson1)  
- [Guilherme Santos](https://github.com/GuilhermexL)  

---

## Descrição do Projeto

O objetivo do projeto é a medição de níveis sonoros utilizando um microfone conectado à placa BitDogLab. A partir da leitura dos níveis de som, o sistema pode ativar alertas visuais ou sonoros através de um buzzer.

Este projeto foi desenvolvido como parte de uma etapa em que os grupos foram definidos por sorteio, resultando na troca de projetos entre os participantes. Nosso grupo ficou responsável pelo projeto desenvolvido por outro grupo, cujo repositório pode ser acessado no link correspondente:

- [Buzzer](https://github.com/athavus/bitdoglab-buzzer-lib)
- [Matriz de LED](https://github.com/athavus/bitdoglab-mledsrgb-lib)
- [Microfone](https://github.com/athavus/bitdoglab-mic-lib)

OBS: Utilizamos nossa biblioteca de [Display](https://github.com/aryelson1/Display_Oled_BitDogLab), pois tivemos ideia de um ajuste para o projeto.

---

## Arquitetura do Projeto

![Placa](images/Placa_profile.png)

### Hardware Utilizado

- 1x Placa BitDogLab
- 1x Botão para iniciar
- 1x Microfone para captar ruído
- 1x Display (imprimir resultado)
- 1x Buzzer (ativo ou passivo)

### Conexões

- Microfone: Pino **GPIO18**
- Buzzer: Pino **GPIO21**
- Botão de calibração: Pino **GPIO5**

---

## Análise crítica e melhorias da biblioteca utilizada/testada

Durante o desenvolvimento e testes da biblioteca utilizada, algumas observações e pontos de melhoria foram identificados:

1. **Eficiência e Uso de Recursos:**
   - A utilização de DMA para capturar os dados do microfone foi eficiente, pois permite que a leitura dos dados seja feita de forma assíncrona, sem sobrecarregar o processador. No entanto, o buffer de dados poderia ser mais flexível, permitindo um maior controle sobre a quantidade de amostras armazenadas.
   - A biblioteca utiliza 12 bits de resolução no ADC, o que oferece uma boa precisão, mas em algumas situações de baixo sinal, pode ser interessante considerar um ajuste automático para aumentar a resolução conforme a necessidade.

2. **Calibração do Microfone:**
   - O cálculo de intensidade e o nível de som em decibéis SPL dependem de uma calibração inicial precisa do microfone. A fórmula para o cálculo de decibéis ajustada empiricamente pode ser melhorada para adaptar-se a diferentes microfones e ambientes. Uma possível melhoria seria a implementação de um processo de calibração automática no início do sistema, onde o ambiente e o microfone são analisados para ajustar os parâmetros.

3. **Consistência na Leitura de Amostras:**
   - Embora o DMA tenha sido uma solução eficiente para a captura das amostras, houve algumas variações na consistência dos dados capturados, especialmente em ambientes com interferências elétricas. A implementação de um filtro digital ou um processo de média móvel poderia ser uma solução viável para melhorar a precisão das leituras.

4. **Flexibilidade na Matriz de LEDs e Buzzer:**
   - A configuração da matriz de LED e do buzzer é relativamente simples, mas falta uma maior flexibilidade para adaptação a diferentes tamanhos de matrizes e diferentes tipos de buzzer. Uma sugestão de melhoria seria a criação de funções genéricas para permitir a personalização do tamanho da matriz de LED, bem como a implementação de modulação de frequência do buzzer, para representar diferentes tipos de feedback sonoro.

5. **Modularidade e Documentação:**
   - A biblioteca pode ser mais modularizada, separando funções específicas (como controle de buzzer, matriz de LED e captura do microfone) em módulos independentes. Isso facilitaria a manutenção e permitiria que partes do código fossem reutilizadas em outros projetos.
   - A documentação da biblioteca também pode ser melhorada, fornecendo exemplos de uso e explicações detalhadas de cada função e parâmetro. Isso ajudaria novos usuários a entender rapidamente como usar a biblioteca em seus projetos.

6. **Ajustes de Desempenho:**
   - Em alguns cenários de alto desempenho, como em leituras de áudio em tempo real ou controle de várias matrizes de LED, a otimização do código pode ser necessária. A implementação de interrupções ou o uso de algoritmos mais eficientes para a leitura e processamento dos dados pode reduzir a carga no processador e melhorar o desempenho geral.

### Sugestões de Melhorias:
- Implementação de calibração automática para o microfone.
- Ajustes de filtros digitais para melhorar a qualidade dos dados capturados.
- Funções genéricas para maior flexibilidade na configuração do buzzer e da matriz de LED.
- Modularização da biblioteca para maior reutilização e manutenção.
- Otimização do desempenho em cenários de alta demanda, com a introdução de interrupções e técnicas de processamento mais eficientes.

---

## Demonstração

Abaixo está um GIF demonstrando o funcionamento do projeto na placa:

![Demonstração](/images/demonstracao.gif)

---

## Como Compilar e Executar

### Requisitos

- **Visual Studio Code** (ou outra IDE compatível)
- **Extensão Raspberry Pi Pico** (para desenvolvimento com Raspberry Pi Pico)
- **Raspberry Pi Pico SDK** (versão 2.1.1)

### Passos para Compilação

1. Clone o repositório do projeto.
2. Abra o projeto no Visual Studio Code.
3. Importe o projeto utilizando a extensão Raspberry Pi Pico.
4. Configure o SDK para a versão 2.1.1.

### Passos para Execução

1. Conecte a Raspberry Pi Pico ao computador via USB.
2. Carregue o arquivo `.uf2` gerado na Pico.
3. Pressione o botão para iniciar a medição e calibração.

---

## Conclusão

Este projeto demonstra a captura e análise de níveis sonoros utilizando a BitDogLab. Ele pode ser expandido para incluir novas funcionalidades, como a integração com uma interface web para visualização dos dados coletados.

Acesse o documento do projeto em PDF [aqui](/doc/Análise%20Crítica%20do%20Projeto%20Final.pdf)

---

## Referências

- [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)  
- [Documentação oficial da Raspberry Pi Pico](https://www.raspberrypi.com/documentation/microcontrollers/)