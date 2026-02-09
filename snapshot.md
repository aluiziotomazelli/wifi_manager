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

Certifique-se de que seu sistema possui as dependências necessárias para compilação em host:

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 libbsd-dev ruby
```

## 4. Executando os Testes de Host

Para executar os testes de host do `wifi_config_storage`:

```bash
# Navegar até a pasta do teste
cd host_test/wifi_config_storage

# Garantir que o target está como linux
idf.py --preview set-target linux

# Build e execução
idf.py build
./build/wifi_config_storage_host_test.elf
```

---
*Este arquivo foi gerado automaticamente pelo Jules para documentar a configuração do ambiente.*
