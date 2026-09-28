# Animatronic Eye — XIAO ESP32-S3 Sense

Firmware nativo **ESP-IDF 5.5**, em C++, sem Arduino. Controle horizontal/vertical, pálpebra opcional, portal local, calibração NVS, detecção facial ESP-DL e atualização OTA com rollback.

**Primeiro uso sem câmera:** câmera e tracking iniciam desligados; os servos iniciam sem PWM. A pálpebra vem desabilitada. A câmera só é inicializada quando solicitada no portal. Não é necessário conectar o módulo Sense para testar o controle manual.

## Hardware e pinagem

Configuração padrão: XIAO ESP32-S3 com **8 MB de flash**, PSRAM octal de 8 MB e câmera do módulo Sense (OV2640/OV3660, conforme revisão).

| Função | Pino XIAO | GPIO |
| --- | --- | --- |
| Servo horizontal — sinal | D0 | 1 |
| Servo vertical — sinal | D1 | 2 |
| Pálpebra — sinal, opcional | D3 | 4 |
| Terra dos servos | GND | GND comum |
| Alimentação dos servos | Fonte externa regulada | Conforme especificação do servo |

Use fonte externa dimensionada para a corrente de partida/travamento dos servos. **Não alimente servos pelo pino 3V3.** Una o GND da fonte ao GND da XIAO. Evite unir saídas de fontes de 5 V diferentes. O USB pode alimentar a XIAO enquanto os servos usam a fonte externa.

Os GPIOs podem ser alterados em `idf.py menuconfig → Component config → Animatronic eye`. São aceitos 1, 2, 4, 5, 6, 7, 8, 9, 43 e 44, sem duplicação. Os pinos 7/8/9 podem conflitar com o cartão SD do Sense; este firmware não usa SD. Preserve os pinos da câmera e do USB.

Pinagem interna da câmera:

| Sinal | GPIO |
| --- | --- |
| XCLK | 10 |
| SCCB SDA / SCL | 40 / 39 |
| D0 / D1 / D2 / D3 | 15 / 17 / 18 / 16 |
| D4 / D5 / D6 / D7 | 14 / 12 / 11 / 48 |
| VSYNC / HREF / PCLK | 38 / 47 / 13 |
| PWDN / RESET | Não conectados (-1) |

A câmera usa LEDC timer 0/channel 0. Os servos usam timer 1/channels 1–3, 50 Hz e resolução de 14 bits.

## Build e flash

Instale o ESP-IDF **v5.5** e suas ferramentas para `esp32s3`. Abra o terminal do ESP-IDF:

```sh
git clone https://github.com/engperini/Animatronic-Eye-ESP32.git
cd Animatronic-Eye-ESP32
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Substitua `PORT` por `COM5`, por exemplo, ou `/dev/ttyACM0`. Para sair do monitor, use Ctrl+]. Se necessário, entre no modo download segurando BOOT ao conectar o USB e depois solte.

No PowerShell, com uma instalação em `C:\Espressif`, carregue o ambiente antes desses comandos:

```powershell
$env:IDF_TOOLS_PATH = 'C:\Espressif'
. C:\Espressif\frameworks\esp-idf-v5.5\export.ps1
```

Se o export apontar para um ambiente Python antigo que não existe, configure `IDF_PYTHON_ENV_PATH` para o ambiente instalado de IDF 5.5 e acrescente sua pasta `Scripts` ao PATH antes do export.

As dependências são baixadas pelo Component Manager e fixadas por `dependencies.lock`: esp32-camera 2.1.3, human_face_detect 0.3.0, ESP-DL 3.2.4 e dependências transitivas. Preserve esse arquivo. Não é necessário copiar bibliotecas manualmente.

Artefatos principais:

- `build/animatronic_eye.bin`: aplicativo para upload OTA.
- `build/bootloader/bootloader.bin`, `build/partition_table/partition-table.bin`, `build/ota_data_initial.bin`: usados pelo flash inicial via USB.
- `build/flash_args`: endereços gerados pelo build.

O primeiro flash deve incluir bootloader e tabela de partições; use `idf.py flash`. OTA não altera esses elementos. Não use o arquivo de bootloader ou uma imagem mesclada no portal.

## Primeiro teste, sem câmera

1. Desconecte mecanicamente os braços dos servos antes da primeira centralização. Os pulsos padrão de 1300/1500/1700 µs são apenas um ponto de partida, não uma calibração do seu mecanismo.
2. Grave o firmware e abra o monitor serial. Localize a linha `AP: AnimatronicEye-XXXXXX | Password: ...`.
3. Conecte-se à rede indicada. A senha aleatória de 16 caracteres é persistida na NVS e permanece igual após reiniciar.
4. Abra **http://192.168.4.1/**. Se o portal cativo não abrir automaticamente, use essa URL diretamente no navegador.
5. Autentique com usuário **admin** e a mesma senha do AP.
6. Deixe a câmera desligada e a pálpebra desabilitada. Clique em **Ativar servos** e **Centralizar**. Monte os braços no centro somente depois de verificar o posicionamento.
7. Faça a calibração descrita abaixo. Teste os movimentos manual e aleatório dentro dos limites calibrados.
8. **Parar / desativar** corta o PWM dos três canais. Isso não corta a alimentação elétrica nem garante que um servo digital remova o torque.

A senha do AP/portal é exibida somente no serial, não por um endpoint do portal. O AP de recuperação permanece disponível mesmo com STA conectado. A XIAO não fornece acesso à internet pelo AP.

## Calibração visual e NVS

A ideia do projeto [ManlyMorgan/Animatronic-Eye](https://github.com/ManlyMorgan/Animatronic-Eye/tree/main/03_calibration/calibration) é preservada: centralizar os servos antes de montar, descobrir os extremos mecânicos de cada eixo e registrar centro/limites, além de aberta/fechada da pálpebra. Esta implementação é independente e usa pulsos em microssegundos, NVS e portal em lugar de Arduino, entrada serial e anotações no código.

1. Desative os servos antes de editar ou aplicar a calibração; o firmware rejeita alterações enquanto estão ativados.
2. Ajuste mínimo, centro e máximo de X/Y em passos pequenos, por exemplo 5–10 µs. O intervalo elétrico permitido é 500–2500 µs; o intervalo mecânico real normalmente é menor.
3. Use **Aplicar sem gravar** para testar uma configuração temporária.
4. Ative os servos e use os botões X/Y mínimo, centro e máximo. Observe o mecanismo; interrompa antes de atingir batentes.
5. Repita o procedimento para refinar os valores.
6. Desative os servos e clique **Gravar na NVS**. Reinicie e confirme que os valores foram recuperados.
7. Para a pálpebra, configure aberta/fechada e marque Habilitar pálpebra antes de gravar. O slider varia de 0 (aberta) a 1 (fechada).

Cada eixo exige `mínimo < centro < máximo`. A inversão troca a direção normalizada sem alterar os limites elétricos. A interpolação é separada em torno do centro, aceitando cursos assimétricos. Movimentos são limitados aos extremos calibrados.

Calibração temporária desaparece ao reiniciar. Calibração salva e Wi-Fi persistem; câmera, modo, ativação, parâmetros de movimento e posições começam no estado padrão a cada boot. Erros na NVS não provocam apagamento automático: uma calibração inválida usa os valores padrão com os servos desativados; falha na inicialização da NVS interrompe o boot.

## Câmera e tracking

Depois do teste manual, desligue a alimentação para montar o módulo Sense. Ligue **Câmera ligada** no portal. O estado informa erro de inicialização quando a câmera ou PSRAM não está disponível; o controle manual continua utilizável.

A captura usa RGB565, 320×240 e um framebuffer em PSRAM. O modelo MSR/MNP do componente oficial `human_face_detect` detecta rostos; o maior retângulo é selecionado. O centro produz coordenadas normalizadas em [-1, 1]: X cresce para a direita da imagem e Y para baixo. O portal exibe essas coordenadas sobre o desenho do olho, sem streaming de vídeo.

Selecione **Seguir rosto** para habilitar tracking. A câmera precisa estar ligada. Ajuste a inversão de cada servo para que o movimento reduza o erro de posição do rosto. Desligar a câmera retorna ao modo manual.

- Deadband padrão: 0,08 em coordenadas da imagem.
- Resposta padrão: 0,12 por atualização de 20 ms; valores menores suavizam mais.
- Velocidade máxima padrão: 0,8 unidade normalizada/s.
- Sem rosto, com câmera indisponível ou dados com mais de 600 ms, mantém a posição.
- Manual e aleatório usam a mesma suavização, limite de velocidade e limites mecânicos.
- A pálpebra é controlada separadamente e não participa da detecção.

A taxa real de detecção depende do sensor, iluminação e carga do processador. Não há reconhecimento de identidade.

## Wi-Fi e portal

O portal salva SSID e senha na NVS e reinicia. São aceitos SSIDs de 1–32 bytes, senha vazia para rede aberta ou 8–63 bytes para senha WPA/WPA2. A STA usa DHCP e tenta reconectar a cada 5 segundos após desconexão. O AP protegido permanece disponível para corrigir uma senha incorreta ou falha do roteador.

O DNS cativo responde consultas IPv4 no AP apontando para 192.168.4.1; requisições HTTP de detecção recebem redirecionamento. Sistemas que exigem HTTPS podem não abrir o portal automaticamente. Na LAN, use o IP atribuído pelo roteador.

Portal e APIs exigem autenticação HTTP Basic. Alterações exigem também `X-Eye-Request: 1`; não há CORS permissivo. **Use em rede local confiável:** HTTP não cifra a senha nem o upload na LAN. Não exponha a porta 80 à internet.

## OTA e rollback

Há dois slots de aplicativo com 0x3e0000 bytes cada (aproximadamente 3,88 MiB). O modelo facial está incorporado ao aplicativo e é atualizado junto com ele.

1. Compile uma nova versão com a mesma tabela de partições e target.
2. Selecione `build/animatronic_eye.bin` no portal e envie.
3. O dispositivo desativa os servos e solicita parada da câmera, escreve no slot inativo, valida a imagem e só então muda a partição de boot.
4. Após reiniciar, valida NVS, inicialização dos drivers, tarefas, Wi-Fi e servidor HTTP; aguarda 10 segundos e verifica que a tarefa de controle está ativa antes de confirmar a imagem.
5. Se houver falha/reset antes da confirmação, o bootloader restaura o aplicativo anterior válido no próximo boot.

Upload incompleto ou imagem inválida não altera a partição de boot. O rollback depende de haver uma versão anterior válida; não protege o primeiro flash via USB nem substitui teste mecânico. A confirmação não depende de câmera conectada ou conexão com o roteador. Bootloader, NVS, e calibração não são revertidos pela troca de slot; mantenha compatibilidade de dados em versões futuras.

## Arquitetura

| Componente | Responsabilidade |
| --- | --- |
| `servo` | PWM LEDC e validação da pinagem/pulsos |
| `eye_control` | Tarefa de controle a 50 Hz, manual/aleatório/tracking, ativação |
| `calibration` | Validação, interpolação min/center/max, gravação atômica NVS |
| `camera` | Inicialização sob demanda, captura e detecção MSR/MNP |
| `tracking` | Deadband, suavização, velocidade e reação à perda do rosto |
| `wifi` | STA, AP de recuperação, credenciais e DNS cativo |
| `web` | Portal embarcado e APIs autenticadas |
| `ota` | Upload binário, validação, troca de slot e confirmação |
| `core` | Estado compartilhado protegido por mutex recursivo |
| `main` | Inicialização e confirmação da saúde após OTA |

As operações de inferência e rede ficam fora do mutex do movimento. O modelo e o framebuffer são liberados ao desligar a câmera. Apenas a tarefa de controle escreve nos servos.

APIs: `GET /api/status`; `POST /api/control`, `/api/calibration`, `/api/wifi` recebem JSON; `POST /api/ota` recebe o binário bruto. O próprio portal é um exemplo completo dos formatos.

## Validação

Veja [VALIDATION.md](VALIDATION.md) para as verificações realizadas e o roteiro de bancada. Build limpo não equivale a validação elétrica/mecânica na placa.

Referências: [XIAO ESP32-S3](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/), [esp32-camera](https://github.com/espressif/esp32-camera), [ESP-DL](https://github.com/espressif/esp-dl), [OTA no ESP-IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/ota.html).
