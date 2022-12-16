#include "logg.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <vector>

#define GLAD_GL_IMPLEMENTATION
#include "gl.h"

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/vector_angle.hpp>

class Boid {
public:
  glm::vec2 pos;
  glm::vec2 velocity;
};

constexpr int windowWidth = 1200;
constexpr int windowHeight = 800;
constexpr int32_t marginSize = 200;
constexpr int32_t innerWidth = windowWidth - marginSize * 2;
constexpr int32_t innerHeight = windowHeight - marginSize * 2;
constexpr uint32_t nrBoids = 400;

float speedLimitMin = 1.0f;
float speedLimitMax = 10.0f;
float vision = 75.0f;
float centeringFactor = 0.005f;
float avoidDistance = 20.0f;
float avoidFactor = 0.05f;
float matchFactor = 0.05f;
float marginTurnFactor = 1.0f;

std::vector<Boid> boids(nrBoids);
std::vector<Boid> visibleBoids(nrBoids);

// clang-format off
const char *vShaderSource = R"(#version 330 core
layout (location = 0) in vec2 aPos;

uniform mat4 model;
uniform mat4 view;

void main() {
  gl_Position = view * model * vec4(aPos, 0.0, 1.0);
}
)";

const char *fShaderSource = R"(#version 330 core
out vec4 FragColor;

void main() {
  FragColor = vec4(0.2, 0.2, 0.2, 1.0);
}
)";
// clang-format on

Logger logger(Logger::Level::Info, stderr);

float randf() { return (float)rand() / (float)RAND_MAX; }

void randomize();
void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods);

int main() {
  logger.debug("Hello world");
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
  glfwWindowHint(GLFW_SAMPLES, 4);

  GLFWwindow *window =
      glfwCreateWindow(windowWidth, windowHeight, "glBoids", nullptr, nullptr);
  if (!window) {
    throw std::runtime_error("failed to create GLFW window!");
  }
  glfwMakeContextCurrent(window);

  glfwSetKeyCallback(window, keyCallback);

  int version = gladLoadGL(glfwGetProcAddress);
  logger.info("Using OpenGL version {}.{}", GLAD_VERSION_MAJOR(version),
              GLAD_VERSION_MINOR(version));

  glfwSwapInterval(1);

  logger.debug("Compiling shaders...");

  uint32_t vShader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vShader, 1, &vShaderSource, nullptr);
  glCompileShader(vShader);

  int32_t success = 0;
  glGetShaderiv(vShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    int32_t maxLength = 0;
    glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &maxLength);
    std::string infoLog(maxLength, '\0');
    glGetShaderInfoLog(vShader, maxLength, nullptr, infoLog.data());
    glDeleteShader(vShader);
    throw std::runtime_error(
        fmt::format("compiling vertex shader failed:\n\n{}", infoLog));
  }

  uint32_t fShader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fShader, 1, &fShaderSource, nullptr);
  glCompileShader(fShader);

  glGetShaderiv(fShader, GL_COMPILE_STATUS, &success);
  if (!success) {
    int32_t maxLength = 0;
    glGetShaderiv(fShader, GL_INFO_LOG_LENGTH, &maxLength);
    std::string infoLog(maxLength, '\0');
    glGetShaderInfoLog(fShader, maxLength, nullptr, infoLog.data());
    glDeleteShader(vShader);
    glDeleteShader(fShader);
    throw std::runtime_error(
        fmt::format("compiling fragment shader failed:\n\n{}", infoLog));
  }

  uint32_t shaderProgram = glCreateProgram();
  glAttachShader(shaderProgram, vShader);
  glAttachShader(shaderProgram, fShader);
  glLinkProgram(shaderProgram);
  glDeleteShader(vShader);
  glDeleteShader(fShader);

  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success) {
    int32_t maxLength = 0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &maxLength);
    std::string infoLog(maxLength, '\0');
    glGetProgramInfoLog(shaderProgram, maxLength, nullptr, infoLog.data());
    glDeleteProgram(shaderProgram);
    throw std::runtime_error(
        fmt::format("linking shader program failed:\n\n{}", infoLog));
  }

  logger.debug("Setting up vao and vbo...");

  const std::vector<float> vertices = {0.0f, 1.0f, -0.6f, -1.0f, 0.6f, -1.0f};

  uint32_t vao = 0, vbo = 0;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  glBindVertexArray(vao);

  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
               GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
  glEnableVertexAttribArray(0);

  glUseProgram(shaderProgram);

  glEnable(GL_MULTISAMPLE);

  glm::mat4 view(1.0f);
  view = glm::scale(view, glm::vec3(1.0f / ((float)windowWidth * 0.5f),
                                    1.0f / ((float)windowHeight * 0.5f), 1.0f));
  glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE,
                     glm::value_ptr(view));

  randomize();

  logger.debug("Entering main loop...");

  float deltaTime = 0.0;
  float lastFrame = glfwGetTime();

  while (!glfwWindowShouldClose(window)) {
    glClearColor(0.9f, 0.9f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (uint32_t i = 0; i < nrBoids; ++i) {
      Boid &boid = boids[i];
      uint32_t visible = 0;
      for (uint32_t j = 0; j < nrBoids; ++j) {
        Boid &b = boids[j];
        if (j != i && glm::distance(b.pos, boid.pos) < vision) {
          visibleBoids[visible] = b;
          ++visible;
        }
      }

      {
        // Fly towards center
        if (visible) {
          glm::vec2 center(0.0f);
          for (uint32_t j = 0; j < visible; ++j) {
            center += visibleBoids[j].pos;
          }
          center /= visible;

          boid.velocity += (center - boid.pos) * centeringFactor;
        }
      }

      {
        // Avoid
        glm::vec2 move(0.0f);
        for (uint32_t j = 0; j < nrBoids; ++j) {
          Boid &b = boids[j];
          if (j == i) {
            continue;
          }
          if (glm::distance(boid.pos, b.pos) < avoidDistance) {
            move += boid.pos - b.pos;
          }
        }
        boid.velocity += move * avoidFactor;
      }

      {
        // Match velocity
        if (visible) {
          glm::vec2 avg(0.0f);
          for (uint32_t j = 0; j < visible; ++j) {
            avg += visibleBoids[j].velocity;
          }
          avg /= visible;

          boid.velocity += (avg - boid.velocity) * matchFactor;
        }
      }

      {
        // Check margins
        if (boid.pos.x < innerWidth * -0.5f) {
          boid.velocity.x += marginTurnFactor;
        }
        if (boid.pos.x > innerWidth * 0.5f) {
          boid.velocity.x -= marginTurnFactor;
        }
        if (boid.pos.y < innerHeight * -0.5f) {
          boid.velocity.y += marginTurnFactor;
        }
        if (boid.pos.y > innerHeight * 0.5f) {
          boid.velocity.y -= marginTurnFactor;
        }
      }

      {
        // Check speed
        if (glm::distance(boid.velocity, glm::vec2(0.0f)) > speedLimitMax) {
          boid.velocity = glm::normalize(boid.velocity);
          boid.velocity *= speedLimitMax;
        } else if (glm::distance(boid.velocity, glm::vec2(0.0f)) <
                   speedLimitMin) {
          boid.velocity = glm::normalize(boid.velocity);
          boid.velocity *= speedLimitMin;
        }
      }

      // Update pos
      boid.pos += boid.velocity * (float)deltaTime * 60.0f;

      glm::mat4 model(1.0f);
      model = glm::translate(model, glm::vec3(boid.pos, 0.0f));
      model = glm::scale(model, glm::vec3(9.0f, 9.0f, 1.0f));
      float angle = glm::orientedAngle(glm::normalize(boid.velocity),
                                       glm::vec2(0.0f, 1.0f));
      model = glm::rotate(model, angle, glm::vec3(0.0f, 0.0f, -1.0f));
      glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1,
                         GL_FALSE, glm::value_ptr(model));

      glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    float currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    float fps = 1.0f / deltaTime;

    fmt::print("\rFrame time: {:.4f}, FPS: {}             ", deltaTime,
               (uint32_t)fps);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  fmt::print("\n");

  logger.debug("Cleaning up...");

  glDeleteVertexArrays(1, &vao);
  glDeleteBuffers(1, &vbo);
  glDeleteProgram(shaderProgram);

  return 0;
}

void randomize() {
  for (Boid &boid : boids) {
    boid.pos.x = (randf() * innerWidth) - innerWidth * 0.5f;
    boid.pos.y = (randf() * innerHeight) - innerHeight * 0.5f;

    boid.velocity.x = (randf() * 2.0f) - 1;
    boid.velocity.y = (randf() * 2.0f) - 1;
    boid.velocity = glm::normalize(boid.velocity);
    boid.velocity *= randf() * (speedLimitMax - speedLimitMin) + speedLimitMin;
  }
}

void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                 int mods) {
  if (action == GLFW_PRESS) {
    randomize();
  }
}
