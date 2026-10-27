# Teste de rastreamento com câmera real no OBS

Esta é uma verificação manual de execução no OBS para o caminho de rastreamento
real de pessoas. Um CTest verde prova apenas o comportamento unitário/layout;
não prova que o OBS carregou o módulo, que um frame foi copiado ou que uma
pessoa foi detectada. Use um computador com OBS Studio, backend gráfico ativo,
câmera, bibliotecas de execução OpenCV >= 4.5 e o plugin `church-director`
instalado no caminho de módulos do OBS. Não é necessário baixar modelos nem
usar uma conta na nuvem.

## Preparação e evidências

1. Inicie o OBS com um **perfil OBS limpo** e uma coleção de cenas vazia. Anote
   a versão do OBS, o sistema operacional, o commit do plugin, o preset de
   build, o prefixo de instalação, o nome da câmera e se o plugin carregou.
   Guarde o log do OBS desta inicialização.
2. Verifique as permissões da plataforma antes de adicionar a fonte. No
   **Windows**, abra Configurações → Privacidade e segurança → Câmera e permita
   o acesso à câmera e aos aplicativos de desktop para o OBS. No **macOS**, abra
   Ajustes do Sistema → Privacidade e Segurança → Câmera, ative o OBS e reinicie
   quando solicitado. No **Linux**, verifique as permissões de dispositivo/portal:
   permita que o usuário do OBS abra o dispositivo V4L2 e conceda a permissão do
   portal de câmera quando o OBS for sandboxed/Flatpak. Registre erros de
   dispositivo ou portal negados como indisponíveis.
3. Adicione uma **fonte de câmera** funcional e confirme que a prévia sem filtro
   está visível. Use iluminação estável e deixe uma ou mais pessoas de pé,
   totalmente visíveis e em tamanho útil. Salve uma captura da fonte e das
   configurações do filtro.
4. Abra os Filtros da fonte e adicione `church_director_filter`. Mantenha
   **Real person tracking** selecionado; este é o modo padrão. Não ative
   Synthetic target mode neste teste. Deixe **Tracking armed** desligado até o
   enquadramento estar pronto.
5. Em Configurações → Atalhos do OBS, atribua uma tecla à ação **Toggle Church
   Director tracking** listada na fonte pai que contém o filtro. Se quiser,
   associe a mesma tecla ao botão do controle PTZ físico ou ao software de macro
   do Windows. Arme somente quando estiver pronto e desarme imediatamente antes
   de assumir o controle manual da PTZ. Um controle que ofereça apenas VISCA,
   serial ou IP precisa de um adaptador específico, fora deste escopo.
6. Confirme que o formato da fonte é um dos caminhos suportados: RGBA, BGRA,
   BGRX, BGR3, NV12 ou I420. O frame enviado deve ter planos, dimensões e
   strides válidos. Frames incompatíveis ou malformados devem falhar de forma
   segura, sem derrubar o OBS; registre o formato e a mensagem relevante do log.

## Comportamento esperado

7. Pressione a tecla atribuída para armar o rastreamento. Observe a prévia por
   pelo menos 30 segundos. Uma pessoa visível deve produzir detecção e movimento
   limitado de pan/crop, mantendo o alvo no enquadramento; espere movimento de
   zoom somente quando o enquadramento exigir. Grave um trecho curto ou faça
   capturas com horário no início, durante qualquer zoom e perto do fim. A prévia
   não pode ficar preta, congelar ou mostrar pixels fora dos limites da fonte.
8. Com várias pessoas visíveis, mantenha a cena estável por pelo menos 15
   segundos. A primeira pessoa selecionada deve permanecer estável, sem trocar
   para alguém distante com maior confiança. O bloqueio espacial não é
   reconhecimento facial; para escolher outra pessoa, desarme e arme novamente
   depois de enquadrar o novo alvo. Registre qual pessoa foi selecionada e
   qualquer troca.
9. Faça todas as pessoas saírem do enquadramento por pelo menos 2 segundos. O
   rastreamento pode prever por pouco tempo, mas deve voltar ao movimento neutro
   quando a observação ficar desatualizada; não pode continuar fazendo pan sem
   limite. Registre o intervalo sem pessoas e o comportamento de retorno.
10. Pressione a tecla para desarmar e confirme que a fonte de câmera sem alteração
    reaparece e que o trabalho de rastreamento parou. Arme novamente e confirme
    que a detecção volta. Depois, **remova** o filtro e **adicione-o novamente**.
    Repita o ciclo de armar/desarmar e desativar/remover/adicionar novamente
    **três vezes**. Em cada ciclo, confirme que o OBS continua responsivo, que a
    prévia volta à fonte inteira ao desarmar/remover e que um filtro novo começa
    desarmado.

## Logs e resultados indisponíveis

Guarde o log exato do OBS (Ajuda → Arquivos de log → Ver arquivo de log atual),
a versão/build do OBS, o commit do plugin, os caminhos do módulo e dos arquivos
de efeito/locale instalados, as configurações da câmera/fonte, a captura do
filtro, a captura da fonte e o vídeo ou as imagens com horário. Registre o
resultado de cada ciclo e cada aviso/erro do Church Director. Um teste aprovado
exige detecção e movimento visíveis de pan/crop, zoom somente quando o
enquadramento exigir e encerramento repetido sem erros; a evidência unitária/
layout não substitui o teste no OBS.

Se o teste não puder ser executado, marque-o como **indisponível**, não como
aprovado. Informe a verificação e o escopo exatos, por exemplo: `executável do
OBS ausente`, `fonte de câmera indisponível`, `plugin não compilado porque
libobs ou OpenCV estavam ausentes`, `biblioteca de execução do OpenCV ausente`,
`plugin não aparece nos Filtros do OBS` ou `backend gráfico indisponível`.
Anexe o comando ou observação da interface e o diagnóstico/log, com plataforma,
versão do OBS se conhecida, status do build do plugin e etapas não tentadas.
Relate separadamente as evidências de dependência/configuração/build/instalação
do plugin e o resultado do teste de execução no OBS. Não declare detecção,
pan/crop/zoom, retorno sem pessoas ou limpeza aprovados sem observá-los no OBS.
