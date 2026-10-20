/*
 * Camera.h - Câmera em primeira pessoa (M5)
 *
 * Agrupa os atributos da câmera sintética (posição, orientação e frustum)
 * e encapsula as ações de mover e rotacionar, seguindo o material de
 * aprofundamento do M5:
 *  - view = lookAt(position, position + front, up)
 *  - front calculado a partir dos ângulos de Euler (yaw e pitch)
 *  - velocidade moderada pelo deltaTime
 *  - zoom alterando o FOV
 *
 * Alunas: Eduarda Fernandes e Maria Eduarda Dias
 */

#ifndef CAMERA_H
#define CAMERA_H

#include <cmath>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    // direções possíveis para o mover
    enum Direcao
    {
        FRENTE,
        TRAS,
        ESQUERDA,
        DIREITA,
        CIMA,
        BAIXO
    };

    // posição e orientação
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp; // "cima" do mundo (eixo y), usado para recalcular right e up

    // ângulos de Euler, em graus
    float yaw;   // olhar para os lados
    float pitch; // olhar para cima e para baixo

    // frustum (projeção)
    float fov;
    float zNear;
    float zFar;

    // ajustes
    float speed;       // unidades por segundo
    float sensitivity; // amortiza o deslocamento do mouse

    // yaw = -90 faz a câmera começar olhando para -z (para dentro da tela)
    Camera(glm::vec3 posicaoInicial = glm::vec3(0.0f, 0.0f, 3.0f),
           float yawInicial = -90.0f, float pitchInicial = 0.0f)
        : position(posicaoInicial),
          worldUp(0.0f, 1.0f, 0.0f),
          yaw(yawInicial),
          pitch(pitchInicial),
          fov(45.0f),
          zNear(0.1f),
          zFar(100.0f),
          speed(2.5f),
          sensitivity(0.05f)
    {
        atualizarVetores();
    }

    // matrizes

    glm::mat4 getViewMatrix() const
    {
        // o alvo e um ponto: a posição deslocada na direção do front
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 getProjectionMatrix(float aspectRatio) const
    {
        return glm::perspective(glm::radians(fov), aspectRatio, zNear, zFar);
    }

    // mover

    // deltaTime deixa a velocidade igual em qualquer taxa de quadros
    void mover(Direcao direcao, float deltaTime)
    {
        float passo = speed * deltaTime;

        if (direcao == FRENTE)
            position += front * passo;
        if (direcao == TRAS)
            position -= front * passo;
        if (direcao == ESQUERDA)
            position -= right * passo;
        if (direcao == DIREITA)
            position += right * passo;
        if (direcao == CIMA)
            position += worldUp * passo;
        if (direcao == BAIXO)
            position -= worldUp * passo;
    }

    // rotacionar

    // recebe o deslocamento (em pixels) do mouse de um quadro para o outro
    void rotacionar(float xoffset, float yoffset)
    {
        yaw += xoffset * sensitivity;
        pitch += yoffset * sensitivity;

        // trava o pitch para a câmera não "virar de cabeça para baixo"
        // (em 90 graus o front ficaria paralelo ao worldUp)
        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        atualizarVetores();
    }

    // recebe a posição absoluta do cursor (vinda da mouse_callback)
    // e calcula sozinha o deslocamento desde o último quadro
    void processarMouse(double xpos, double ypos)
    {
        if (firstMouse)
        {
            lastX = (float)xpos;
            lastY = (float)ypos;
            firstMouse = false;
        }

        float xoffset = (float)xpos - lastX;
        float yoffset = lastY - (float)ypos; // invertido: y da tela cresce para baixo

        lastX = (float)xpos;
        lastY = (float)ypos;

        rotacionar(xoffset, yoffset);
    }

    // zoom

    // scroll do mouse: diminuir o FOV aproxima a imagem
    void zoom(float yoffset)
    {
        fov -= yoffset;
        if (fov < 1.0f)
            fov = 1.0f;
        if (fov > 45.0f)
            fov = 45.0f;
    }

private:
    // estado do mouse, para calcular o deslocamento entre quadros
    bool firstMouse = true;
    float lastX = 0.0f;
    float lastY = 0.0f;

    // recalcula front a partir de yaw e pitch, e depois right e up
    void atualizarVetores()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);

        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

#endif