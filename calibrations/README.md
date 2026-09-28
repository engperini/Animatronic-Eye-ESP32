# Backup de calibração

`xiao-744dbd81b13c.json` contém a calibração ativa exportada de `/api/status` em 28/09/2026, após o usuário informar que a gravou na NVS. A API consulta o estado em RAM; esta exportação não realizou uma leitura independente da NVS nem reiniciou a placa.

Os pulsos são em microssegundos. O arquivo não contém SSID ou senha. Ele é específico desta montagem; não deve ser aplicado automaticamente em outro mecanismo.

O firmware não carrega este arquivo durante o build. Para restaurar, desative os servos, copie os valores para a calibração no portal e clique em **Gravar na NVS**.

Também é possível restaurar pelo PowerShell, ajustando o endereço IP:

```powershell
$portal = 'http://192.168.0.19'
$calibration = Get-Content './calibrations/xiao-744dbd81b13c.json' -Raw | ConvertFrom-Json
$calibration | Add-Member -NotePropertyName persist -NotePropertyValue $true
Invoke-RestMethod "$portal/api/control" -Method Post -ContentType 'application/json' -Headers @{'X-Eye-Request'='1'} -Body '{"armed":false,"mode":"manual"}'
Invoke-RestMethod "$portal/api/calibration" -Method Post -ContentType 'application/json' -Headers @{'X-Eye-Request'='1'} -Body ($calibration | ConvertTo-Json -Depth 5)
```

A restauração mantém os servos desativados. Confira os valores antes de ativá-los.
