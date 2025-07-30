
# 📈 Projeto: Datalogger de Movimento com MPU6050

**Fase 2 - EmbarcaTech**  
**Desenvolvido por: Jeová Pinheiro - Polo de Vitória da Conquista**

---

## 📌 Descrição

Este projeto implementa um **datalogger portátil de movimento** utilizando a placa **BitDogLab com Raspberry Pi Pico W** e o sensor IMU **MPU6050**. O dispositivo é capaz de capturar dados de aceleração e giroscópio, calcular os ângulos de inclinação e armazenar essas informações em um cartão MicroSD no formato `.csv`.

Todo o processo de captura é controlado por botões físicos com feedback visual (LED RGB), sonoro (buzzer) e textual (display OLED). Um script em Python permite a posterior análise dos dados gerando gráficos.

---

## 🛠️ Componentes Utilizados

| Componente             | Função                                        |
| ---------------------- | --------------------------------------------- |
| Raspberry Pi Pico W    | Microcontrolador principal (BitDogLab)        |
| MPU6050                | Sensor IMU (aceleração e giroscópio via I2C) |
| Display OLED SSD1306   | Exibição de status, roll, pitch e contagem   |
| Cartão MicroSD         | Armazenamento dos dados em CSV               |
| LED RGB (pinos 11-13)  | Indicação visual de estado                   |
| Buzzer (pino 21)       | Feedback sonoro                              |
| Botões físicos         | Controle de gravação, montagem e BOOTSEL     |

---

## 🚀 Funcionalidades Implementadas

- ✅ Leitura contínua dos dados de aceleração e giroscópio (eixos X, Y, Z)
- ✅ Cálculo dos ângulos de **roll** e **pitch**
- ✅ Armazenamento dos dados no cartão SD em formato `.csv`
- ✅ Exibição das informações em tempo real no **display OLED**
- ✅ Feedback por **LED RGB** indicando os estados do sistema:
  * 🟨 Amarelo – Inicializando
  * 🟩 Verde – Pronto para gravar
  * 🔴 Vermelho – Gravando dados
  * 🔵 Azul – Acessando SD
  * 🟪 Roxo – Erro ao montar ou acessar SD
- ✅ Alertas sonoros com **buzzer**:
  * 1 bip: montagem do SD
  * 2 bips: erro
  * 3 bips: gravação finalizada
- ✅ Controle por **botões físicos com interrupções e debounce**
- ✅ Script Python para leitura e **geração de gráficos** com os dados registrados

---

## 📂 Estrutura do Projeto

```
📁 ArquivosDados/
├── CMakeLists.txt         → Arquivo de build do projeto
├── Principal.c            → Código principal do sistema
├── hw_config.c            → Configuração de hardware da BitDogLab
├── lib/                   → Bibliotecas auxiliares (SSD1306, SD card, etc.)
├── plotarGrafico.py       → Script Python para gerar gráficos dos dados
└── README.md              → Este arquivo
```

---

## ▶️ Como Executar o Projeto

1. **Clone o projeto** no seu computador:

```bash
git clone https://github.com/jeova-pinheiro/Datalogger-Movimento-MPU6050
```

2. **Compile usando o Pico SDK**:

```bash
mkdir build && cd build
cmake ..
make
```

3. **Grave o firmware** no Raspberry Pi Pico W:

- Pressione e segure o botão **BOOTSEL**
- Conecte o cabo USB
- Copie o arquivo `.uf2` gerado para a unidade montada

4. **Execute o projeto e pressione o botão de gravação**
5. **Remova o cartão SD e analise o arquivo `imu_data.csv` gerado**
6. **Rode o script Python para visualizar os gráficos:**

```bash
python3 plotarGrafico.py
```

---

## 🧠 Lógica de Funcionamento

- O sistema inicializa os periféricos (MPU6050, OLED, LED RGB, buzzer e SD)
- O botão de gravação inicia a captura de 30 amostras (personalizável)
- Cada amostra é registrada no cartão SD com os valores brutos de aceleração e giroscópio
- O LED RGB e o display indicam os estados ao usuário
- O botão de SD permite listar os arquivos no cartão via terminal serial
- O botão BOOTSEL entra em modo de regravação USB

---

## 👨‍💻 Autor

Projeto desenvolvido por **Jeová Pinheiro** para a fase 2 do programa ***EmbarcaTech***.
