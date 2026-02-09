# WiFi Manager Host Tests

Esta pasta contém os testes unitários e de integração configurados para rodar diretamente no Linux Host, utilizando mocks para o hardware e a implementação real do NVS para persistência.

## Estrutura

- `common/`: Contém utilitários compartilhados, stubs globais e mocks manuais para as APIs do ESP-IDF.
- `wifi_*/`: Projetos individuais de teste para cada sub-componente.
- `integration_internal/`: Testes de integração da lógica interna da FSM e gerenciamento de filas.
- `pytest_host_tests.py`: Script de automação para build e execução de toda a suíte.

## Como Executar

Certifique-se de que o ambiente do ESP-IDF está carregado (`. export.sh`).

### Executar todos os testes
```bash
python -m pytest pytest_host_tests.py
```

### Executar um teste específico
```bash
cd <pasta_do_teste>
idf.py --preview set-target linux
idf.py build
./build/*.elf
```

## Benefícios desta Arquitetura
1. **Isolamento**: Cada suíte roda em seu próprio projeto, evitando conflitos de mocks.
2. **Velocidade**: Execução em milissegundos sem necessidade de flash no hardware.
3. **Persistência Real**: O uso do `nvs_flash` de host permite validar se as credenciais realmente sobrevivem a reinicializações simuladas.
4. **CI-Ready**: O script `pytest` gera relatórios compatíveis com ferramentas de CI como GitHub Actions.
