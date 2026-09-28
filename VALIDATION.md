# Validação

## Ambiente

- Windows, ESP-IDF v5.5 (5.5.0), Python 3.13.1.
- Target esp32s3, GCC 14.2.0, build Ninja.
- Componentes resolvidos em dependencies.lock.
- Flash 8 MB, PSRAM octal, rollback habilitado, duas partições OTA.
- Câmera, tracking e saídas PWM desligados por padrão.

## Verificações locais

- Compilação de todos os componentes, modelo facial, bootloader e aplicativo.
- Verificação automática de tamanho de partição pelo ESP-IDF.
- Verificação de sintaxe JavaScript do portal.
- Execução do JavaScript com DOM/API simulados: inicialização, ativação/parada, centro, calibração temporária/persistente, câmera, movimento manual, cabeçalhos e exibição de erros — aprovada.
- Verificação de espaços em branco e arquivos versionados antes de publicar.

Build concluído sem erros ou avisos do compilador. Imagem: 0x21d990 bytes; 45% de espaço livre em cada slot OTA. SHA-256 do binário validado: `bb63df6eaea83f6191121ef78f39fa42456094bcc37edb7bb7d4e4e2a04d62ba`.

## Flash USB e inicialização verificados

- Placa detectada na COM8: ESP32-S3 revisão v0.2, flash de 8 MB e PSRAM de 8 MB.
- Bootloader, tabela de partições, dados OTA iniciais e aplicativo gravados por USB; hashes verificados pelo esptool.
- Log de boot confirma ESP-IDF v5.5, aplicativo 1.0.0, teste de PSRAM aprovado e AP/DHCP iniciado em 192.168.4.1.
- Após 10 segundos: `Startup self-test complete; outputs disarmed; camera OFF`.
- Nenhum servo foi ativado e a câmera não foi inicializada nesta verificação. As credenciais individuais não são publicadas no repositório.

## Testes de bancada pendentes

Flash e inicialização foram verificados pelo serial. Os testes funcionais de portal, mecanismo, câmera e OTA ainda devem ser executados na montagem:

1. Sem módulo de câmera: boot, AP, login, servos inicialmente sem PWM.
2. Manual: centro e extremos de X/Y; respeitar limites com movimento suavizado.
3. Calibração: rejeitar mínimo >= centro, centro >= máximo e pulsos fora de 500–2500 µs.
4. Tentar alterar calibração com servos ativados: deve recusar.
5. Salvar calibração, reiniciar e conferir todos os valores; aplicar temporariamente e confirmar que reiniciar descarta.
6. Pálpebra desabilitada: sem PWM no GPIO 4; habilitada: aberta/fechada e inversão de valores conforme montagem.
7. Aleatório: observar repetidos movimentos sem exceder os limites.
8. Wi-Fi correto: STA conecta; senha incorreta/roteador desligado: AP continua acessível.
9. Camera ausente: ligar câmera deve informar erro sem impedir controle manual.
10. Com Sense/PSRAM: ligar/desligar câmera repetidamente; verificar detecção, direção do tracking e ausência de fuga de memória.
11. Tracking: deadband, limites de velocidade, rosto perdido e desligamento de câmera.
12. Segurança HTTP: sem credenciais deve retornar 401; POST sem X-Eye-Request deve retornar 403.
13. OTA: imagem válida reinicia, conserva NVS e confirma após autoteste; arquivo inválido ou upload interrompido preserva aplicativo atual.
14. Rollback: em firmware de teste, provoque reset antes da confirmação; o boot seguinte deve recuperar a versão válida anterior.
15. Teste prolongado com a fonte definitiva: servos, Wi-Fi e detecção ativos; monitorar reset por alimentação e aquecimento.

Teste de rollback deve ser feito com acesso ao USB e uma imagem funcional disponível. Não altera automaticamente a calibração ou a fonte de alimentação.
