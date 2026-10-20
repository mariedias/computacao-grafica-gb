/*
 * Visualizador final (Atividade Vivencial - Grau B)
 * Cena: um café com três gatinhos (diorama)
 *
 * Junta tudo o que foi feito no semestre em um programa só:
 * - Leitura de vários .OBJ, cada um com vários grupos (usemtl), e de cada
 *   grupo o seu material (ka, kd, ks, Ns) e a sua textura (map_Kd) do .MTL;
 * - Arquivo de configuração da cena (assets/cena.txt): objetos, transformações
 *   iniciais, trajetórias, luzes e câmera (posição, orientação e frustum);
 * - Modelo de iluminação de Phong com 3 fontes de luz (key, fill e back);
 * - Câmera em primeira pessoa (Camera.h);
 * - Seleção de objetos e transformações (translação, rotação e escala);
 * - Animação por curvas de Bézier cúbicas (trajetória fechada).
 *
 * Teclas utilizadas:
 * - Seleção do objeto ==> TAB (SHIFT + TAB volta; objetos "fixo" são pulados)
 * - Modo rotação / translação / escala ==> 'R' / 'T' / 'E'
 * - Eixo da transformação ==> 'X', 'Y' e 'Z' (SHIFT inverte o sentido)
 * - Escala uniforme (liga/desliga) ==> 'U'
 * - Translação em x e y ==> setas
 * - Modo de exibição do material ==> 'V'
 *     completo -> só material (sem textura) -> só textura -> só ka -> só kd -> só ks
 * - Liga/desliga luz key, fill e back ==> '1', '2' e '3'
 * - Escolher a luz a ser ajustada ==> 'L'
 * - Intensidade da luz escolhida ==> '+' / '-'
 * - Luz ambiente (Ia) ==> 'I' aumenta / SHIFT + 'I' diminui
 * - Mover a câmera ==> W, A, S, D (Q / F descem e sobem)
 * - Olhar ao redor ==> mouse; zoom ==> scroll
 * - Iniciar / pausar a animação ==> 'M'
 * - Mostrar / esconder as curvas ==> 'B'
 * - Adicionar ponto de controle (posição do objeto selecionado) ==> clique esquerdo
 * - Apagar a trajetória do objeto selecionado ==> 'C'
 * - Imprimir a trajetória no formato do cena.txt ==> 'G'
 * - Reset da cena ==> ESPAÇO
 * - Malha preenchida / wireframe ==> 'P'
 * - Sair ==> ESC
 *
 * Alunas: Eduarda Fernandes e Maria Eduarda Dias
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Camera.h"

using namespace std;

const GLuint WIDTH = 1280, HEIGHT = 800;
const string PASTA_ASSETS = "../assets/";

// ---------------------------------------------------------------------------
// Estruturas
// ---------------------------------------------------------------------------

struct Material
{
    string nome;
    glm::vec3 ka = glm::vec3(0.2f);   // ambiente
    glm::vec3 kd = glm::vec3(1.0f);   // difusa
    glm::vec3 ks = glm::vec3(0.5f);   // especular
    float ns = 32.0f;                 // expoente especular (q)
    string textura = "";
    GLuint texID = 0;
};

// um grupo do .OBJ (tudo o que vem depois de um "usemtl"): geometria + material
struct Grupo
{
    GLuint VAO;
    int nVertices;
    Material material;
};

struct Object3D
{
    string nome;
    vector<Grupo> *grupos;  // aponta para a malha carregada (vários objetos podem usar o mesmo .OBJ)

    glm::vec3 position;   // translação
    glm::vec3 scale;      // escala
    glm::vec3 rotation;   // ângulos em graus, um por eixo

    // valores lidos do arquivo de configuração, usados no reset
    glm::vec3 posicaoInicial, rotacaoInicial, escalaInicial;

    // trajetória: pontos de controle da Bézier cúbica (de 3 em 3, curva fechada)
    vector<glm::vec3> trajetoria;
    int segmento = 0;      // qual trecho da curva está sendo percorrido
    float t = 0.0f;        // parâmetro dentro do trecho (0 a 1)
    float direcao = 0.0f;  // ângulo (em y) para o objeto olhar para onde anda
    bool orientar = true;  // se o objeto vira na direção do movimento
    bool selecionavel = true;  // false: cenário fixo, o TAB pula este objeto
};

struct Luz
{
    string nome;
    glm::vec3 posicao;
    glm::vec3 cor;
    float intensidade;
    float intensidadeInicial;  // para o reset
    bool ligada = true;
};

// ---------------------------------------------------------------------------
// Estado global
// ---------------------------------------------------------------------------

Camera camera;
Camera cameraInicial;  // guardada para o reset
float deltaTime = 0.0f, lastFrame = 0.0f;

bool animando = false;
bool mostrarCurvas = false;
float velocidade = 0.6f;  // unidades por segundo ao longo da curva

#define MAX_LUZES 3
vector<Luz> luzes;
int luzEscolhida = 0;
float Ia = 0.3f;                          // intensidade da luz ambiente
float IaInicial = 0.3f;
glm::vec3 atenuacao(1.0f, 0.09f, 0.032f); // Kc, Kl, Kq
glm::vec3 corFundo(0.97f, 0.89f, 0.87f);

map<string, vector<Grupo>> malhas;  // malhas já carregadas, pelo nome do arquivo
vector<Object3D> cena;
int selecionado = -1;  // -1: nenhum objeto selecionado (TAB seleciona)
char modo = 'T'; // 'T' -> translação, 'R' -> rotação, 'E' -> escala
bool escalaUniforme = true;

int modoExibicao = 0;
const char *nomesModos[6] = {"COMPLETO (Phong + textura)", "SO MATERIAL (sem textura)", "SO TEXTURA (sem iluminacao)",
                             "SO AMBIENTE (Ia * ka)", "SO DIFUSA (kd)", "SO ESPECULAR (ks)"};

GLuint texturaBranca = 0;  // usada quando o material não tem textura

// ---------------------------------------------------------------------------
// Shaders
// ---------------------------------------------------------------------------

const GLchar *vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec3 position;
layout (location = 1) in vec2 texc;
layout (location = 2) in vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 texCoord;
out vec3 fragPos;
out vec3 scaledNormal;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
    texCoord = texc;

    // posicao do fragmento no espaco do mundo, para calcular as direcoes da luz e da camera
    fragPos = vec3(model * vec4(position, 1.0));

    // matriz normal: ignora a translacao e corrige a escala nao uniforme
    scaledNormal = mat3(transpose(inverse(model))) * normal;
})";

const GLchar *fragmentShaderSource = R"(
#version 330 core
in vec2 texCoord;
in vec3 fragPos;
in vec3 scaledNormal;

uniform sampler2D texBuff;
uniform int isSelected;
uniform int modoExibicao;   // 0 completo, 1 sem textura, 2 so textura, 3 so ka, 4 so kd, 5 so ks
uniform int desenhaCurva;   // 1 quando esta desenhando a linha da trajetoria
uniform vec3 corCurva;

// material (vem do .mtl)
uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float q;

#define MAX_LUZES 3

uniform int nLuzes;
uniform vec3 lightPos[MAX_LUZES];
uniform vec3 lightColor[MAX_LUZES];
uniform float lightIntensity[MAX_LUZES];
uniform int lightOn[MAX_LUZES];

uniform float Ia;

uniform float Kc;
uniform float Kl;
uniform float Kq;

uniform vec3 cameraPos;

out vec4 color;

void main()
{
    if (desenhaCurva == 1)
    {
        color = vec4(corCurva, 1.0);
        return;
    }

    vec4 texColor = texture(texBuff, texCoord);

    vec3 N = normalize(scaledNormal);
    vec3 V = normalize(cameraPos - fragPos);

    vec3 ambient = Ia * ka;
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);

    for (int i = 0; i < nLuzes; i++)
    {
        if (lightOn[i] == 0) continue;   // luz desligada

        vec3 L = lightPos[i] - fragPos;
        float d = length(L);
        L = normalize(L);

        float fatt = 1.0 / (Kc + Kl * d + Kq * d * d);
        float peso = fatt * lightIntensity[i];

        float diff = max(dot(N, L), 0.0);
        diffuse += peso * kd * diff * lightColor[i];

        vec3 R = reflect(-L, N);
        float spec = pow(max(dot(R, V), 0.0), q);
        specular += peso * ks * spec * lightColor[i];
    }

    vec3 result;
    if (modoExibicao == 0)        // a textura faz o papel da cor do objeto, a especular
        result = (ambient + diffuse) * texColor.rgb + specular; // fica de fora: brilho tem a cor da luz
    else if (modoExibicao == 1)   // so os coeficientes do .mtl, sem a textura
        result = ambient + diffuse + specular;
    else if (modoExibicao == 2)   // so a textura, sem iluminacao
        result = texColor.rgb;
    else if (modoExibicao == 3)
        result = ambient;
    else if (modoExibicao == 4)
        result = diffuse;
    else
        result = specular;

    // objeto selecionado fica com um tom amarelado
    if (isSelected == 1)
        result = mix(result, vec3(1.0, 0.8, 0.3), 0.3);

    color = vec4(result, 1.0);
})";

// ---------------------------------------------------------------------------
// Funções de leitura (OBJ, MTL, textura e cena)
// ---------------------------------------------------------------------------

map<string, GLuint> texturasCarregadas;  // cada imagem vai para a GPU uma vez só

GLuint loadTexture(string filePath)
{
    // se vários grupos usam a mesma imagem, reaproveita a textura já carregada
    if (texturasCarregadas.count(filePath))
        return texturasCarregadas[filePath];

    GLuint texID; // id da textura a ser carregada

    // Gera o identificador da textura na memória
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    // Ajuste dos parâmetros de wrapping e filtering
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Carregamento da imagem usando a função stbi_load da biblioteca stb_image
    int width, height, nrChannels;
    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {
        // o OpenGL espera, por padrão, que cada linha da imagem tenha um número de
        // bytes múltiplo de 4; em RGB (3 bytes por pixel) isso nem sempre acontece
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        if (nrChannels == 3) // RGB: jpg e png sem transparência
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        else // RGBA: png com transparência
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    else
    {
        cout << "Falha ao carregar a textura " << filePath << endl;
    }

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    texturasCarregadas[filePath] = texID;
    return texID;
}

// Lê todos os materiais (newmtl) de um arquivo .MTL: ka, kd, ks, Ns e a textura
map<string, Material> loadMTL(string filePath)
{
    map<string, Material> materiais;
    string atual = "";

    ifstream arq(filePath.c_str());
    if (!arq.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
        return materiais;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "newmtl")            // começa um material novo
        {
            ssline >> atual;
            materiais[atual].nome = atual;
        }
        else if (atual.empty())
            continue;
        else if (word == "map_Kd")
            ssline >> materiais[atual].textura;
        else if (word == "Ka")
            ssline >> materiais[atual].ka.r >> materiais[atual].ka.g >> materiais[atual].ka.b;
        else if (word == "Kd")
            ssline >> materiais[atual].kd.r >> materiais[atual].kd.g >> materiais[atual].kd.b;
        else if (word == "Ks")
            ssline >> materiais[atual].ks.r >> materiais[atual].ks.g >> materiais[atual].ks.b;
        else if (word == "Ns")
            ssline >> materiais[atual].ns;
    }
    arq.close();
    return materiais;
}

// Envia o buffer (x, y, z, s, t, nx, ny, nz) de um grupo para a GPU e devolve o VAO
GLuint criarVAO(const vector<GLfloat> &vBuffer)
{
    GLuint VBO, VAO;

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // atributo 0: posição (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    // atributo 1: coordenadas de textura (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // atributo 2: vetor normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return VAO;
}

/*
 * Lê um arquivo .OBJ com um ou mais grupos. Cada "usemtl" começa um grupo
 * novo, com o seu próprio buffer de vértices e o seu material do .MTL.
 * As listas de v, vt e vn são compartilhadas por todos os grupos do arquivo
 * (os índices das faces são globais no arquivo).
 */
vector<Grupo> loadOBJ(string filePath)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;

    vector<Grupo> grupos;
    vector<vector<GLfloat>> buffers;   // um buffer por grupo
    vector<string> nomesMateriais;     // material de cada grupo
    map<string, Material> materiais;

    ifstream arqEntrada(filePath.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
        return grupos;
    }

    string diretorio = filePath.substr(0, filePath.find_last_of("/\\") + 1);

    string line;
    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "v")
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 vn;
            ssline >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (word == "mtllib")
        {
            string nomeMTL;
            ssline >> nomeMTL;
            materiais = loadMTL(diretorio + nomeMTL);
        }
        else if (word == "usemtl")   // começa um grupo novo
        {
            string nome;
            ssline >> nome;
            nomesMateriais.push_back(nome);
            buffers.push_back(vector<GLfloat>());
        }
        else if (word == "f")
        {
            if (buffers.empty())     // face antes de qualquer usemtl: grupo sem material
            {
                nomesMateriais.push_back("");
                buffers.push_back(vector<GLfloat>());
            }
            vector<GLfloat> &vBuffer = buffers.back();

            // guarda os índices (vértice, textura, normal) de cada vértice da face
            vector<glm::ivec3> faceIndices;
            while (ssline >> word)
            {
                int vi = 0, ti = 0, ni = 0;
                istringstream ss(word);
                string index;

                // cada getline avança um campo do formato v/vt/vn (o .OBJ indexa a partir de 1)
                if (getline(ss, index, '/') && !index.empty()) vi = stoi(index) - 1;
                if (getline(ss, index, '/') && !index.empty()) ti = stoi(index) - 1;
                if (getline(ss, index, '/') && !index.empty()) ni = stoi(index) - 1;

                if ((vi >= 0 && vi < (int)vertices.size()) &&
                    (ti >= 0 && ti < (int)texCoords.size()) &&
                    (ni >= 0 && ni < (int)normals.size()))
                {
                    faceIndices.push_back(glm::ivec3(vi, ti, ni));
                }
            }

            // triangulação em leque: cobre faces com mais de 3 vértices
            for (size_t i = 1; i + 1 < faceIndices.size(); i++)
            {
                glm::ivec3 tri[3] = {faceIndices[0], faceIndices[i], faceIndices[i + 1]};
                for (int k = 0; k < 3; k++)
                {
                    glm::vec3 v = vertices[tri[k].x];
                    glm::vec2 vt = texCoords[tri[k].y];
                    glm::vec3 vn = normals[tri[k].z];
                    vBuffer.insert(vBuffer.end(), {v.x, v.y, v.z, vt.s, vt.t, vn.x, vn.y, vn.z});
                }
            }
        }
    }
    arqEntrada.close();

    cout << "Modelo carregado: " << filePath << " (" << vertices.size() << " vertices)" << endl;

    // cria um VAO por grupo e carrega a textura do material de cada um
    for (size_t g = 0; g < buffers.size(); g++)
    {
        if (buffers[g].empty()) continue;

        Grupo grupo;
        grupo.VAO = criarVAO(buffers[g]);
        grupo.nVertices = buffers[g].size() / 8;  // x, y, z, s, t, nx, ny, nz

        if (materiais.count(nomesMateriais[g]))
            grupo.material = materiais[nomesMateriais[g]];

        if (!grupo.material.textura.empty())
            grupo.material.texID = loadTexture(diretorio + grupo.material.textura);
        else
            grupo.material.texID = texturaBranca;  // sem textura: a cor vem só do kd

        Material &m = grupo.material;
        cout << "  grupo '" << m.nome << "': " << grupo.nVertices / 3 << " triangulos"
             << " | Ka " << m.ka.r << " " << m.ka.g << " " << m.ka.b
             << " | Kd " << m.kd.r << " " << m.kd.g << " " << m.kd.b
             << " | Ks " << m.ks.r << " " << m.ks.g << " " << m.ks.b
             << " | Ns " << m.ns << " | textura: " << (m.textura.empty() ? "-" : m.textura) << endl;

        grupos.push_back(grupo);
    }
    return grupos;
}

/*
 * Lê o arquivo de configuração da cena. Cada linha começa com uma palavra
 * que diz o que ela define (linhas com # são comentários):
 *
 *   camera     x y z  yaw pitch  fov near far  velocidade
 *   ambiente   Ia
 *   atenuacao  Kc Kl Kq
 *   fundo      r g b
 *   velocidade v
 *   luz        nome  x y z  r g b  intensidade
 *   objeto     nome arquivo.obj  tx ty tz  rx ry rz  escala
 *   ponto      x y z       (ponto de controle da trajetória do último objeto)
 *   orientar   0 ou 1      (se o último objeto vira na direção do movimento)
 *   fixo                   (o último objeto não pode ser selecionado nem transformado)
 */
void atualizarTrajetoria(Object3D &obj, float dt);  // definida mais abaixo

bool carregarCena(string caminho)
{
    ifstream arq(caminho.c_str());
    if (!arq.is_open())
    {
        cerr << "Erro ao abrir o arquivo de cena " << caminho << endl;
        return false;
    }

    string line;
    while (getline(arq, line))
    {
        istringstream ss(line);
        string tipo;
        ss >> tipo;

        if (tipo.empty() || tipo[0] == '#')
            continue;

        if (tipo == "camera")
        {
            glm::vec3 pos;
            float yaw, pitch;
            ss >> pos.x >> pos.y >> pos.z >> yaw >> pitch;
            camera = Camera(pos, yaw, pitch);
            ss >> camera.fov >> camera.zNear >> camera.zFar; 
            float v;
            if (ss >> v) camera.speed = v;
            cameraInicial = camera;
        }
        else if (tipo == "ambiente")
        {
            ss >> Ia;
            IaInicial = Ia;
        }
        else if (tipo == "atenuacao")
            ss >> atenuacao.x >> atenuacao.y >> atenuacao.z;
        else if (tipo == "fundo")
            ss >> corFundo.r >> corFundo.g >> corFundo.b;
        else if (tipo == "velocidade")
            ss >> velocidade;
        else if (tipo == "luz")
        {
            Luz luz;
            ss >> luz.nome >> luz.posicao.x >> luz.posicao.y >> luz.posicao.z
               >> luz.cor.r >> luz.cor.g >> luz.cor.b >> luz.intensidade;
            luz.intensidadeInicial = luz.intensidade;
            if (luzes.size() < MAX_LUZES)
                luzes.push_back(luz);
        }
        else if (tipo == "objeto")
        {
            Object3D obj;
            string arquivo;
            float escala;
            ss >> obj.nome >> arquivo
               >> obj.position.x >> obj.position.y >> obj.position.z
               >> obj.rotation.x >> obj.rotation.y >> obj.rotation.z
               >> escala;
            obj.scale = glm::vec3(escala);

            // o mesmo .OBJ só é lido uma vez, mesmo que apareça em vários objetos
            if (!malhas.count(arquivo))
                malhas[arquivo] = loadOBJ(PASTA_ASSETS + arquivo);
            obj.grupos = &malhas[arquivo];

            obj.posicaoInicial = obj.position;
            obj.rotacaoInicial = obj.rotation;
            obj.escalaInicial = obj.scale;
            cena.push_back(obj);
        }
        else if (tipo == "ponto" && !cena.empty())
        {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            cena.back().trajetoria.push_back(p);
        }
        else if (tipo == "fixo" && !cena.empty())
            cena.back().selecionavel = false;
        else if (tipo == "orientar" && !cena.empty())
        {
            int o;
            ss >> o;
            cena.back().orientar = (o != 0);
        }
    }
    arq.close();

    // objetos com trajetória começam no primeiro ponto, já virados para a curva
    for (size_t i = 0; i < cena.size(); i++)
        atualizarTrajetoria(cena[i], 0.0f);

    cout << "Cena carregada: " << cena.size() << " objetos, " << luzes.size() << " luzes" << endl;
    return true;
}

// ---------------------------------------------------------------------------
// Curva de Bézier
// ---------------------------------------------------------------------------

/*
 * Bézier cúbica (material do M6):
 *   B(t) = (1-t)^3 P0 + 3(1-t)^2 t P1 + 3(1-t) t^2 P2 + t^3 P3,   t em [0, 1]
 * A curva começa em P0, termina em P3 e é "puxada" por P1 e P2.
 */
glm::vec3 bezier(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t)
{
    float u = 1.0f - t;
    return u * u * u * P0 + 3.0f * u * u * t * P1 + 3.0f * u * t * t * P2 + t * t * t * P3;
}

// derivada (vetor tangente): diz para onde o objeto está indo
glm::vec3 bezierDerivada(glm::vec3 P0, glm::vec3 P1, glm::vec3 P2, glm::vec3 P3, float t)
{
    float u = 1.0f - t;
    return 3.0f * u * u * (P1 - P0) + 6.0f * u * t * (P2 - P1) + 3.0f * t * t * (P3 - P2);
}

/*
 * A trajetória é uma sequência de Béziers cúbicas emendadas. Com os pontos
 * P0 P1 P2 P3 P4 P5 ..., o trecho i usa P[3i], P[3i+1], P[3i+2] e P[3i+3].
 * O último índice dá a volta (% n), então com um número de pontos múltiplo
 * de 3 o fim do último trecho é o P0 e a curva fica fechada (cíclica).
 */
int numSegmentos(const Object3D &obj)
{
    return obj.trajetoria.size() / 3;
}

void pontosDoSegmento(const Object3D &obj, int seg, glm::vec3 P[4])
{
    int n = obj.trajetoria.size();
    for (int k = 0; k < 4; k++)
        P[k] = obj.trajetoria[(3 * seg + k) % n];
}

void atualizarTrajetoria(Object3D &obj, float dt)
{
    if (numSegmentos(obj) < 1) return;   // precisa de pelo menos 3 pontos

    glm::vec3 P[4];
    pontosDoSegmento(obj, obj.segmento, P);

    // Para a velocidade não depender do tamanho do trecho, t avança na
    // proporção da velocidade dividida pelo tamanho da derivada (|B'(t)|).
    glm::vec3 d = bezierDerivada(P[0], P[1], P[2], P[3], obj.t);
    float rapidez = glm::length(d);
    obj.t += (rapidez > 0.0001f) ? velocidade * dt / rapidez : 1.0f;

    if (obj.t >= 1.0f)   // terminou o trecho: passa para o próximo
    {
        obj.t -= 1.0f;
        obj.segmento = (obj.segmento + 1) % numSegmentos(obj);
        pontosDoSegmento(obj, obj.segmento, P);
        obj.t = glm::min(obj.t, 1.0f);
    }

    obj.position = bezier(P[0], P[1], P[2], P[3], obj.t);

    // vira o objeto para a direção do movimento (só no plano xz)
    d = bezierDerivada(P[0], P[1], P[2], P[3], obj.t);
    if (obj.orientar && glm::length(glm::vec2(d.x, d.z)) > 0.0001f)
        obj.direcao = glm::degrees(atan2(d.x, d.z));
}

// pontos ao longo das curvas, para desenhar a trajetória com GL_LINE_STRIP
GLuint curvaVAO = 0, curvaVBO = 0;

int montarLinhaDaCurva(const Object3D &obj)
{
    vector<GLfloat> pts;
    int nSeg = numSegmentos(obj);
    for (int s = 0; s < nSeg; s++)
    {
        glm::vec3 P[4];
        pontosDoSegmento(obj, s, P);
        for (int i = 0; i <= 30; i++)
        {
            glm::vec3 p = bezier(P[0], P[1], P[2], P[3], i / 30.0f);
            pts.insert(pts.end(), {p.x, p.y + 0.01f, p.z, 0, 0, 0, 1, 0});
        }
    }
    if (curvaVAO == 0)
    {
        glGenVertexArrays(1, &curvaVAO);
        glGenBuffers(1, &curvaVBO);
        glBindVertexArray(curvaVAO);
        glBindBuffer(GL_ARRAY_BUFFER, curvaVBO);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid *)0);
        glEnableVertexAttribArray(0);
    }
    glBindVertexArray(curvaVAO);
    glBindBuffer(GL_ARRAY_BUFFER, curvaVBO);
    glBufferData(GL_ARRAY_BUFFER, pts.size() * sizeof(GLfloat), pts.data(), GL_DYNAMIC_DRAW);
    return pts.size() / 8;
}

// ---------------------------------------------------------------------------
// Matriz de modelo
// ---------------------------------------------------------------------------

/*
    GLM faz a multiplicação pela direita, então a ordem que o vértice
    é afetado é a inversa da escrita: scale -> rotate -> translate.
    A rotação e a escala acontecem em torno da origem; se o objeto fosse
    transladado primeiro, ele orbitaria a origem em vez de girar sobre
    o próprio eixo.
    A "direcao" é a rotação em y da animação (para onde o gato anda).
*/
glm::mat4 getModelMatrix(const Object3D &obj)
{
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, obj.position);
    model = glm::rotate(model, glm::radians(obj.direcao), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(obj.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, obj.scale);
    return model;
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

void resetarCena()
{
    for (size_t i = 0; i < cena.size(); i++)
    {
        cena[i].position = cena[i].posicaoInicial;
        cena[i].rotation = cena[i].rotacaoInicial;
        cena[i].scale = cena[i].escalaInicial;
        cena[i].segmento = 0;
        cena[i].t = 0.0f;
        cena[i].direcao = 0.0f;
        atualizarTrajetoria(cena[i], 0.0f);
    }
    selecionado = -1;
    for (size_t i = 0; i < luzes.size(); i++)
    {
        luzes[i].ligada = true;
        luzes[i].intensidade = luzes[i].intensidadeInicial;
    }
    Ia = IaInicial;
    camera = cameraInicial;
    animando = false;
    modoExibicao = 0;
    cout << "Cena resetada" << endl;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (action != GLFW_PRESS && action != GLFW_REPEAT)
        return;

    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        static bool wireframe = false;
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }

    // seleção (SHIFT + TAB volta)
    if (key == GLFW_KEY_TAB)
    {
        int n = cena.size();
        int passo = (mode & GLFW_MOD_SHIFT) ? n - 1 : 1;   // SHIFT anda para trás
        int i = (selecionado < 0) ? ((mode & GLFW_MOD_SHIFT) ? 0 : n - 1) : selecionado;

        // anda pela lista até achar um objeto selecionável (pula o cenário fixo)
        for (int k = 0; k < n; k++)
        {
            i = (i + passo) % n;
            if (cena[i].selecionavel)
            {
                selecionado = i;
                cout << "Objeto selecionado: " << selecionado << " (" << cena[selecionado].nome << ")" << endl;
                break;
            }
        }
    }

    // escolha do modo de transformação
    if (key == GLFW_KEY_R) { modo = 'R'; cout << "Modo: ROTACAO" << endl; }
    if (key == GLFW_KEY_T) { modo = 'T'; cout << "Modo: TRANSLACAO" << endl; }
    if (key == GLFW_KEY_E) { modo = 'E'; cout << "Modo: ESCALA" << endl; }

    if (key == GLFW_KEY_U && action == GLFW_PRESS)
    {
        escalaUniforme = !escalaUniforme;
        cout << "Escala uniforme: " << (escalaUniforme ? "ON" : "OFF") << endl;
    }

    // luzes: liga / desliga
    if (key >= GLFW_KEY_1 && key < GLFW_KEY_1 + (int)luzes.size() && action == GLFW_PRESS)
    {
        int i = key - GLFW_KEY_1;
        luzes[i].ligada = !luzes[i].ligada;
        cout << "Luz " << luzes[i].nome << ": " << (luzes[i].ligada ? "ON" : "OFF") << endl;
    }

    // luzes: escolhe qual ajustar e muda a intensidade
    if (key == GLFW_KEY_L && action == GLFW_PRESS && !luzes.empty())
    {
        luzEscolhida = (luzEscolhida + 1) % luzes.size();
        cout << "Ajustando a luz " << luzes[luzEscolhida].nome
             << " (intensidade " << luzes[luzEscolhida].intensidade << ")" << endl;
    }
    if ((key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) && !luzes.empty())
    {
        luzes[luzEscolhida].intensidade += 0.1f;
        cout << "Luz " << luzes[luzEscolhida].nome << ": intensidade " << luzes[luzEscolhida].intensidade << endl;
    }
    if ((key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) && !luzes.empty())
    {
        luzes[luzEscolhida].intensidade = glm::max(0.0f, luzes[luzEscolhida].intensidade - 0.1f);
        cout << "Luz " << luzes[luzEscolhida].nome << ": intensidade " << luzes[luzEscolhida].intensidade << endl;
    }
    // luz ambiente: I aumenta, SHIFT + I diminui (o GLFW identifica as teclas pela
    // posição no teclado americano, então [ e ] mudam de lugar no ABNT2; o I não muda)
    if (key == GLFW_KEY_I)
    {
        if (mode & GLFW_MOD_SHIFT)
            Ia = glm::max(0.0f, Ia - 0.05f);
        else
            Ia = glm::min(1.0f, Ia + 0.05f);
        cout << "Ia: " << Ia << endl;
    }

    // materiais e texturas: alterna o que o fragment shader devolve
    if (key == GLFW_KEY_V && action == GLFW_PRESS)
    {
        modoExibicao = (modoExibicao + 1) % 6;
        cout << "Exibicao: " << nomesModos[modoExibicao] << endl;
    }

    // animação
    if (key == GLFW_KEY_M && action == GLFW_PRESS)
    {
        animando = !animando;
        cout << "Animacao: " << (animando ? "ON" : "OFF") << endl;
    }
    if (key == GLFW_KEY_B && action == GLFW_PRESS)
    {
        mostrarCurvas = !mostrarCurvas;
        cout << "Curvas: " << (mostrarCurvas ? "visiveis" : "escondidas") << endl;
    }

    if (key == GLFW_KEY_SPACE && action == GLFW_PRESS)
        resetarCena();

    // daqui para baixo as teclas agem no objeto selecionado
    if (selecionado < 0)
        return;
    Object3D &obj = cena[selecionado];

    // SHIFT inverte o sentido da transformação
    float sentido = (mode & GLFW_MOD_SHIFT) ? -1.0f : 1.0f;

    float passoRot = 5.0f * sentido;
    float passoTrans = 0.05f * sentido;
    float passoEscala = 1.0f + 0.05f * sentido;

    glm::vec3 eixo(0.0f);
    if (key == GLFW_KEY_X) eixo.x = 1.0f;
    if (key == GLFW_KEY_Y) eixo.y = 1.0f;
    if (key == GLFW_KEY_Z) eixo.z = 1.0f;

    if (eixo != glm::vec3(0.0f))
    {
        if (modo == 'R')
            obj.rotation += eixo * passoRot;
        else if (modo == 'T')
            obj.position += eixo * passoTrans;
        else if (modo == 'E')
        {
            if (escalaUniforme)
                obj.scale *= passoEscala;
            else
                obj.scale *= glm::mix(glm::vec3(1.0f), glm::vec3(passoEscala), eixo);
        }
    }

    // translação alternativa pelas setas (X e Y)
    if (modo == 'T')
    {
        if (key == GLFW_KEY_LEFT)  obj.position.x -= 0.05f;
        if (key == GLFW_KEY_RIGHT) obj.position.x += 0.05f;
        if (key == GLFW_KEY_UP)    obj.position.y += 0.05f;
        if (key == GLFW_KEY_DOWN)  obj.position.y -= 0.05f;
    }

    if (key == GLFW_KEY_C && action == GLFW_PRESS)
    {
        obj.trajetoria.clear();
        obj.segmento = 0;
        obj.t = 0.0f;
        cout << "Trajetoria de " << obj.nome << " apagada" << endl;
    }
    if (key == GLFW_KEY_G && action == GLFW_PRESS)
    {
        // imprime no formato do cena.txt, para copiar e colar no arquivo
        cout << "# trajetoria de " << obj.nome << endl;
        for (size_t i = 0; i < obj.trajetoria.size(); i++)
            cout << "ponto " << obj.trajetoria[i].x << " " << obj.trajetoria[i].y << " " << obj.trajetoria[i].z << endl;
    }
}

void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    camera.processarMouse(xpos, ypos);
}

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset)
{
    camera.zoom((float)yoffset);
}

void mouse_button_callback(GLFWwindow *window, int button, int action, int mods)
{
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && selecionado >= 0)
    {
        Object3D &obj = cena[selecionado];
        obj.trajetoria.push_back(obj.position);   // a posição atual vira ponto de controle
        cout << "Ponto " << obj.trajetoria.size() << " adicionado em " << obj.nome << ": "
             << obj.position.x << " " << obj.position.y << " " << obj.position.z << endl;
        if (obj.trajetoria.size() % 3 != 0)
            cout << "  (a curva usa os pontos de 3 em 3)" << endl;
    }
}

void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.mover(Camera::FRENTE, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.mover(Camera::TRAS, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.mover(Camera::ESQUERDA, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.mover(Camera::DIREITA, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) camera.mover(Camera::CIMA, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camera.mover(Camera::BAIXO, deltaTime);
}

// ---------------------------------------------------------------------------
// Shader
// ---------------------------------------------------------------------------

int setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cerr << "ERRO no vertex shader:\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cerr << "ERRO no fragment shader:\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cerr << "ERRO ao linkar o shader:\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return shaderProgram;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_SAMPLES, 4);  // antialiasing

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Cafe dos Gatinhos - Visualizador 3D", nullptr, nullptr);
    if (!window)
    {
        cerr << "Falha ao criar a janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);  // esconde o cursor

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cerr << "Falha ao inicializar o GLAD" << endl;
        return -1;
    }

    cout << "Placa de video: " << glGetString(GL_RENDERER) << endl;
    cout << "Versao do OpenGL: " << glGetString(GL_VERSION) << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    GLuint shaderID = setupShader();

    // o PNG é lido de cima para baixo, mas o OpenGL espera a origem
    // da textura embaixo à esquerda (sem isso a textura fica invertida)
    stbi_set_flip_vertically_on_load(true);

    // textura branca 1x1: para materiais sem map_Kd a cor fica só com o kd
    unsigned char branco[3] = {255, 255, 255};
    glGenTextures(1, &texturaBranca);
    glBindTexture(GL_TEXTURE_2D, texturaBranca);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, branco);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    if (!carregarCena(PASTA_ASSETS + "cena.txt") || cena.empty())
    {
        glfwTerminate();
        return -1;
    }

    glUseProgram(shaderID);

    // o shader recebe o número da unidade de textura (0), não o id da textura
    glUniform1i(glGetUniformLocation(shaderID, "texBuff"), 0);
    glActiveTexture(GL_TEXTURE0);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint selectedLoc = glGetUniformLocation(shaderID, "isSelected");
    GLint kaLoc = glGetUniformLocation(shaderID, "ka");
    GLint kdLoc = glGetUniformLocation(shaderID, "kd");
    GLint ksLoc = glGetUniformLocation(shaderID, "ks");
    GLint qLoc = glGetUniformLocation(shaderID, "q");
    GLint curvaLoc = glGetUniformLocation(shaderID, "desenhaCurva");

    glUniform1f(glGetUniformLocation(shaderID, "Kc"), atenuacao.x);
    glUniform1f(glGetUniformLocation(shaderID, "Kl"), atenuacao.y);
    glUniform1f(glGetUniformLocation(shaderID, "Kq"), atenuacao.z);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // loop principal
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        processInput(window);

        if (animando)
            for (size_t i = 0; i < cena.size(); i++)
                atualizarTrajetoria(cena[i], deltaTime);

        glClearColor(corFundo.r, corFundo.g, corFundo.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // câmera: matrizes de view e projection
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = camera.getProjectionMatrix((float)width / (float)glm::max(height, 1));
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(shaderID, "cameraPos"), 1, glm::value_ptr(camera.position));

        // parâmetros de iluminação que podem mudar pelo teclado
        glUniform1i(glGetUniformLocation(shaderID, "modoExibicao"), modoExibicao);
        glUniform1f(glGetUniformLocation(shaderID, "Ia"), Ia);
        glUniform1i(glGetUniformLocation(shaderID, "nLuzes"), (int)luzes.size());
        for (size_t i = 0; i < luzes.size(); i++)
        {
            string idx = "[" + to_string(i) + "]";
            glUniform3fv(glGetUniformLocation(shaderID, ("lightPos" + idx).c_str()), 1, glm::value_ptr(luzes[i].posicao));
            glUniform3fv(glGetUniformLocation(shaderID, ("lightColor" + idx).c_str()), 1, glm::value_ptr(luzes[i].cor));
            glUniform1f(glGetUniformLocation(shaderID, ("lightIntensity" + idx).c_str()), luzes[i].intensidade);
            glUniform1i(glGetUniformLocation(shaderID, ("lightOn" + idx).c_str()), luzes[i].ligada ? 1 : 0);
        }

        // desenha cada objeto com a sua matriz de modelo e cada grupo com o seu material
        glUniform1i(curvaLoc, 0);
        for (size_t i = 0; i < cena.size(); i++)
        {
            glUniform1i(selectedLoc, (int)i == selecionado ? 1 : 0);

            glm::mat4 model = getModelMatrix(cena[i]);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

            vector<Grupo> &grupos = *cena[i].grupos;
            for (size_t g = 0; g < grupos.size(); g++)
            {
                Material &m = grupos[g].material;
                glUniform3fv(kaLoc, 1, glm::value_ptr(m.ka));
                glUniform3fv(kdLoc, 1, glm::value_ptr(m.kd));
                glUniform3fv(ksLoc, 1, glm::value_ptr(m.ks));
                glUniform1f(qLoc, m.ns);
                glBindTexture(GL_TEXTURE_2D, m.texID);

                glBindVertexArray(grupos[g].VAO);
                glDrawArrays(GL_TRIANGLES, 0, grupos[g].nVertices);
            }
        }

        // linhas das trajetórias (a do objeto selecionado em vermelho)
        if (mostrarCurvas)
        {
            glUniform1i(curvaLoc, 1);
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
            glLineWidth(2.0f);
            for (size_t i = 0; i < cena.size(); i++)
            {
                if (numSegmentos(cena[i]) < 1) continue;
                glm::vec3 cor = ((int)i == selecionado) ? glm::vec3(0.9f, 0.1f, 0.1f) : glm::vec3(0.1f, 0.3f, 0.9f);
                glUniform3fv(glGetUniformLocation(shaderID, "corCurva"), 1, glm::value_ptr(cor));
                int n = montarLinhaDaCurva(cena[i]);
                glDrawArrays(GL_LINE_STRIP, 0, n);
            }
            glUniform1i(curvaLoc, 0);
        }
        glBindVertexArray(0);

        glfwSwapBuffers(window);
    }

    glfwTerminate();
    return 0;
}