#include <algorithm>

#include <imgui_impl_glfw.h>

#include <glm/gtc/matrix_transform.hpp>

#include <ctime>
#include <cstdlib>

#include "OGLRenderer.h"
#include "ModelSettings.h"
#include "Logger.h"

OGLRenderer::OGLRenderer(GLFWwindow *window) {
  mRenderData.rdWindow = window;
}

bool OGLRenderer::init(unsigned int width, unsigned int height) {
  /* randomize rand() */
  std::srand(static_cast<int>(time(NULL)));

  /* required for perspective */
  mRenderData.rdWidth = width;
  mRenderData.rdHeight = height;

  /* initalize GLAD */
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    Logger::log(1, "%s error: failed to initialize GLAD\n", __FUNCTION__);
    return false;
  }

  if (!GLAD_GL_VERSION_4_6) {
    Logger::log(1, "%s error: failed to get at least OpenGL 4.6\n", __FUNCTION__);
    return false;
  }

  GLint majorVersion, minorVersion;
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
  Logger::log(1, "%s: OpenGL %d.%d initializeed\n", __FUNCTION__, majorVersion, minorVersion);

  if (!mFramebuffer.init(width, height)) {
    Logger::log(1, "%s error: could not init Framebuffer\n", __FUNCTION__);
    return false;
  }
  Logger::log(1, "%s: framebuffer succesfully initialized\n", __FUNCTION__);

  mVertexBuffer.init();
  Logger::log(1, "%s: vertex buffer successfully created\n", __FUNCTION__);

  size_t uniformMatrixBufferSize = 2 * sizeof(glm::mat4);
  mUniformBuffer.init(uniformMatrixBufferSize);
  Logger::log(1, "%s: matrix uniform buffer (size %i bytes) successfully created\n", __FUNCTION__, uniformMatrixBufferSize);

  if (!mLineShader.loadShaders("shader/line.vert", "shader/line.frag")) {
    Logger::log(1, "%s: line shader loading failed\n", __FUNCTION__);
    return false;
  }

  if (!mGltfGPUShader.loadShaders("shader/gltf_gpu.vert", "shader/gltf_gpu.frag")) {
    Logger::log(1, "%s: gltTF GPU shader loading failed\n", __FUNCTION__);
    return false;
  }
  if (!mGltfGPUShader.getUniformLocation("aModelStride")) {
    Logger::log(1, "%s: failed to get model stride uniform for gltTF GPU shader\n",
      __FUNCTION__);
    return false;
  }

  if (!mGltfGPUDualQuatShader.loadShaders("shader/gltf_gpu_dquat.vert",
      "shader/gltf_gpu_dquat.frag")) {
    Logger::log(1, "%s: glTF GPU dual quat shader loading failed\n", __FUNCTION__);
    return false;
  }
  if (!mGltfGPUDualQuatShader.getUniformLocation("aModelStride")) {
    Logger::log(1, "%s: failed to get model stride uniform for gltTF GPU dual quat shader\n",
      __FUNCTION__);
    return false;
  }
  Logger::log(1, "%s: shaders succesfully loaded\n", __FUNCTION__);

  mUserInterface.init(mRenderData);
  Logger::log(1, "%s: user interface initialized\n", __FUNCTION__);

  /* add backface culling and depth test already here */
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glLineWidth(3.0);

  mGltfModel = std::make_shared<GltfModel>();
  std::string modelFilename = "assets/Woman.gltf";
  std::string modelTexFilename = "textures/Woman.png";
  if (!mGltfModel->loadModel(mRenderData, modelFilename, modelTexFilename)) {
    Logger::log(1, "%s: loading glTF model '%s' failed\n", __FUNCTION__, modelFilename.c_str());
    return false;
  }
  mGltfModel->uploadVertexBuffers();
  mGltfModel->uploadIndexBuffer();

  Logger::log(1, "%s: glTF model '%s' succesfully loaded\n", __FUNCTION__, modelFilename.c_str());

  int numTriangles = 0;

  /* create glTF instances from the model */
  for (int i = 0; i < 1000; ++i) {
    int xPos = std::rand() % 150 - 75;
    int zPos = std::rand() % 150 - 75;
    mGltfInstances.emplace_back(std::make_shared<GltfInstance>(mGltfModel, glm::vec2(static_cast<float>(xPos),
      static_cast<float>(zPos)), true));
    numTriangles += mGltfModel->getTriangleCount();
  }

  mRenderData.rdTriangleCount = numTriangles;

  mRenderData.rdNumberOfInstances = mGltfInstances.size();

  size_t modelJointMatrixBufferSize = mRenderData.rdNumberOfInstances * mGltfInstances.at(0)->getJointMatrixSize() *
    sizeof(glm::mat4);
  size_t modelJointDualQuatBufferSize = mRenderData.rdNumberOfInstances * mGltfInstances.at(0)->getJointDualQuatsSize() *
     sizeof(glm::mat2x4);

  mGltfShaderStorageBuffer.init(modelJointMatrixBufferSize);
  Logger::log(1, "%s: glTF joint matrix shader storage buffer (size %i bytes) successfully created\n", __FUNCTION__, modelJointMatrixBufferSize);

  mGltfDualQuatSSBuffer.init(modelJointDualQuatBufferSize);
  Logger::log(1, "%s: glTF joint dual quaternions shader storage buffer (size %i bytes) successfully created\n", __FUNCTION__, modelJointDualQuatBufferSize);

  /* valid, but emtpy */
  mLineMesh = std::make_shared<OGLMesh>();
  Logger::log(1, "%s: line mesh storage initialized\n", __FUNCTION__);

  mFrameTimer.start();

  return true;
}

void OGLRenderer::setSize(unsigned int width, unsigned int height) {
  /* handle minimize */
  if (width == 0 || height == 0) {
    return;
  }

  mRenderData.rdWidth = width;
  mRenderData.rdHeight = height;

  mFramebuffer.resize(width, height);
  glViewport(0, 0, width, height);

  Logger::log(1, "%s: resized window to %dx%d\n", __FUNCTION__, width, height);
}

void OGLRenderer::uploadData(OGLMesh vertexData) {
  mVertexBuffer.uploadData(vertexData);
}

void OGLRenderer::handleKeyEvents(int key, int scancode, int action, int mods) {
}

void OGLRenderer::handleMouseButtonEvents(int button, int action, int mods) {
  /* forward to ImGui */
  ImGuiIO& io = ImGui::GetIO();
  if (button >= 0 && button < ImGuiMouseButton_COUNT) {
    io.AddMouseButtonEvent(button, action == GLFW_PRESS);
  }

  /* hide from application if above ImGui window */
  if (io.WantCaptureMouse) {
    return;
  }

  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mMouseLock = !mMouseLock;

    if (mMouseLock) {
      glfwSetInputMode(mRenderData.rdWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      /* enable raw mode if possible */
      if (glfwRawMouseMotionSupported()) {
        glfwSetInputMode(mRenderData.rdWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      }
    } else {
      glfwSetInputMode(mRenderData.rdWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
  }
}

void OGLRenderer::handleMousePositionEvents(double xPos, double yPos) {
  /* forward to ImGui */
  ImGuiIO& io = ImGui::GetIO();
  io.AddMousePosEvent((float)xPos, (float)yPos);

  /* hide from application if above ImGui window */
  if (io.WantCaptureMouse) {
    return;
  }

  /* calculate relative movement from last position */
  int mouseMoveRelX = static_cast<int>(xPos) - mMouseXPos;
  int mouseMoveRelY = static_cast<int>(yPos) - mMouseYPos;

  if (mMouseLock) {
    mRenderData.rdViewAzimuth += mouseMoveRelX / 10.0;
    /* keep between 0 and 360 degree */
    if (mRenderData.rdViewAzimuth < 0.0) {
      mRenderData.rdViewAzimuth += 360.0;
    }
    if (mRenderData.rdViewAzimuth >= 360.0) {
      mRenderData.rdViewAzimuth -= 360.0;
    }

    mRenderData.rdViewElevation -= mouseMoveRelY / 10.0;
    /* keep between -89 and +89 degree */
    if (mRenderData.rdViewElevation > 89.0) {
      mRenderData.rdViewElevation = 89.0;
    }
    if (mRenderData.rdViewElevation < -89.0) {
      mRenderData.rdViewElevation = -89.0;
    }
  }

  /* save old values*/
  mMouseXPos = static_cast<int>(xPos);
  mMouseYPos = static_cast<int>(yPos);
}

void OGLRenderer::handleMovementKeys() {
  mRenderData.rdMoveForward = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_W) == GLFW_PRESS) {
    mRenderData.rdMoveForward += 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_S) == GLFW_PRESS) {
    mRenderData.rdMoveForward -= 1;
  }

  mRenderData.rdMoveRight = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_A) == GLFW_PRESS) {
    mRenderData.rdMoveRight -= 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_D) == GLFW_PRESS) {
    mRenderData.rdMoveRight += 1;
  }

  mRenderData.rdMoveUp = 0;
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_E) == GLFW_PRESS) {
    mRenderData.rdMoveUp += 1;
  }
  if (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_Q) == GLFW_PRESS) {
    mRenderData.rdMoveUp -= 1;
  }

  /* speed up movement with shift */
  if ((glfwGetKey(mRenderData.rdWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ||
      (glfwGetKey(mRenderData.rdWindow, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)) {
    mRenderData.rdMoveForward *= 4;
    mRenderData.rdMoveRight *= 4;
    mRenderData.rdMoveUp *= 4;
  }
}

void OGLRenderer::draw() {
  /* handle minimize */
  while (mRenderData.rdWidth == 0 || mRenderData.rdHeight == 0) {
    glfwGetFramebufferSize(mRenderData.rdWindow, &mRenderData.rdWidth, &mRenderData.rdHeight);
    glfwWaitEvents();
  }

  /* get time difference for movement */
  double tickTime = glfwGetTime();
  mRenderData.rdTickDiff = tickTime - mLastTickTime;

  mRenderData.rdFrameTime = mFrameTimer.stop();
  mFrameTimer.start();

  handleMovementKeys();

  /* draw to framebuffer */
  mFramebuffer.bind();

  glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
  glClearDepth(1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  mMatrixGenerateTimer.start();
  mProjectionMatrix = glm::perspective(
    glm::radians(static_cast<float>(mRenderData.rdFieldOfView)),
    static_cast<float>(mRenderData.rdWidth) / static_cast<float>(mRenderData.rdHeight),
    0.01f, 500.0f);

  mViewMatrix = mCamera.getViewMatrix(mRenderData);

  /* animate */
  for (auto &instance : mGltfInstances) {
    instance->updateAnimation();
  }

  mLineMesh->vertices.clear();

  /* get gltTF skeleton */
  mSkeletonLineIndexCount = 0;
  for (const auto &instance : mGltfInstances) {
    ModelSettings settings = instance->getInstanceSettings();
    if (settings.msDrawSkeleton) {
      std::shared_ptr<OGLMesh> mesh = instance->getSkeleton();
      mSkeletonLineIndexCount += mesh->vertices.size();
      mLineMesh->vertices.insert(mLineMesh->vertices.begin(),
        mesh->vertices.begin(), mesh->vertices.end());
    }
  }

  /* save value to avoid changes during later call */
  int selectedInstance = mRenderData.rdCurrentSelectedInstance;
  glm::vec2 modelWorldPos = mGltfInstances.at(selectedInstance)->getWorldPosition();
  glm::quat modelWorldRot = mGltfInstances.at(selectedInstance)->getWorldRotation();

  /* draw coordiante arrows*/
  mCoordArrowsMesh = mCoordArrowsModel.getVertexData();
  mCoordArrowsLineIndexCount = mCoordArrowsMesh.vertices.size();
  std::for_each(mCoordArrowsMesh.vertices.begin(), mCoordArrowsMesh.vertices.end(),
    [=](auto &n){
      n.color /= 2.0f;
      n.position = modelWorldRot * n.position;
      n.position += glm::vec3(modelWorldPos.x, 0.0f, modelWorldPos.y);
  });

  mLineMesh->vertices.insert(mLineMesh->vertices.end(),
    mCoordArrowsMesh.vertices.begin(), mCoordArrowsMesh.vertices.end());

  mRenderData.rdMatrixGenerateTime = mMatrixGenerateTimer.stop();

  mUploadToUBOTimer.start();
  std::vector<glm::mat4> matrixData;
  matrixData.push_back(mViewMatrix);
  matrixData.push_back(mProjectionMatrix);
  mUniformBuffer.uploadUboData(matrixData, 0);

  mModelJointMatrices.clear();
  mModelJointDualQuats.clear();

  unsigned int matrixInstances = 0;
  unsigned int dualQuatInstances = 0;
  unsigned int numTriangles = 0;

  for (const auto &instance : mGltfInstances) {
    ModelSettings settings = instance->getInstanceSettings();
    if (!settings.msDrawModel) {
      continue;
    }

    if (settings.msVertexSkinningMode == skinningMode::dualQuat) {
      std::vector<glm::mat2x4> quats = instance->getJointDualQuats();
      mModelJointDualQuats.insert(mModelJointDualQuats.end(),
        quats.begin(), quats.end());
      ++dualQuatInstances;
    } else {
      std::vector<glm::mat4> mats = instance->getJointMatrices();
      mModelJointMatrices.insert(mModelJointMatrices.end(),
        mats.begin(), mats.end());
      ++matrixInstances;
    }
    numTriangles += mGltfModel->getTriangleCount();
  }

  mRenderData.rdTriangleCount = numTriangles;

  mGltfShaderStorageBuffer.uploadSsboData(mModelJointMatrices, 1);
  mGltfDualQuatSSBuffer.uploadSsboData(mModelJointDualQuats, 2);

  mRenderData.rdUploadToUBOTime = mUploadToUBOTimer.stop();

  /* upload vertex data */
  mUploadToVBOTimer.start();

  uploadData(*mLineMesh);

  mRenderData.rdUploadToVBOTime = mUploadToVBOTimer.stop();

  /* draw the glTF models */
  mGltfGPUShader.use();
  /* set SSBO stride, identical for ALL models */
  mGltfGPUShader.setUniformValue(mGltfInstances.at(0)->getJointMatrixSize());
  mGltfModel->drawInstanced(matrixInstances);

  mGltfGPUDualQuatShader.use();
  mGltfGPUDualQuatShader.setUniformValue(mGltfInstances.at(0)->getJointDualQuatsSize());
  mGltfModel->drawInstanced(dualQuatInstances);

  /* draw the coordinate arrow WITH depth buffer */
  if (mCoordArrowsLineIndexCount > 0) {
    mLineShader.use();
    mVertexBuffer.bindAndDraw(GL_LINES, mSkeletonLineIndexCount, mCoordArrowsLineIndexCount);
  }

  /* draw the skeleton, disable depth test to overlay */
  if (mSkeletonLineIndexCount > 0) {
    glDisable(GL_DEPTH_TEST);
    mLineShader.use();
    mVertexBuffer.bindAndDraw(GL_LINES, 0, mSkeletonLineIndexCount);
    glEnable(GL_DEPTH_TEST);
  }

  mFramebuffer.unbind();

  /* blit color buffer to screen */
  mFramebuffer.drawToScreen();

  mUIGenerateTimer.start();

  ModelSettings settings = mGltfInstances.at(selectedInstance)->getInstanceSettings();
  mUserInterface.createFrame(mRenderData, settings);
  mGltfInstances.at(selectedInstance)->setInstanceSettings(settings);
  mGltfInstances.at(selectedInstance)->checkForUpdates();

  mRenderData.rdUIGenerateTime = mUIGenerateTimer.stop();

  mUIDrawTimer.start();
  mUserInterface.render();
  mRenderData.rdUIDrawTime = mUIDrawTimer.stop();

  mLastTickTime = tickTime;
}

void OGLRenderer::cleanup() {
  mGltfModel->cleanup();
  mGltfModel.reset();

  mGltfGPUDualQuatShader.cleanup();
  mGltfGPUShader.cleanup();
  mUserInterface.cleanup();
  mLineShader.cleanup();
  mVertexBuffer.cleanup();
  mGltfShaderStorageBuffer.cleanup();
  mGltfDualQuatSSBuffer.cleanup();
  mUniformBuffer.cleanup();
  mFramebuffer.cleanup();
}
