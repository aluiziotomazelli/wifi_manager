# Guia de Estudo: Testes Unitários no Host (Linux) com ESP-IDF

Este guia resume o estudo sobre como utilizar o sistema de mocks e testes em host do ESP-IDF, focando nos componentes `gpio_validator` e `wifi_manager`.

## 1. Arquitetura de Testes em Host

O ESP-IDF permite executar aplicações (incluindo testes) diretamente no Linux/macOS. Existem duas abordagens principais que podem ser combinadas:

1.  **Linux Target (Simulação POSIX):** O IDF compila o código para a arquitetura do seu PC (`x86_64` ou `arm64`) em vez do chip ESP32. Alguns componentes (como `esp_hw_support`, `esp_event`, `nvs_flash`, `freertos`) possuem uma implementação específica para Linux que simula o comportamento real do hardware ou do RTOS usando APIs do sistema operacional (pthreads, sockets, etc).
2.  **CMock (Mocking):** Para componentes que não possuem uma simulação para Linux ou que você deseja isolar completamente, o IDF integra o framework **CMock**. Ele gera automaticamente implementações "falsas" (mocks) de headers C, permitindo que você defina expectativas (ex: `expect_esp_wifi_connect()`) e valores de retorno nos seus testes.

## 2. Requisitos de Sistema

Para rodar testes de host com mocks, seu sistema Linux precisa de:

*   **ESP-IDF configurado:** Com a variável `IDF_PATH` exportada.
*   **Ruby:** Necessário para o CMock processar os headers e gerar os mocks.
*   **libbsd-dev:** Necessário para algumas funções de compatibilidade BSD usadas no simulador Linux do IDF.
*   **GCC/G++ e CMake:** Ferramentas padrão de compilação.



## 5. Estudo de Caso: `wifi_manager`

O `wifi_manager` é mais complexo pois depende de:
*   `freertos` (Tasks, Queues, Event Groups, Mutexes)
*   `esp_wifi` (API de controle do rádio)
*   `esp_event` (Sistema de eventos)
*   `nvs_flash` (Persistência de credenciais)

### Estratégia de Mocking para `wifi_manager`:
1.  **FreeRTOS:** Usar o mock/simulador de FreeRTOS do IDF. No host, ele permite criar tasks, mas o escalonamento é cooperativo ou simulado via pthreads.
2.  **esp_wifi:** Como não existe na v5.1.1, criaríamos um mock simples que apenas registra se `esp_wifi_connect()` foi chamado.
3.  **Lógica da FSM:** O maior valor do teste de host aqui é validar a **Máquina de Estados**. Podemos simular eventos (ex: `WIFI_EVENT_STA_DISCONNECTED`) chamando os handlers e verificar se o `wifi_manager` entra em `WAITING_RECONNECT` com o backoff correto.

## 6. Como configurar um teste de host no seu componente

A estrutura recomendada é criar uma pasta `host_test` dentro do componente:

```text
meu_componente/
├── host_test/
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── test_meu_componente.cpp
│   └── CMakeLists.txt (Projeto de teste)
├── meu_componente.cpp
└── include/meu_componente.hpp
```

No `CMakeLists.txt` do projeto de teste, você define o target como Linux:
```cmake
set(COMPONENTS main meu_componente)
list(APPEND EXTRA_COMPONENT_DIRS "$ENV{IDF_PATH}/tools/mocks/freertos")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
```

