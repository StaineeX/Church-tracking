# Compilar e verificar o Church Director

A suíte independente do OBS exige CMake 3.21 ou posterior, Ninja e um
compilador C++20. Ela não exige OBS, OpenCV, câmera, GPU, acesso à rede ou
arquivos de modelo. Execute a partir da raiz do repositório:

```sh
cmake --preset unit-tests
cmake --build --preset unit-tests
ctest --test-dir build/unit-tests --output-on-failure
```

Isso executa os testes principais/estado do filtro, o fixture de layout da
instalação do plugin e a verificação documental do marco. Uma aprovação prova
apenas essas verificações; não compila o módulo real do OBS nem abre o OBS.

## Teste opcional do detector OpenCV

O teste do detector sem dependências do OBS fica separado do preset unitário
normal. No Ubuntu/Linux, instale OpenCV >= 4.5 e execute o teste HOG independente
do OBS:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ libopencv-dev
cmake --preset opencv-tests
cmake --build --preset opencv-tests
ctest --test-dir build/opencv-tests -R person_detector --output-on-failure
```

O preset `unit-tests` não exige nem localiza OpenCV. O preset `opencv-tests` não
exige OBS/libobs; ele compila apenas o detector opcional e seu fixture HOG.

## Compilar o plugin do OBS

O plugin real do OBS exige arquivos de desenvolvimento do OBS Studio/libobs e
OpenCV >= 4.5 com os módulos `core`, `imgproc` e `objdetect`. O detector usa os
coeficientes HOG de pessoas integrados ao OpenCV; não é necessário baixar modelos.

No Ubuntu/Linux, instale os pacotes antes de configurar:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ pkg-config libobs-dev libopencv-dev
```

No Windows, instale os arquivos de desenvolvimento/libobs do OBS Studio, o
Visual Studio 2022 e o OpenCV 4 com um pacote CMake localizável (por exemplo,
uma instalação que forneça `OpenCVConfig.cmake`). No macOS, instale Xcode,
arquivos de desenvolvimento/libobs do OBS Studio e OpenCV 4 com um pacote CMake
localizável (por exemplo, o pacote `opencv` do Homebrew). O gerador do CMake
deve corresponder ao sistema. Nenhuma dessas plataformas precisa de arquivos de
modelo, download de modelo ou credenciais de nuvem. Use o preset correspondente:

```sh
# Ubuntu/Linux com arquivos de desenvolvimento do libobs e Ninja
cmake --preset plugin-linux
cmake --build --preset plugin-linux
cmake --install build/plugin-linux --prefix "$PWD/build/plugin-stage"
```

```powershell
# Windows com Visual Studio 2022 e pacote CMake localizável do libobs
cmake --preset plugin-windows
cmake --build --preset plugin-windows
cmake --install build/plugin-windows --config RelWithDebInfo --prefix "$PWD/build/plugin-stage"
```

```sh
# macOS com Xcode e pacote CMake localizável do libobs
cmake --preset plugin-macos
cmake --build --preset plugin-macos
cmake --install build/plugin-macos --config RelWithDebInfo --prefix "$PWD/build/plugin-stage"
```

O plugin se chama `church-director`. Uma instalação em staging bem-sucedida
contém o módulo, `effects/church-director.effect` e `locale/en-US.ini`; o teste
`plugin_layout` verifica o layout de cada plataforma usando um módulo fixture.
Um build real exige os pacotes efetivos de `libobs` e OpenCV. Se o CMake não
encontrar `libobs` ou OpenCV, ou se o gerador/compilador estiver ausente, registre
o comando, o código de saída e o diagnóstico exato como **indisponível**. Não
chame um build de fixture de build real do plugin.

O CI executa o preset unitário e o teste separado do detector OpenCV a cada
branch e pull request enviado. O job do detector instala `libopencv-dev` e
executa apenas o teste HOG independente do OBS. Um job separado no Ubuntu 24.04
instala `libobs-dev` e `libopencv-dev`, configura/compila `plugin-linux`, faz a
instalação em staging e envia logs e arquivos instalados. Esse job falha quando
a preparação das dependências, configuração, compilação ou instalação falha;
seu status e logs mostram o que ficou indisponível ou quebrado. O CI não possui
uma sessão gráfica do OBS, então seus artefatos são apenas evidências de build.
Builds do plugin para Windows/macOS são manuais até que esses ambientes tenham
dependências de desenvolvimento do OBS configuradas no CI.

## Privacidade da câmera e permissões de dispositivo

Antes do teste com câmera real, verifique as permissões da plataforma. No
Windows, abra Configurações → Privacidade e segurança → Câmera e permita o acesso
à câmera e aos aplicativos de desktop para o OBS. No macOS, abra Ajustes do
Sistema → Privacidade e Segurança → Câmera, ative o OBS e reinicie se solicitado.
No Linux, verifique as permissões de dispositivo/portal: o usuário do OBS deve
poder abrir o dispositivo de câmera V4L2 (geralmente pelo grupo `video` da
distribuição ou por uma política udev), e o OBS sandboxed/Flatpak deve receber a
permissão do portal de câmera. Registre qualquer mensagem de dispositivo ou
portal negado como uma verificação de fonte de câmera indisponível.

Para a aceitação em execução, siga o [procedimento de teste com câmera real](../smoke-tests/obs-real-camera.md). Em Configurações → Atalhos do OBS, atribua a ação `Toggle Church Director tracking` à fonte de câmera pai e associe-a ao botão físico da PTZ ou ao software de macro do Windows; o filtro começa desarmado para que o controle manual da PTZ nunca seja substituído inesperadamente. Preserve o log do OBS e as evidências visuais separadamente dos logs do CTest. O detector HOG é um detector CPU de referência: registre detecções perdidas, oclusões, pessoas pequenas ou falsos positivos em vez de tratá-los como prova de que outro modelo foi carregado.
