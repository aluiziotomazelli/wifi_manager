# Snapshot do Ambiente de Desenvolvimento

Para recriar este ambiente em uma nova sessão ou máquina, siga os passos abaixo:

## 1. Instalação do ESP-IDF v5.5.2

```bash
# Criar diretório para o ESP
mkdir -p ~/esp
cd ~/esp

# Clonar o repositório do ESP-IDF (versão 5.5.2)
git clone --recursive --branch v5.5.2 --depth 1 https://github.com/espressif/esp-idf.git

# Entrar no diretório e instalar as ferramentas para host Linux
cd ~/esp/esp-idf
./install.sh linux
```

## 2. Configuração das Variáveis de Ambiente

Sempre que iniciar uma nova sessão, você deve carregar o ambiente do IDF:

```bash
. ~/esp/esp-idf/export.sh
```

## 3. Dependências do Sistema (Linux)

Certifique-se de que seu sistema possui as dependências necessárias para compilação em host e automação:

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 libbsd-dev ruby
```

### Instalar dependências de teste do Python:

```bash
python -m pip install pytest
```

## 4. Executando os Testes de Host

### Modo Automático (Recomendado)

Para executar todos os testes de host de uma vez:

```bash
cd host_test
python -m pytest pytest_host_tests.py
```

### Modo Manual

Para executar um teste específico manualmente:

```bash
cd host_test/<nome_do_teste>
idf.py --preview set-target linux
idf.py build
./build/*.elf
```

Testes disponíveis:
- `wifi_config_storage`
- `wifi_driver_hal`
- `wifi_event_handler`
- `wifi_state_machine`
- `wifi_sync_manager`
- `integration_internal`

---
*Este arquivo foi gerado automaticamente pelo Jules para documentar a configuração do ambiente.*
