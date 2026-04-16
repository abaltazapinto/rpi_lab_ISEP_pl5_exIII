# Loadable Kernel Module – Software PWM (GPIO12)

## 1. Objetivo

O objetivo deste trabalho é compreender e implementar um **Loadable Kernel Module (LKM)** em Linux, aplicado ao controlo de hardware no Raspberry Pi 4.

Neste caso, foi desenvolvido um módulo que implementa um **PWM (Pulse Width Modulation) por software** (“bit banging”) no pino **GPIO12**, permitindo controlar o brilho de um LED através de um valor de **duty cycle (0–100%)**.

---

## 2. Conceitos Fundamentais

### 2.1 Loadable Kernel Module (LKM)

Ao contrário de um programa em user-space, um módulo do kernel não possui função `main()`.
O ponto de entrada é definido através da macro:

```c
module_init(blinker_init);
```

Assim, a função `blinker_init()` é executada quando o módulo é carregado.

---

### 2.2 Character Device Driver

Este módulo implementa um **character device driver**, o que pode ser comprovado por:

1. Registo do dispositivo:

```c
register_chrdev(...)
```

2. Definição da interface através de:

```c
struct file_operations
```

Inclui:

* `open`
* `read`
* `write`
* `release`

3. Criação do device em user-space:

```
/dev/dimmer
```

---

### 2.3 Interação User Space ↔ Kernel Space

A comunicação com o módulo é feita através de operações sobre o ficheiro:

```bash
echo 50 > /dev/dimmer
```

Fluxo:

```
user → write() → kernel → blinker_write() → variável interna → timer → GPIO
```

---

### 2.4 Timer no Kernel

A função principal do PWM é executada em:

```c
my_timer_func()
```

Esta função é chamada sempre que o **hrtimer expira**, permitindo execução periódica com alta precisão.

---

## 3. Implementação do PWM

### 3.1 Parâmetros

* Entrada:

  * `duty_cycle_setpoint` (0–100)

* Saída:

  * GPIO12

* Frequência:

  * 1 kHz (período = 1 ms)

---

### 3.2 Lógica do PWM

O período é dividido em duas fases:

```
ONTIME  = duty_cycle / 100 × 1 ms
OFFTIME = (1 - duty_cycle / 100) × 1 ms
```

A variável `new_period` controla o estado:

* `1` → início do período (fase ON)
* `0` → fase OFF

---

### 3.3 Funcionamento

Dentro de `my_timer_func()`:

1. Se início de período:

   * Liga GPIO (se duty > 0)
   * Agenda tempo ON

2. Se fase OFF:

   * Desliga GPIO
   * Agenda tempo OFF

---

## 4. GPIO no Raspberry Pi

O GPIO é controlado através de **memory-mapped I/O**, escrevendo diretamente nos registos:

* SET → coloca pino a 1
* CLR → coloca pino a 0

Foi utilizada uma abstração:

```c
gpio12_init_output()
gpio12_set(value)
gpio12_cleanup()
```

---

## 5. Ambiente de Execução

* Board: Raspberry Pi 4 (BCM2711)
* OS: Raspberry Pi OS
* Kernel: 6.12.75+rpt-rpi-v8

---

## 6. Ligações de Hardware

* GPIO12 → resistência (~220Ω) → LED → GND

---

## 7. Compilação

```bash
make
```

---

## 8. Execução

### Carregar módulo

```bash
sudo insmod dimmer-rpi4.ko
```

### Testar PWM

```bash
echo 0   | sudo tee /dev/dimmer
echo 10  | sudo tee /dev/dimmer
echo 50  | sudo tee /dev/dimmer
echo 90  | sudo tee /dev/dimmer
echo 100 | sudo tee /dev/dimmer
```

### Remover módulo

```bash
sudo rmmod dimmer-rpi4
```

---

## 9. Resultados

O LED responde corretamente ao duty cycle:

| Duty Cycle | Comportamento   |
| ---------- | --------------- |
| 0          | OFF             |
| 10         | brilho reduzido |
| 50         | brilho médio    |
| 90         | quase ON        |
| 100        | ON contínuo     |

---

## 10. Conclusão

Foi possível:

* Implementar um LKM funcional
* Controlar hardware via kernel
* Desenvolver PWM por software usando `hrtimer`
* Estabelecer comunicação user ↔ kernel através de character device

Este exercício permitiu consolidar conceitos fundamentais de:

* sistemas embebidos
* drivers de dispositivos
* interação direta com hardware
