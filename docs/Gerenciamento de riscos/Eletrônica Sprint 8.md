# Gerenciamento de Riscos — Eletrônica

Mapeamento dos riscos da sub-equipe de Eletrônica, levantado a partir das issues do Project `2026_2_PI1_Grupo2_Hilmer` e dos resultados registrados em [7.3 - Testes de hardware](../7.3%20-%20Testes%20de%20hardware.md).

## Métricas

O score de cada risco é o produto da probabilidade pelo impacto (Score = P × I), variando de 1 a 100.

| Probabilidade | Nota |
|---|:-:|
| Muito provável | 9 a 10 |
| Provável | 7 a 8 |
| Médio | 5 a 6 |
| Difícil | 3 a 4 |
| Muito difícil | 1 a 2 |

| Impacto | Nota |
|---|:-:|
| Desprezível | 1 a 2 |
| Menor | 3 a 4 |
| Moderada | 5 a 6 |
| Maior | 7 a 8 |
| Catastrófica | 9 a 10 |

## Matriz de riscos

| # | Risco | Probabilidade | Impacto | Score (P×I) |
|:-:|---|:-:|:-:|:-:|
| R1 | **Densidade dos componentes:** o chassi atual não comporta ESP32 de 38 pinos, L298N, buck, INA219, MPU6050, microSD, 3 VL53L0X, bateria e cabeamento. A remodelagem atrasa a montagem e pode deixar sensores mal posicionados, cabos tensionados ou nenhum acesso ao USB, ao cartão e à bateria | 9 | 9 | **81** |
| R2 | PID mal sintonizado: o robô não anda reto nem gira 90° com precisão e bate nas paredes | 7 | 8 | **56** |
| R3 | Curto ou ligação invertida na montagem (VBAT 8,4 V, buck 5 V) queima o ESP32 ou os módulos | 5 | 9 | **45** |
| R4 | Mau contato por causa da vibração, com reset do ESP32 ou sensor sumindo do I2C durante a corrida | 6 | 7 | **42** |
| R5 | Odometria imprecisa: contagem de pulsos por célula ainda não medida e erro acumulado em 12x4 | 6 | 7 | **42** |
| R6 | Ruído dos motores e quedas de tensão afetam o I2C, a IMU e o INA219 (agravado pelos cabos próximos em um chassi denso) | 6 | 7 | **42** |
| R7 | Deriva da IMU (~2,2°/min) e erro de ~2,3° no giro de 90° acumulam desalinhamento nas corridas longas | 7 | 6 | **42** |
| R8 | Erro dos sensores de distância leva a parede detectada errada e mapa incorreto | 5 | 8 | **40** |
| R9 | Wi-Fi instável no local da apresentação: a telemetria não chega e a nota do labirinto cai para x0,75 | 5 | 8 | **40** |
| R10 | **Tensão acima do suportado pelos motores:** a bateria chega a 8,4 V e os motores são de 6 V nominais. Se o limite de duty falhar (leitura errada do INA219, bug no firmware ou duty de 100% durante testes), ou se a L298N for trocada por uma ponte de baixa queda sem recalcular o limite, os motores superaquecem, desgastam escovas e redução ou queimam | 5 | 8 | **40** |
| R11 | Componente queimar ou falhar sem peça de reserva (Ponte H, MPU6050, motor N20) | 4 | 9 | **36** |
| R12 | Bateria LiPo 2S descarregada abaixo de 6,6 V, inchada ou danificada, e o robô fica parado no dia | 4 | 9 | **36** |
| R13 | **Falha na ponte H:** a L298N queima ou deixa de acionar um dos motores por sobrecorrente (motor travado contra a parede), aquecimento, inversão brusca de sentido ou mau contato nos bornes, e o robô para ou anda em círculo | 4 | 9 | **36** |
| R14 | INA219 sem validação sob carga, com estimativa de carga e limite de duty errados | 5 | 5 | **25** |
| R15 | Flash do ESP32 em 92,2%, sem espaço para ajustes | 5 | 4 | **20** |
| R16 | Cartão microSD falha ou corrompe, e os dados da corrida se perdem | 3 | 4 | **12** |

## Mitigações

- **R1 — Densidade dos componentes:**
  - Montar os componentes no CAD do chassi novo, com modelos simplificados (caixas com as medidas), e validar o encaixe antes de imprimir.
  - Usar layout em dois andares: em baixo motores, bateria e L298N; em cima ESP32, sensores I2C e microSD.
- **R2 — PID:** sintonizar uma roda de cada vez com resposta ao degrau, registrando Kp, Ki e Kd. Usar a IMU para corrigir a direção ao andar reto e testar primeiro em uma célula e só depois em trechos do 4x4.
- **R3 — Curto ou ligação invertida:** antes de ligar o ESP32, conferir continuidade e polaridade com o multímetro. Regular o buck para 5,0 V sem carga, usar fusível ou polifusível na saída da bateria e conectores com polaridade.
- **R4 — Mau contato:** soldar em vez de usar jumpers de protoboard, fixar os cabos com abraçadeira ou cola quente e fazer um teste de vibração (bater levemente e mover o robô) observando o monitor serial.
- **R5 — Odometria:** fazer o teste 3.10 empurrando o robô 18 cm várias vezes e calcular os pulsos por célula pela média. Corrigir a posição usando as paredes como referência ao final de cada célula.
- **R6 — Ruído:** usar GND em estrela e manter os fios do I2C curtos e longe dos fios dos motores. Validar com os motores ligados.
- **R7 — Deriva da IMU:** recalibrar o offset sempre que o robô estiver parado, zerar o ângulo após cada giro e alinhar com as paredes laterais.
- **R8 — Sensores de distância:** recalibrar com as paredes reais do labirinto (brancas, de 5 cm), definir limiares de parede com histerese, filtrar com média móvel e tratar 65535 como "sem parede".
- **R9 — Wi-Fi:** levar roteador ou hotspot dedicado em 2,4 GHz, testar no local antes da apresentação e garantir a reconexão automática e a gravação no microSD como fonte de backup para reenviar a telemetria.
- **R10 — Tensão acima do suportado pelos motores:**
  - Medir com o multímetro a tensão real nos terminais dos motores com duty máximo e bateria cheia (8,4 V), já com a queda da ponte, e ajustar o limite para no máximo 6 V médios.
  - Ter um limite fixo de duty no firmware, além do calculado pela tensão do INA219: se a leitura do INA219 falhar ou sair da faixa de 6,6 a 8,4 V, usar o duty correspondente a 8,4 V (pior caso).
- **R11 — Peças de reserva:** comprar uma unidade extra dos componentes críticos (incluindo a ponte H e um par de motores) e deixar um ESP32 reserva já gravado com o firmware final.
- **R12 — Bateria:** usar só o carregador balanceado (#212), bloquear a operação abaixo de 6,6 V (#252).
- **R13 — Falha na ponte H:**
  - No firmware, frear e esperar alguns milissegundos antes de inverter o sentido, e cortar o PWM se o encoder indicar roda parada com duty alto por mais de um tempo limite (motor travado).
  - Apertar e conferir os bornes para evitar mau contato com a vibração.
- **R14 — INA219:** completar o teste 3.8 com os motores girando e comparar com um multímetro de melhor resolução em três níveis de carga.
- **R15 — Flash:** trocar a tabela de partições (por exemplo `huge_app.csv` ou `min_spiffs.csv`) e remover logs de depuração da versão final.
- **R16 — microSD:** gravar com `flush` periódico, testar com o robô montado (teste 3.15).
