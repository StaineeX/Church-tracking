# Câmera do iPhone usando Camo no Windows

Este teste usa um iPhone como uma fonte comum de captura de vídeo do OBS. Ele
é útil quando não há uma webcam física ou uma câmera PTZ disponível.

## Instalar e conectar

1. Instale o **Camo Studio para Windows** pela página oficial de downloads do Camo.
2. Instale o **Camo Camera** no iPhone.
3. Conecte o iPhone ao Windows por USB primeiro. Desbloqueie o telefone e aceite
   **Confiar neste computador** quando o iOS solicitar.
4. Abra o Camo Camera no iPhone e o Camo Studio no Windows. Selecione o iPhone
   e confirme que a prévia ao vivo está visível.
5. Mantenha o telefone estável, na horizontal, com a câmera traseira apontada
   para a área do culto. USB é recomendado para uma instalação fixa na igreja;
   Wi-Fi pode ser usado depois que o caminho USB funcionar.

## Adicionar a fonte no OBS

1. Em **Fontes**, adicione **Dispositivo de captura de vídeo**.
2. Selecione `Camo Camera` (o nome exato pode incluir `Camo`).
3. Use 1280x720 ou 1920x1080 a 30 FPS, de acordo com a capacidade disponível
   de USB e CPU.
4. Adicione o filtro `Church Director` a esta fonte do Camo. O filtro e o
   atalho `Toggle` devem pertencer à mesma fonte pai; um atalho atribuído a
   uma fonte `Image` diferente não controlará a fonte da câmera.
5. Deixe **Synthetic** desmarcado para o vídeo real do iPhone. Deixe o
   rastreamento desarmado até o operador estar pronto e então use a tecla
   `Toggle` configurada para armá-lo.
6. Associe a mesma tecla à macro do controle PTZ quando o operador precisar de
   um botão físico para ativar/desativar rapidamente.

O Church Director executa pan/crop/zoom virtual dentro do OBS. Ele não move
fisicamente o iPhone nem envia comandos VISCA, serial ou IP para uma cabeça PTZ.

## Verificação

- A prévia do OBS mostra o vídeo ao vivo do iPhone antes de armar o filtro.
- Pressionar a tecla de alternância uma vez registra `Church Director: tracking ARMED`.
- Pressioná-la novamente registra `Church Director: tracking DISARMED` e retorna
  a fonte ao comportamento de passagem sem alteração.
- Com uma pessoa real visível e o rastreamento armado, o filtro pode alterar o
  crop virtual. Uma imagem parada ou uma pessoa dentro da zona morta pode não
  mostrar movimento visível mesmo quando o atalho está funcionando.

## Solução de problemas

- Se `Camo Camera` não aparecer, desbloqueie o iPhone, mantenha o Camo Camera
  aberto, tente novamente a conexão USB e aceite o aviso de confiança.
- Feche outros aplicativos que possam estar usando a câmera do telefone.
- Teste USB antes do Wi-Fi. No Wi-Fi, mantenha telefone e computador na mesma rede.
- Se o log mostrar `ARMED`/`DISARMED`, mas a imagem não se mover, confirme que
  o filtro está na fonte Camo ao vivo, que **Synthetic** está desligado e que a
  pessoa está fora da zona morta. O detector não é um reconhecedor facial.

Referências oficiais:

- [Guia de câmera de smartphone do OBS](https://obsproject.com/kb/smartphone-camera-guide)
- [Downloads do Camo Studio](https://camo.com/downloads)
- [Camo Camera na App Store](https://apps.apple.com/us/app/camo-camera/id1514199064)
