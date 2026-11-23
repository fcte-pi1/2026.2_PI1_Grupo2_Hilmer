# Gerenciamento de Riscos — Eletrônica (Sprint 9)

Reavaliação da [matriz de riscos da Sprint 8](Eletr%C3%B4nica%20Sprint%208.md), feita a partir das issues do Project `2026_2_PI1_Grupo2_Hilmer` e do andamento da sprint. Os riscos mantêm a numeração da Sprint 8 e a tabela está ordenada pelo score atual.

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

| # | Risco | Probabilidade | Impacto | Score Sprint 8 | Score Sprint 9 (P×I) |
|:-:|---|:-:|:-:|:-:|:-:|
| R2 | PID mal sintonizado: o robô não anda reto nem gira 90° com precisão e bate nas paredes | 8 | 8 | 56 | **64** |
| R1 | **Densidade dos componentes:** o chassi novo pode não comportar ESP32 de 38 pinos, L298N, buck, INA219, MPU6050, microSD, 3 VL53L0X, bateria e cabeamento, como já ocorreu no chassi antigo (#330). Um encaixe apertado pode deixar sensores mal posicionados, cabos tensionados ou nenhum acesso ao USB, ao cartão e à bateria | 6 | 9 | 81 | **54** |
| R4 | Mau contato por causa da vibração, com reset do ESP32 ou sensor sumindo do I2C durante a corrida. | 7 | 7 | 42 | **49** |
| R8 | Erro dos sensores de distância leva a parede detectada errada e mapa incorreto. A calibração foi feita em protoboard, antes do novo suporte dos sensores (#317) | 6 | 8 | 40 | **48** |
| R17 | **Robô pesado demais para os motores N20:** o peso do robô montado derruba a rotação ou leva a corrente para perto do limite dos motores, sem medição registrada até agora (#332) | 6 | 8 | Novo | **48** |
| R3 | Curto ou ligação invertida na montagem (VBAT 8,4 V, buck 5 V) queima o ESP32 ou os módulos | 5 | 9 | 45 | **45** |
| R5 | Odometria imprecisa: contagem de pulsos por célula ainda não medida e erro acumulado em 12x4 | 6 | 7 | 42 | **42** |
| R6 | Ruído dos motores e quedas de tensão afetam o I2C, a IMU e o INA219 (agravado pelos cabos próximos em um chassi denso) | 6 | 7 | 42 | **42** |
| R7 | Deriva da IMU (~2,2°/min) e erro de ~2,3° no giro de 90° acumulam desalinhamento nas corridas longas | 7 | 6 | 42 | **42** |
| R9 | Wi-Fi instável no local da apresentação: a telemetria não chega e a nota do labirinto cai para x0,75 | 5 | 8 | 40 | **40** |
| R10 | **Tensão acima do suportado pelos motores:** a bateria chega a 8,4 V e os motores são de 6 V nominais. Se o limite de duty falhar (leitura errada do INA219, bug no firmware ou duty de 100% durante testes), ou se a L298N for trocada por uma ponte de baixa queda sem recalcular o limite, os motores superaquecem, desgastam escovas e redução ou queimam | 5 | 8 | 40 | **40** |
| R11 | Componente queimar ou falhar sem peça de reserva (Ponte H, MPU6050, motor N20) | 4 | 9 | 36 | **36** |
| R12 | Bateria LiPo 2S descarregada abaixo de 6,6 V, inchada ou danificada, e o robô fica parado no dia | 4 | 9 | 36 | **36** |
| R13 | **Falha na ponte H:** a L298N queima ou deixa de acionar um dos motores por sobrecorrente (motor travado contra a parede), aquecimento, inversão brusca de sentido ou mau contato nos bornes, e o robô para ou anda em círculo | 4 | 9 | 36 | **36** |
| R14 | INA219 sem validação sob carga, com estimativa de carga e limite de duty errados | 5 | 5 | 25 | **25** |
| R15 | Flash do ESP32 em 92,2%, sem espaço para ajustes | 5 | 4 | 20 | **20** |
| R16 | Cartão microSD falha ou corrompe, e os dados da corrida se perdem | 3 | 4 | 12 | **12** |

## Alterações desde a Sprint 8

- **R2 (56 → 64):** a sintonia do PID (#252) depende do robô montado. Com o atraso da montagem, sobra menos tempo para os ajustes.
- **R1 (81 → 54):** o risco se concretizou no chassi antigo (#330). O chassi foi remodelado (#306), o que reduz a probabilidade, mas o encaixe só será comprovado após a reimpressão (#321) e a montagem (#329).
- **R4 (42 → 49):** o mau contato já ocorreu. Os pinos soldados nos fios (#313) ajudam, mas as extensões soldadas (#331) ainda não foram feitas. A probabilidade só deve cair depois delas.
- **R8 (40 → 48):** os sensores ganharam um novo suporte (#317), então a calibração anterior precisa ser refeita na posição final.
- **R17 (novo, 48):** a equipe de Estruturas pediu a verificação do peso em relação aos motores (#332), e ainda não há resultado registrado.

Nenhum risco foi encerrado nesta sprint, porque as issues e os testes que os resolveriam continuam pendentes.

## Mitigações

- **R1 — Densidade dos componentes:** Conferir o encaixe de todos os componentes na peça reimpressa (#321), antes de fixar.
- **R2 — PID:** sintonizar uma roda de cada vez com resposta ao degrau, registrando Kp, Ki e Kd. Usar a IMU para corrigir a direção ao andar reto e testar primeiro em uma célula e só depois em trechos do 4x4.
- **R3 — Curto ou ligação invertida:** antes de ligar o ESP32, conferir continuidade e polaridade com o multímetro. Regular o buck para 5,0 V sem carga, usar fusível ou polifusível na saída da bateria e conectores com polaridade.
- **R4 — Mau contato:** soldar as extensões em vez de encaixar um jumper no outro, isolar as emendas e testar a continuidade com o multímetro. Fixar os cabos com abraçadeira ou cola quente e fazer um teste de vibração (bater levemente e mover o robô) observando o monitor serial.
- **R5 — Odometria:** fazer o teste 3.10 empurrando o robô 18 cm várias vezes e calcular os pulsos por célula pela média. Corrigir a posição usando as paredes como referência ao final de cada célula.
- **R6 — Ruído:** usar GND em estrela e manter os fios do I2C curtos e longe dos fios dos motores. Validar com os motores ligados.
- **R7 — Deriva da IMU:** recalibrar o offset sempre que o robô estiver parado, zerar o ângulo após cada giro e alinhar com as paredes laterais.
- **R8 — Sensores de distância:** recalibrar com os sensores no novo suporte, na posição final, e com as paredes reais do labirinto.
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
- **R17 — Peso do robô:** comparar o RPM das rodas no ar e no chão, junto com a corrente lida pelo INA219 (#332), e enviar o resultado à equipe de Estruturas.