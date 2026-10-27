# Teste com câmera sintética no OBS

Este é um teste manual de execução no OBS. A suíte unitária/layout e o build do
plugin no CI não provam que o OBS carregou o módulo ou que a prévia se comportou
corretamente. Use um computador com OBS Studio, backend gráfico ativo e o plugin
`church-director` instalado no caminho de módulos do OBS, junto com os arquivos
de efeito e locale. Não é necessário ter câmera, modelo do detector ou conta na
nuvem.

1. Inicie o OBS com um **perfil OBS limpo** e uma coleção de cenas vazia. Anote a
   versão do OBS, o sistema operacional, o commit do plugin, o preset de build,
   o prefixo de instalação e se o plugin carregou. Não reutilize um perfil de
   transmissão. Guarde o log desta inicialização.
2. Adicione um **padrão de teste** determinístico como fonte Image do OBS: use
   um PNG local estático de 1920×1080 com grade contrastante e marcadores
   numerados nos cantos e no centro. Defina o canvas em 1920×1080 e mantenha a
   fonte Image em seu tamanho nativo; não a mova durante o teste. Preserve o PNG
   exato (ou seu checksum e uma captura) para comparar o crop visível depois.
3. Abra os Filtros dessa fonte, adicione o filtro de vídeo Church Director
   (`church_director_filter`) e mantenha **Synthetic target mode** ativado.
   Deixe Enabled ligado, mas mantenha **Tracking armed** desligado no começo.
   Guarde uma captura das configurações. Em Configurações → Atalhos do OBS,
   atribua uma tecla à ação **Toggle Church Director tracking** listada nessa
   fonte. Esta é a tecla que pode ser associada ao botão físico da PTZ ou ao
   software de macro do Windows.
4. Com o rastreamento desarmado, observe a prévia por pelo menos 5 segundos. Ela
   deve permanecer como a fonte inteira sem alteração e o log não deve mostrar
   trabalho do detector. Pressione a tecla uma vez: o filtro deve registrar
   `tracking ARMED`, adquirir o alvo sintético e produzir uma rampa inicial de
   zoom visível, seguida de pan e crop contínuos sobre o padrão fixo. Pressione
   novamente: deve registrar `tracking DISARMED`, voltar à fonte inteira e parar
   de enviar frames. A região visível deve permanecer dentro dos limites da
   fonte e o padrão nunca pode ficar preto, congelar ou mostrar área externa.
   Faça capturas antes de armar, durante a rampa, enquanto armado e depois de
   desarmar. Se o movimento estiver difícil de ver, aumente Max zoom acima de
   1.0; o Minimum zoom é fixo em 1.0 neste marco.
5. **Desative** o filtro, remova-o e **adicione-o novamente**. Repita o ciclo de
   armar/desarmar e desativar/remover/adicionar novamente **três vezes**. Após
   cada ciclo, confirme que o OBS continua responsivo, que um filtro desarmado
   mostra o frame inteiro e que um filtro novo começa desarmado. Feche o OBS
   normalmente e verifique o log em busca de erros de unload/encerramento,
   travamentos e erros de crop-effect.

Guarde o log exato do OBS desta execução (Ajuda → Arquivos de log → Ver arquivo
de log atual), a versão/build do OBS e o commit do plugin, os caminhos do módulo
e dos arquivos de efeito/locale instalados, a captura das configurações do filtro,
o arquivo/padrão de teste e o vídeo ou imagens com horário. Registre o resultado
de cada ciclo e qualquer aviso/erro do Church Director. Um resultado de execução
aprovado exige comportamento visível e encerramento repetido sem erros; um CTest
verde é apenas evidência **unitária/layout**.

Se o OBS não puder ser executado, marque o **teste de execução no OBS como
indisponível** e informe a verificação e o escopo exatos: por exemplo,
`executável do OBS ausente`, `plugin não compilado porque os arquivos de
desenvolvimento do libobs estavam ausentes`, `plugin não aparece nos Filtros do
OBS` ou `backend gráfico indisponível`. Anexe o comando ou observação da
interface e o diagnóstico/log, com plataforma, versão do OBS se conhecida,
status do build do plugin e quais etapas de 1 a 5 não foram tentadas. Não marque
pan/crop/zoom ou limpeza como aprovados sem observá-los no OBS. Se o build do
plugin estiver indisponível, relate separadamente o comando, código de saída e
diagnóstico da configuração/compilação.
