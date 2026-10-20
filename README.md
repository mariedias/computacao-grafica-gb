# Café dos Gatinhos: Visualizador 3D com OpenGL moderna

Trabalho final (Atividade Vivencial, Grau B) da disciplina de Computação Gráfica (Unisinos).

O visualizador junta tudo o que foi feito ao longo do semestre em uma cena única,
no estilo diorama: um pequeno café isométrico com três gatinhos. Um gato dá voltas
no corredor, outro pula para cima da vitrine e o terceiro passeia em cima da mesa.

O programa final é o `src/Visualizador.cpp`.

## O que foi implementado

- **Leitura de vários OBJ com vários grupos.** Cada `usemtl` do `.obj` começa um grupo
  novo, com o seu próprio VAO e o seu material. O café, por exemplo, tem 8 grupos, cada
  um com a sua textura e os seus coeficientes. Os OBJ estão triangulados e trazem
  normais (`vn`) e coordenadas de textura (`vt`).
- **Materiais do `.mtl`.** De cada `newmtl` são lidos `Ka`, `Kd`, `Ks`, `Ns` e `map_Kd`.
  Esses valores vão para o shader como uniforms antes de cada grupo ser desenhado.
  Materiais sem textura usam uma textura branca 1×1, e aí a cor vem só do `Kd`.
- **Iluminação de Phong** com três fontes de luz (key, fill e back), atenuação pela
  distância e luz ambiente. Dá para ligar e desligar cada luz, mudar a intensidade e
  mudar a luz ambiente pelo teclado.
- **Arquivo de configuração da cena** (`assets/cena.txt`). Nele ficam os objetos com as
  transformações iniciais, as trajetórias, as luzes e a câmera (posição, orientação e
  frustum).
- **Câmera em primeira pessoa** (`src/Camera.h`), controlada pelo teclado e pelo mouse.
- **Seleção e transformação** de objetos: translação, rotação e escala uniforme.
- **Animação por curvas de Bézier cúbicas.** A trajetória é uma sequência de Béziers
  emendadas e fechada, percorrida com velocidade constante. O objeto vira na direção do
  movimento, usando a derivada da curva.

## Setup

Precisa de CMake (3.10 ou mais novo) e de um compilador C++17. Usamos o MSYS2 com o
VS Code no Windows. O CMake baixa sozinho a **GLFW 3.4**, a **GLM** e a **stb_image**.
A **GLAD** (OpenGL 3.3 Core) já está no repositório, em `include/glad` e `Common/glad.c`.

```bash
git clone <link-do-repositorio>
cd <pasta-do-projeto>
cmake -S . -B build
cmake --build build
```

O programa precisa ser executado de dentro da pasta `build`, porque os caminhos dos
arquivos são relativos (`../assets/`):

```bash
cd build
./Visualizador        # no Windows: .\Visualizador.exe
```

## Controles

| Tecla             | O que faz                                                                      |
| ----------------- | ------------------------------------------------------------------------------ |
| W / A / S / D     | move a câmera (frente / esquerda / trás / direita)                             |
| F / Q             | sobe / desce a câmera                                                          |
| Mouse / scroll    | olha ao redor / zoom (muda o FOV)                                              |
| TAB / Shift + TAB | seleciona o próximo / o anterior gato (o selecionado fica amarelado)           |
| T / R / E         | modo translação / rotação / escala                                             |
| X / Y / Z         | aplica a transformação no eixo escolhido (Shift inverte o sentido)             |
| Setas             | translação em X e Y                                                            |
| U                 | liga e desliga a escala uniforme (começa ligada)                               |
| V                 | modo de exibição: completo → só material → só textura → só ka → só kd → só ks  |
| 1 / 2 / 3         | liga e desliga a luz key / fill / back                                         |
| L                 | escolhe a luz que vai ser ajustada                                             |
| + / -             | aumenta / diminui a intensidade da luz escolhida                               |
| I / Shift + I     | aumenta / diminui a luz ambiente (Ia)                                          |
| M                 | inicia / pausa a animação                                                      |
| B                 | mostra / esconde as curvas das trajetórias (a do selecionado fica em vermelho) |
| Clique esquerdo   | adiciona a posição do objeto selecionado como ponto de controle                |
| C                 | apaga a trajetória do objeto selecionado                                       |
| G                 | imprime a trajetória do selecionado no formato do `cena.txt`                   |
| Espaço            | reseta a cena (objetos, luzes, câmera e animação)                              |
| P                 | malha preenchida / wireframe                                                   |
| ESC               | fecha o programa                                                               |

## Arquivo de configuração da cena

O `assets/cena.txt` é um arquivo de texto. Cada linha começa com uma palavra-chave, e
as linhas com `#` são comentários:

```
camera     x y z  yaw pitch  fov near far  velocidade
ambiente   Ia
atenuacao  Kc Kl Kq
luz        nome  x y z  r g b  intensidade
objeto     nome  arquivo.obj  tx ty tz  rx ry rz  escala
ponto      x y z      # ponto de controle da trajetória do último objeto
fixo                  # o último objeto é cenário: não pode ser selecionado
```

O parser (`carregarCena`) lê uma linha por vez com `getline` e separa as palavras
com `istringstream`, do mesmo jeito que o leitor de OBJ. Se o mesmo `.obj` aparece em
mais de um objeto, ele só é lido uma vez. Da mesma forma, cada imagem de textura só vai
para a GPU uma vez, mesmo que vários grupos a usem (`texturasCarregadas`).

### Trajetórias com Bézier

Os pontos de controle são lidos de 3 em 3. O trecho `i` usa `P[3i]`, `P[3i+1]`,
`P[3i+2]` e `P[3i+3]`:

```
B(t) = (1-t)³ P0 + 3(1-t)² t P1 + 3(1-t) t² P2 + t³ P3,    t ∈ [0, 1]
```

Essa é a "combinação de várias curvas cúbicas" (Bézier path) do material do M6: em vez
de uma única curva de grau alto, a trajetória é feita de vários trechos cúbicos. Como o
`P3` de um trecho é o `P0` do seguinte, cada trecho novo só acrescenta 3 pontos. A curva
passa pelos pontos `P0, P3, P6, ...`; os outros dois de cada trecho só "puxam" a curva.

Para não haver um "bico" na emenda, o ponto anterior à emenda, a emenda e o ponto
seguinte ficam alinhados (garantia de continuidade da Bézier, também do material do M6).
Nas trajetórias do `cena.txt` isso vale em todas as emendas, menos nas aterrissagens do
gato que pula: ali a direção muda de propósito (ele desce e passa a andar no chão), mas
continua alinhada no plano xz, então o gato não "gira" de repente.

O último índice é calculado com `% n`. Assim, quando o número de pontos é múltiplo de
3, o último trecho termina no primeiro ponto e a curva fica fechada. Para a velocidade
ser constante, o `t` avança `velocidade · dt / |B'(t)|` a cada quadro. Para o gato
olhar para onde anda, a rotação em y é `atan2(B'.x, B'.z)`.

## Onde está cada parte no código (`src/Visualizador.cpp`)

| Parte                            | Onde                                                                     |
| -------------------------------- | ------------------------------------------------------------------------ |
| Parser do arquivo de cena        | `carregarCena()`                                                         |
| Leitura do OBJ (grupos, v/vt/vn) | `loadOBJ()` e `criarVAO()`                                               |
| Leitura do MTL                   | `loadMTL()`                                                              |
| Cálculo de iluminação (Phong)    | `fragmentShaderSource`, laço das luzes                                   |
| Passagem de uniforms             | `main()`, laço principal: câmera, luzes e material de cada grupo         |
| Matriz Model                     | `getModelMatrix()`                                                       |
| Matrizes View e Projection       | `Camera::getViewMatrix()` e `Camera::getProjectionMatrix()` (`Camera.h`) |
| Matriz das normais               | `vertexShaderSource` (`transpose(inverse(model))`)                       |
| Curva de Bézier e animação       | `bezier()`, `bezierDerivada()` e `atualizarTrajetoria()`                 |
| Atalhos de teclado               | `key_callback()`                                                         |

## Assets

| Modelo                      | Autor          | Licença   | Link                  |
| --------------------------- | -------------- | --------- | --------------------- |
| FREE_ Isometric_ Cafe_remix | HiQ3D          | CC BY 4.0 | https://skfb.ly/p9uFZ |
| Chonky Cat Trio             | Kanna-Nakajima | CC BY 4.0 | https://skfb.ly/oOTtn |

**Créditos (atribuição exigida pela licença):**

- "Chonky Cat Trio" (https://skfb.ly/oOTtn) by Kanna-Nakajima is licensed under
  Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).
- "FREE* Isometric* Cafe_remix" (https://skfb.ly/p9uFZ) by HiQ3D is licensed under
  Creative Commons Attribution (http://creativecommons.org/licenses/by/4.0/).

Os modelos foram modificados: convertidos de GLB para OBJ + MTL, com as peças do café
juntadas, os gatos separados e reposicionados e os coeficientes dos materiais
reescritos (detalhes abaixo).

**Processamento prévio (Blender).** Os modelos foram baixados do Sketchfab em GLB e
convertidos para OBJ + MTL no Blender:

- **Importação** do GLB (File → Import → glTF 2.0) e extração das texturas que vêm
  dentro do arquivo (File → External Data → Unpack Resources).
- **Café:** todas as peças foram juntadas em um objeto só (Ctrl+J). Assim o OBJ sai
  com um grupo por material (8 grupos), em vez de um grupo por peça (231).
- **Gatos:** a base quadrada que vinha no arquivo foi apagada. Cada gato foi tirado da
  hierarquia do Sketchfab mantendo a transformação (Alt+P → Clear and Keep
  Transformation, Ctrl+A → All Transforms) e teve a origem colocada no centro da base
  (y = 0), para a rotação e a escala acontecerem em torno do próprio gato e ele andar
  sobre o piso. Cada gato foi exportado em um OBJ separado.
- **Exportação** em Wavefront OBJ com Forward -Z / Up Y, malha triangulada, normais,
  coordenadas de textura e materiais (.mtl), com Path Mode "Copy" para levar as
  texturas junto.
- **Ajuste manual dos `.mtl`:** o GLB usa material PBR, que não tem `Ka`, `Kd` e `Ks`,
  então o Blender escreve valores genéricos (por exemplo, `Kd 0.8` e `Ns` de 160 a
  721). Os coeficientes foram reescritos à mão: `Kd 1` nos materiais com textura,
  potes de vidro mais especulares (`Ks 0.8`), madeira e parede foscas e lâmpadas e
  janelas com `Ka 3`, para parecerem acesas. Nos gatos também foi adicionada a linha
  `map_Kd`, que o Blender não escreveu.

## Referências

**Computação gráfica e OpenGL**

- Material de apoio dos Módulos 1 a 6 da disciplina: buffers, formato OBJ, texturas,
  iluminação, câmera sintética e curvas paramétricas (em especial _Aula 7 – Curvas
  Paramétricas_ e _M6 – Aprofundamento_: Bézier path e garantia de continuidade)
- GORTLER, Steven J. _Foundations of 3D Computer Graphics_. MIT Press, 2012.
- Capítulo 11, _Curves and Surfaces_ (material de apoio do M6)
- Pomax. _A Primer on Bézier Curves_. https://pomax.github.io/bezierinfo/
- LearnOpenGL (https://learnopengl.com/): Hello Triangle, Shaders, Textures,
  Transformations, Coordinate Systems, Camera, Basic Lighting, Materials e
  Multiple Lights
- Documentação da OpenGL: https://registry.khronos.org/OpenGL-Refpages/gl4/
- Formato Wavefront OBJ / MTL (Paul Bourke): https://paulbourke.net/dataformats/obj/

**Bibliotecas**

- GLFW: https://www.glfw.org/docs/latest/
- GLM: https://github.com/g-truc/glm
- GLAD: https://glad.dav1d.de/
- stb_image: https://github.com/nothings/stb

**Blender (conversão dos modelos)**

- Extração das texturas embutidas (Packed Data → Unpack Resources):
  https://docs.blender.org/manual/en/latest/files/blend/packed_data.html
- VERSLUIS, Jay. _Exporting all textures from a Blend File_, 2023:
  https://www.versluis.com/2023/07/exporting-all-textures-from-a-blend-file/
- Hierarquia de objetos (Parent → Clear Parent / Clear and Keep Transformation):
  https://docs.blender.org/manual/en/latest/scene_layout/object/editing/parent.html
- Aplicar transformações (Apply → All Transforms / Location):
  https://docs.blender.org/manual/en/latest/scene_layout/object/editing/apply.html
- Origem do objeto (Set Origin → Origin to Geometry):
  https://docs.blender.org/manual/en/latest/scene_layout/object/origin.html
- Juntar objetos (Join, Ctrl+J):
  https://docs.blender.org/manual/en/latest/scene_layout/object/editing/join.html
- Exportação Wavefront OBJ: https://docs.blender.org/manual/en/latest/files/import_export/obj.html
- How to Import a GLB into Blender (Fast & Simple Tutorial):
  https://youtu.be/V1q5w6onuCE?si=aYJ6E-8PEVJ--1Ko
- How to Convert Epic Games Reality Scan GLB to OBJ in Blender (Photogrammetry Export):
  https://youtu.be/Lj1Z2XmpOM0?si=QaJDcj5ZM_mBAdYM

**Modelos 3D**

- Sketchfab: https://sketchfab.com/ (créditos completos na seção Assets)

---

Alunas: Eduarda Fernandes e Maria Eduarda Dias
