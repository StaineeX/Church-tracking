<div align="center">
  <img src="assets/church-director-logo.svg" alt="Logo de câmera e cruz do Church Director" width="240">
  <h1>Church Director para OBS</h1>
  <p>Enquadramento automático, controlado pelo operador, para transmissões de cultos.</p>
  <p>
    <a href="docs/smoke-tests/obs-real-camera.md">Teste com câmera real</a> ·
    <a href="docs/smoke-tests/iphone-camo-windows.md">iPhone + Camo</a> ·
    <a href="docs/development/building.md">Guia de compilação</a>
  </p>
</div>

O Church Director é um filtro de vídeo do OBS Studio que detecta uma pessoa na fonte da câmera e mantém o alvo dentro de um enquadramento virtual limitado. O caminho de rastreamento real copia os frames compatíveis do OBS para um worker assíncrono, usa o detector de pessoas HOG integrado ao OpenCV e controla movimentos suaves de pan, crop e zoom.

| Área | Comportamento atual |
| --- | --- |
| Rastreamento | Rastreamento real de pessoas por padrão, com modo de alvo sintético para diagnósticos determinísticos |
| Segurança | O rastreamento começa desarmado e deve ser ativado deliberadamente pelo atalho do OBS ou pela opção **Tracking armed** |
| Movimento da câmera | Pan/crop/zoom virtual dentro do OBS; não envia comandos para uma PTZ física |
| ID do plugin | `church_director_filter` |
| Licença | GNU GPL v2 ou posterior |

## O que o Church Director faz

- Encontra uma pessoa visível na fonte e acompanha o alvo bloqueado sem trocar imediatamente para uma pessoa mais distante com maior confiança.
- Aplica movimentos limitados de pan, crop e zoom, preservando uma zona morta e um retorno neutro quando o rastreamento não está disponível.
- Mantém a detecção desligada enquanto está desarmado, para que o operador escolha exatamente quando o enquadramento automático ficará ativo.
- Aceita frames RGBA, BGRA, BGRX, BGR3, NV12 e I420 quando planos, dimensões e strides são válidos; frames malformados ou incompatíveis falham de forma segura, retornando ao rastreamento neutro.

### Importante: este é um PTZ virtual

O plugin atual altera o enquadramento do vídeo dentro do OBS. Ele **não** movimenta uma câmera PTZ física. Um controle PTZ pode ser usado como botão rápido de ativar/desativar quando sua macro envia o mesmo atalho configurado no OBS, mas o movimento físico continua manual até existir uma integração PTZ separada.

## Início rápido no Windows e no OBS

### 1. Adicione uma fonte de câmera

1. Abra o OBS e adicione **Dispositivo de captura de vídeo** à cena.
2. Selecione a câmera, a placa de captura ou o dispositivo `Camo Camera`.
3. Confirme que a fonte mostra uma imagem estável antes de adicionar o filtro.

Para usar um iPhone como câmera, siga o [guia do iPhone com Camo no Windows](docs/smoke-tests/iphone-camo-windows.md). O Camo Studio expõe o telefone como um dispositivo normal de captura de vídeo do OBS.

### 2. Adicione o Church Director à mesma fonte

1. Clique com o botão direito na fonte da câmera e escolha **Filtros**.
2. Adicione o filtro de efeito **Church Director**.
3. Deixe **Synthetic** desmarcado para uma câmera real.
4. Deixe **Enabled** marcado e comece com os valores padrão de movimento.

O filtro precisa estar na mesma fonte do OBS que contém os frames da câmera. Uma fonte de navegador separada ou uma fonte de outra cena não fornecerá frames ao detector.

### 3. Configure o atalho de rastreamento

1. Abra **Configurações → Atalhos** no OBS.
2. Encontre **Toggle Church Director tracking**.
3. Escolha uma tecla fácil de alcançar, mas difícil de apertar por acidente.
4. Se quiser, associe o mesmo atalho a um botão do controle PTZ físico ou ao software de macros do Windows.

O rastreamento começa **desarmado**. Pressione o atalho uma vez para armá-lo e observe o status/log; pressione novamente para desarmar antes de assumir o controle manual da câmera. O filtro também oferece a opção **Tracking armed** em suas propriedades para configuração e diagnóstico; deixe-a desligada quando o enquadramento automático não for desejado. O atalho controla a segurança do filtro no OBS, não movimenta a cabeça PTZ física.

## Teste sem câmera

É possível validar os controles do filtro e o motor de movimento sem ter uma câmera:

1. Adicione uma imagem ou outra fonte de vídeo estável ao OBS.
2. Adicione o filtro Church Director.
3. Ative **Synthetic** e mantenha **Enabled** marcado.
4. Use o atalho de rastreamento para armar e desarmar o filtro.
5. Confirme que a prévia responde ao alvo sintético e que, ao desarmar, volta ao comportamento neutro de passagem da fonte.

O modo sintético é um caminho de diagnóstico determinístico. Ele prova que o filtro, o atalho e o movimento virtual estão conectados; não prova que uma câmera real conseguirá detectar uma pessoa. Use o [teste com câmera sintética](docs/smoke-tests/obs-synthetic-camera.md) para seguir a lista completa.

## Operação segura

| Estado | O que esperar | Ação do operador |
| --- | --- | --- |
| Desarmado | A fonte passa sem alteração; nenhum frame é copiado e nenhum detector é executado | Use este estado durante a configuração e o movimento manual da PTZ |
| Armado, alvo encontrado | A pessoa bloqueada controla o enquadramento virtual limitado | Mantenha o alvo visível e observe mudanças indesejadas na cena |
| Armado, alvo desatualizado/perdido | O movimento é mantido ou retorna ao neutro conforme a zona morta e o tempo de desatualização configurados | Desarme, corrija o enquadramento e arme novamente de forma deliberada |
| Alvo errado | O bloqueio espacial evita uma troca imediata para uma pessoa distante com maior confiança | Desarme e arme novamente quando quiser adquirir outra pessoa |

O detector HOG usa CPU e funciona melhor com pessoas visíveis, de pé e em tamanho útil. Ele não é um reconhecedor facial e pode perder pessoas ocultas, pequenas, contra a luz ou em poses incomuns. O zoom mínimo é fixo em `1.0` no motor de movimento atual; os demais controles continuam editáveis.

## Verificação e solução de problemas

- **A prévia não muda:** confirme que o filtro está na fonte da câmera, que **Enabled** está marcado e que **Synthetic** está ativo quando o teste for feito sem câmera.
- **A câmera real não acompanha:** deixe **Synthetic** desmarcado, verifique se a pessoa aparece com clareza e execute o [teste com câmera real](docs/smoke-tests/obs-real-camera.md).
- **O atalho parece não fazer nada:** confirme que existe uma associação para **Toggle Church Director tracking**, pressione a tecla com o OBS em foco e verifique se o status/log muda de `DISARMED` para `ARMED`.
- **O rastreamento troca de pessoa:** use o ciclo deliberado de desarmar e armar novamente; o bloqueio espacial foi feito para resistir a substituições distantes com maior confiança.
- **Não há câmera disponível:** use o [modo sintético](docs/smoke-tests/obs-synthetic-camera.md) para verificar controle e movimento virtual; faça o teste real quando tiver uma fonte de câmera.
- **A fonte do iPhone não aparece:** conclua a [configuração do Camo com iPhone](docs/smoke-tests/iphone-camo-windows.md), prefira USB para uma conexão estável durante o culto e selecione `Camo Camera` no OBS.

O teste com câmera real registra logs do detector, movimento visível de pan/crop, zoom somente quando o enquadramento exigir, limpeza repetida e comportamento seguro diante de falhas. Uma suíte de testes unitários/layout aprovada não é um resultado de execução dentro do OBS.

## Mapa da documentação

| Guia | Use quando... |
| --- | --- |
| [Teste do Camo com iPhone](docs/smoke-tests/iphone-camo-windows.md) | Você quer usar um iPhone como câmera do OBS |
| [Teste com câmera real](docs/smoke-tests/obs-real-camera.md) | Você tem uma câmera e quer provar o rastreamento real de pessoas |
| [Teste com câmera sintética](docs/smoke-tests/obs-synthetic-camera.md) | Você precisa validar de forma determinística sem câmera |
| [Guia de compilação](docs/development/building.md) | Você precisa compilar ou instalar o plugin |

## Compilação e verificação

O preset de testes unitários sem dependências exige CMake 3.21 ou posterior, Ninja e um compilador C++20. Esses testes não exigem OBS, OpenCV, GPU, câmera, modelos ou acesso à rede. A suíte inclui a camada de estado do filtro independente do OBS e um fixture do layout de instalação.

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

O preset opcional `opencv-tests` no Linux ativa o teste do detector HOG independente do OBS. Ele exige OpenCV >= 4.5, mas não exige OBS/libobs:

```sh
cmake --preset opencv-tests
cmake --build --preset opencv-tests
ctest --test-dir build/opencv-tests -R person_detector --output-on-failure
```

Consulte o [guia de compilação](docs/development/building.md) para builds específicos de cada sistema e evidências do CI. O CI executa a suíte unitária a cada branch e pull request enviados; o job do plugin Linux registra separadamente o resultado das dependências, compilação e instalação. Uma suíte unitária/layout aprovada não equivale a um build real do plugin nem a um teste no OBS.

### Observações sobre o plugin por sistema

Use o preset `plugin-windows`, `plugin-linux` ou `plugin-macos` quando houver um gerador compatível. Instale primeiro os arquivos de desenvolvimento do OBS/libobs e o OpenCV >= 4.5 (módulos core, imgproc e objdetect). Os presets do plugin ativam `BUILD_OBS_PLUGIN=ON`; a configuração falha se o gerador, `libobs` ou OpenCV não estiver disponível. Não é necessário baixar modelos, acessar a rede ou usar credenciais de nuvem.

- No Windows e no Linux, o efeito e o locale em inglês são instalados no diretório de dados do módulo.
- No macOS, o módulo é instalado como `church-director.plugin`, com recursos em `Contents/Resources/effects` e `Contents/Resources/locale`, seguindo o layout de bundle do OBS.
- Os módulos Linux usam `${CMAKE_INSTALL_LIBDIR}/obs-plugins` por padrão, incluindo diretórios multiarch configurados. Substitua por `CHURCH_DIRECTOR_LINUX_MODULE_DESTINATION` quando o empacotamento exigir outro destino.

## Licença e atribuições

Este projeto é licenciado sob a GNU GPL versão 2 ou, a seu critério, qualquer versão posterior. Consulte [LICENSE](LICENSE) e [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) para os termos completos e as atribuições de terceiros/templates.
