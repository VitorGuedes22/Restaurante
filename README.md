# Sistema de Gerenciamento de Recursos de Cozinha

## Descrição

Este projeto implementa um sistema de gerenciamento de recursos para cozinheiros em um ambiente de cozinha, utilizando programação concorrente em C. O sistema simula um restaurante onde cozinheiros preparam diferentes receitas, gerenciando recursos como fogões, panelas, fornos e bancadas. O projeto utiliza threads, semáforos, mutexes e variáveis de condição para garantir a coordenação eficiente entre os cozinheiros e os recursos disponíveis.

## Funcionalidades

- **Gerenciamento de Recursos**: Controle do acesso aos recursos da cozinha (fogão, panela, forno, bancada, grill) usando semáforos.
- **Simulação de Receitas**: Preparação de receitas que exigem diferentes combinações de recursos.
- **Threads de Cozinheiros**: Criação de threads para simular cozinheiros que preparam receitas de maneira concorrente.
- **Fila de Cozinheiros**: Gerenciamento da fila de cozinheiros para garantir a ordem de atendimento.
- **Sincronização**: Uso de mutexes, semáforos e variáveis de condição para sincronizar o acesso aos recursos e gerenciar a disponibilidade dos cozinheiros.

## Estrutura do Projeto

1. **Código Fonte**
   - `projeto.c`: Implementação da função principal que inicializa os recursos, cria e gerencia threads de cozinheiros.

2. **Documentação**
   - `relatorio.pdf`: Relatório detalhado sobre a implementação do programa e a solução proposta.

## Compilação e Execução

Para compilar e executar o projeto, siga estas etapas:

1. **Compilação**
   ```bash
   gcc -o projeto.c -lpthread
