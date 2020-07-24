/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#define DEBUG_FILTER     1

#include <nvgl/extensions_gl.hpp>

#include <imgui/imgui_helper.h>
#include <imgui/imgui_impl_gl.h>

#include <nvmath/nvmath_glsltypes.h>
#include <nvgl/glsltypes_gl.hpp>

#include <nvh/geometry.hpp>
#include <nvh/misc.hpp>
#include <nvh/fileoperations.hpp>
#include <nvh/cameracontrol.hpp>

#include <nvgl/appwindowprofiler_gl.hpp>
#include <nvgl/error_gl.hpp>
#include <nvgl/programmanager_gl.hpp>
#include <nvgl/base_gl.hpp>

#include "transformsystem.hpp"

#include "cadscene.hpp"
#include "renderer.hpp"

#include <algorithm>

#include "common.h"


namespace csfviewer
{
  int const SAMPLE_SIZE_WIDTH(800);
  int const SAMPLE_SIZE_HEIGHT(600);
  int const SAMPLE_MAJOR_VERSION(4);
  int const SAMPLE_MINOR_VERSION(5);

 
  class Sample : public nvgl::AppWindowProfilerGL 
  {
  public:

    enum GuiEnums {
      GUI_RENDERER,
      GUI_MSAA,
      GUI_SHADE,
      GUI_STRATEGY,
    };

    struct {
      nvgl::ProgramID
        draw_object,
        draw_object_tris,
        draw_object_line,
        draw_object_indexed,
        draw_object_indexed_tris,
        draw_object_indexed_line,

        cull_object_frustum,
        cull_object_hiz,
        cull_object_raster,
        cull_bit_temporallast,
        cull_bit_temporalnew,
        cull_bit_regular,
        cull_depth_mips,

        scan_prefixsum,
        scan_offsets,
        scan_combine,

        transform_leaves,
        transform_level,

        xplode;

    } programs;

    struct {
      GLuint scene = 0;
      GLuint scene2 = 0;
    } fbos;

    struct {
      GLuint scene_ubo = 0;
    } buffers;

    struct {
      GLuint64
        scene_ubo;
    } addresses;

    struct {
      GLuint scene_color = 0;
      GLuint scene_color2 = 0;
      GLuint scene_depthstencil = 0;
      GLuint scene_depthstencil2 = 0;
    } textures;

    struct Tweak {
      int           renderer = 0;
      ShadeType     shade = SHADE_SOLID;
      Strategy      strategy = STRATEGY_GROUPS;
      int           clones = 0;
      bool          cloneaxisX = true;
      bool          cloneaxisY = true;
      bool          cloneaxisZ = false;
      bool          animateActive = false;
      float         animateMin = 1;
      float         animateDelta = 1;
      int           zoom = 100;
      int           msaa = 0;
      bool          noUI = false;
    };

    nvgl::ProgramManager  m_progManager;

    ImGuiH::Registry      m_ui;
    double                m_uiTime = 0;

    Tweak                 m_tweak;
    Tweak                 m_lastTweak;

    std::string           m_modelFilename;

    SceneData             m_sceneUbo;
    CadScene              m_scene;
    TransformSystem       m_transformSystem;

    GLuint                m_xplodeGroupSize;

    std::vector<unsigned int>   m_renderersSorted;
    std::string                 m_rendererName;

    Renderer* NV_RESTRICT       m_renderer;
    Resources                   m_resources;

    size_t                m_stateChangeID;


    void updateProgramDefine();
    bool initProgram();
    bool initScene(const char *filename, int clones, int cloneaxis);
    bool initFramebuffers(int width, int height);
    void initRenderer(int type, Strategy strategy);
    void deinitRenderer();

    void getCullPrograms( CullingSystem::Programs &cullprograms );
    void getScanPrograms( ScanSystem::Programs &scanprograms );
    void getTransformPrograms( TransformSystem::Programs &xfromPrograms);

    void updatedPrograms();

    void setupConfigParameters();
    void setRendererFromName();


  public:

    Sample() 
    {
      setupConfigParameters();
    }

    bool validateConfig() override;

    bool begin() override;
    void think(double time) override;
    void resize(int width, int height) override;

    void processUI(double time);

    nvh::CameraControl m_control;

    void end() override {
      ImGui::ShutdownGL();
    }
    // return true to prevent m_windowState updates
    bool mouse_pos(int x, int y) override {
      if (m_tweak.noUI) return false;
      return ImGuiH::mouse_pos(x, y);
    }
    bool mouse_button(int button, int action) override {
      if (m_tweak.noUI) return false;
      return ImGuiH::mouse_button(button, action);
    }
    bool mouse_wheel(int wheel) override {
      if (m_tweak.noUI) return false;
      return ImGuiH::mouse_wheel(wheel);
    }
    bool key_char(int button) override {
      if (m_tweak.noUI) return false;
      return ImGuiH::key_char(button);
    }
    bool key_button(int button, int action, int mods) override {
      if (m_tweak.noUI) return false;
      return ImGuiH::key_button(button, action, mods);
    }
    
  };

  void Sample::updateProgramDefine()
  {

  }

  void Sample::getTransformPrograms( TransformSystem::Programs &xformPrograms)
  {
    xformPrograms.transform_leaves = m_progManager.get( programs.transform_leaves );
    xformPrograms.transform_level  = m_progManager.get( programs.transform_level );
  }

  void Sample::getCullPrograms( CullingSystem::Programs &cullprograms )
  {
    cullprograms.bit_regular      = m_progManager.get( programs.cull_bit_regular );
    cullprograms.bit_temporallast = m_progManager.get( programs.cull_bit_temporallast );
    cullprograms.bit_temporalnew  = m_progManager.get( programs.cull_bit_temporalnew );
    cullprograms.depth_mips       = m_progManager.get( programs.cull_depth_mips );
    cullprograms.object_frustum   = m_progManager.get( programs.cull_object_frustum );
    cullprograms.object_hiz       = m_progManager.get( programs.cull_object_hiz );
    cullprograms.object_raster    = m_progManager.get( programs.cull_object_raster );
  }

  void Sample::getScanPrograms( ScanSystem::Programs &scanprograms )
  {
    scanprograms.prefixsum  = m_progManager.get( programs.scan_prefixsum );
    scanprograms.offsets    = m_progManager.get( programs.scan_offsets );
    scanprograms.combine    = m_progManager.get( programs.scan_combine );
  }

  bool Sample::initProgram()
  {
    bool validated(true);
    m_progManager.m_filetype = nvh::ShaderFileManager::FILETYPE_GLSL;
    m_progManager.addDirectory( std::string("GLSL_" PROJECT_NAME));
    m_progManager.addDirectory( exePath() + std::string(PROJECT_RELDIRECTORY));

    m_progManager.registerInclude("common.h");

    updateProgramDefine();

    programs.draw_object = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    programs.draw_object_tris = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,    "#define WIREMODE 0\n",  "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define WIREMODE 0\n",  "scene.frag.glsl"));

    programs.draw_object_line = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,    "#define WIREMODE 1\n",  "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define WIREMODE 1\n",  "scene.frag.glsl"));

    programs.draw_object_indexed = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n",  "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n",  "scene.frag.glsl"));

    programs.draw_object_indexed_tris = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n#define WIREMODE 0\n",  "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n#define WIREMODE 0\n",  "scene.frag.glsl"));

    programs.draw_object_indexed_line = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n#define WIREMODE 1\n",  "scene.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n#define WIREMODE 1\n",  "scene.frag.glsl"));


    programs.cull_object_raster = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,   "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_GEOMETRY_SHADER, "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.geo.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.frag.glsl"));

    programs.cull_object_frustum = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-xfb.vert.glsl"));

    programs.cull_object_hiz = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define DUALINDEX 1\n#define MATRICES 4\n#define OCCLUSION\n", "cull-xfb.vert.glsl"));

    programs.cull_bit_regular = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL 0\n", "cull-bitpack.vert.glsl"));
    programs.cull_bit_temporallast = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL TEMPORAL_LAST\n", "cull-bitpack.vert.glsl"));
    programs.cull_bit_temporalnew = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL TEMPORAL_NEW\n", "cull-bitpack.vert.glsl"));

    programs.cull_depth_mips = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_VERTEX_SHADER,   "cull-downsample.vert.glsl"),
      nvgl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "cull-downsample.frag.glsl"));

    programs.scan_prefixsum = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_SUM\n", "scan.comp.glsl"));
    programs.scan_offsets = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_OFFSETS\n", "scan.comp.glsl"));
    programs.scan_combine = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_COMBINE\n", "scan.comp.glsl"));

    programs.transform_leaves = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "transform-leaves.comp.glsl"));
    programs.transform_level = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "transform-level.comp.glsl"));

    programs.xplode = m_progManager.createProgram(
      nvgl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "xplode-animation.comp.glsl"));

    validated = m_progManager.areProgramsValid();

    return validated;
  }

  bool Sample::initScene(const char* filename, int clones, int cloneaxis)
  {
    m_scene.unload();

    if (buffers.scene_ubo && has_GL_NV_shader_buffer_load){
      glMakeNamedBufferNonResidentNV(buffers.scene_ubo);
    }

    nvgl::newBuffer(buffers.scene_ubo);
    glNamedBufferStorage(buffers.scene_ubo, sizeof(SceneData), NULL, GL_DYNAMIC_STORAGE_BIT);

    if (has_GL_NV_shader_buffer_load){
      glGetNamedBufferParameterui64vNV(buffers.scene_ubo, GL_BUFFER_GPU_ADDRESS_NV, &addresses.scene_ubo);
      glMakeNamedBufferResidentNV(buffers.scene_ubo,GL_READ_ONLY);
    }

    m_resources.sceneUbo  = buffers.scene_ubo;
    m_resources.sceneAddr = addresses.scene_ubo;

    m_resources.stateChangeID++;

    bool status = m_scene.loadCSF(filename, clones, cloneaxis);

    LOGI("\nscene %s\n", filename);
    LOGI("geometries: %6d\n", (uint32_t)m_scene.m_geometry.size());
    LOGI("materials:  %6d\n", (uint32_t)m_scene.m_materials.size());
    LOGI("nodes:      %6d\n", (uint32_t)m_scene.m_matrices.size());
    LOGI("objects:    %6d\n", (uint32_t)m_scene.m_objects.size());
    LOGI("\n");

    return status;
  }

  bool Sample::initFramebuffers(int width, int height)
  {
    bool layered = true;
   
    if (!fbos.scene || m_tweak.msaa != m_lastTweak.msaa)
    {
      nvgl::newFramebuffer(fbos.scene);
      nvgl::newFramebuffer(fbos.scene2);

      m_resources.fbo = fbos.scene;
      m_resources.fbo2 = fbos.scene2;

      m_resources.stateChangeID++;
    }

    if (layered){

      if (has_GL_NV_bindless_texture && textures.scene_color){
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
      }

      nvgl::newTexture(textures.scene_color, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY);
      nvgl::newTexture(textures.scene_depthstencil, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY);

      if (m_tweak.msaa){
        glTextureStorage3DMultisample(textures.scene_color,        m_tweak.msaa, GL_RGBA8, width, height, 2, GL_TRUE);
        glTextureStorage3DMultisample(textures.scene_depthstencil, m_tweak.msaa, GL_DEPTH24_STENCIL8, width, height, 2, GL_TRUE);
      }
      else{
        glTextureStorage3D(textures.scene_color,         1, GL_RGBA8, width, height,2);
        glTextureStorage3D(textures.scene_depthstencil,  1, GL_DEPTH24_STENCIL8, width, height,2);
      }

      glNamedFramebufferTextureLayer(fbos.scene, GL_COLOR_ATTACHMENT0,        textures.scene_color, 0,0);
      glNamedFramebufferTextureLayer(fbos.scene, GL_DEPTH_STENCIL_ATTACHMENT, textures.scene_depthstencil, 0,0);

      glNamedFramebufferTextureLayer(fbos.scene2, GL_COLOR_ATTACHMENT0,         textures.scene_color, 0,1);
      glNamedFramebufferTextureLayer(fbos.scene2, GL_DEPTH_STENCIL_ATTACHMENT,  textures.scene_depthstencil, 0,1);

      if (has_GL_NV_bindless_texture){
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
      }
    }
    else{

      if (has_GL_NV_bindless_texture && textures.scene_color){
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color2));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil2));
      }

      nvgl::newTexture(textures.scene_color, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);
      nvgl::newTexture(textures.scene_depthstencil, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);

      if (m_tweak.msaa){
        glTextureStorage2DMultisample(textures.scene_color,        1, GL_RGBA8, width, height, GL_TRUE);
        glTextureStorage2DMultisample(textures.scene_depthstencil, 1, GL_DEPTH24_STENCIL8, width, height, GL_TRUE);
      }
      else{
        glTextureStorage2D(textures.scene_color,         1, GL_RGBA8, width, height);
        glTextureStorage2D(textures.scene_depthstencil,  1, GL_DEPTH24_STENCIL8, width, height);
      }

      glNamedFramebufferTexture(fbos.scene, GL_COLOR_ATTACHMENT0,        textures.scene_color, 0);
      glNamedFramebufferTexture(fbos.scene, GL_DEPTH_STENCIL_ATTACHMENT, textures.scene_depthstencil, 0);

      nvgl::newTexture(textures.scene_color2, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);
      nvgl::newTexture(textures.scene_depthstencil2, m_tweak.msaa ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D);

      if (m_tweak.msaa){
        glTextureStorage2DMultisample(textures.scene_color2,        1, GL_RGBA8, width, height, GL_TRUE);
        glTextureStorage2DMultisample(textures.scene_depthstencil2, 1, GL_DEPTH24_STENCIL8, width, height, GL_TRUE);
      }
      else{
        glTextureStorage2D(textures.scene_color2,         1, GL_RGBA8, width, height);
        glTextureStorage2D(textures.scene_depthstencil2,  1, GL_DEPTH24_STENCIL8, width, height);
      }
      
      glNamedFramebufferTexture(fbos.scene2, GL_COLOR_ATTACHMENT0,        textures.scene_color2, 0);
      glNamedFramebufferTexture(fbos.scene2, GL_DEPTH_STENCIL_ATTACHMENT, textures.scene_depthstencil2, 0);

      if (has_GL_NV_bindless_texture){
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color2));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil2));
      }
    }

    m_resources.fboTextureChangeID++;

    return true;
  }

  void Sample::deinitRenderer()
  {
    if (m_renderer){
      m_renderer->deinit();
      delete m_renderer;
      m_renderer = NULL;
    }
  }

  void Sample::initRenderer(int type, Strategy strategy)
  {
    deinitRenderer();
    Renderer::getRegistry()[m_renderersSorted[type]]->updatedPrograms( m_progManager );
    m_renderer = Renderer::getRegistry()[m_renderersSorted[type]]->create();
    m_renderer->m_strategy = strategy;
    m_renderer->init(&m_scene,m_resources);
  }

  bool Sample::begin()
  {
    m_renderer = NULL;
    m_stateChangeID = 0;

    ImGuiH::Init(m_windowState.m_winSize[0], m_windowState.m_winSize[1], this);
    ImGui::InitGL();

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

#if defined (NDEBUG)
    setVsync(false);
#endif

    Renderer::s_bindless_ubo = !!m_contextWindow.extensionSupported("GL_NV_uniform_buffer_unified_memory");
    LOGI("\nNV_uniform_buffer_unified_memory support: %s\n\n", Renderer::s_bindless_ubo ? "true" : "false");

    bool validated(true);

    GLuint defaultVAO;
    glGenVertexArrays(1, &defaultVAO);
    glBindVertexArray(defaultVAO);

    validated = validated && initProgram();
    validated = validated && initScene(m_modelFilename.c_str(), 0, 3);
    validated = validated && initFramebuffers(m_windowState.m_winSize[0],m_windowState.m_winSize[1]);

    
    const Renderer::Registry registry = Renderer::getRegistry();
    for (size_t i = 0; i < registry.size(); i++)
    {
      if (registry[i]->isAvailable())
      {
        if (!registry[i]->loadPrograms(m_progManager)){
          LOGE("Failed to load resources for renderer %s\n",registry[i]->name());
          return false;
        }

        uint sortkey = uint(i);
        sortkey |= registry[i]->priority() << 16;
        m_renderersSorted.push_back( sortkey );
      }
    }

    std::sort(m_renderersSorted.begin(),m_renderersSorted.end());

    for (size_t i = 0; i < m_renderersSorted.size(); i++){
      m_renderersSorted[i] &= 0xFFFF;

      m_ui.enumAdd(GUI_RENDERER, int(i), registry[m_renderersSorted[i]]->name());
    }

    {
      m_ui.enumAdd(GUI_STRATEGY, STRATEGY_INDIVIDUAL, "drawcall individual");
      m_ui.enumAdd(GUI_STRATEGY, STRATEGY_JOIN, "drawcall join");
      m_ui.enumAdd(GUI_STRATEGY, STRATEGY_GROUPS, "material groups");

      m_ui.enumAdd(GUI_SHADE, SHADE_SOLID, toString(SHADE_SOLID));
      m_ui.enumAdd(GUI_SHADE, SHADE_SOLIDWIRE,toString(SHADE_SOLIDWIRE));
      m_ui.enumAdd(GUI_SHADE, SHADE_SOLIDWIRE_SPLIT,"solid w edges (split test, only in sorted)");

      m_ui.enumAdd(GUI_MSAA, 0, "none");
      m_ui.enumAdd(GUI_MSAA, 2, "2x");
      m_ui.enumAdd(GUI_MSAA, 4, "4x");
      m_ui.enumAdd(GUI_MSAA, 8, "8x");
    }


    m_control.m_sceneOrbit = nvmath::vec3f(m_scene.m_bbox.max+m_scene.m_bbox.min)*0.5f;
    m_control.m_sceneDimension = nvmath::length((m_scene.m_bbox.max-m_scene.m_bbox.min));
    m_control.m_viewMatrix = nvmath::look_at(m_control.m_sceneOrbit - (-vec3(1,1,1)*m_control.m_sceneDimension*0.5f*(float(m_tweak.zoom)/100.0f)), m_control.m_sceneOrbit, vec3(0,1,0));

    m_sceneUbo.wLightPos = (m_scene.m_bbox.max+m_scene.m_bbox.min)*0.5f + m_control.m_sceneDimension;
    m_sceneUbo.wLightPos.w = 1.0;

    updatedPrograms();

    CullingSystem::Programs cullprogs;
    getCullPrograms(cullprogs);
    Renderer::s_cullsys.init(cullprogs,true);

    ScanSystem::Programs scanprogs;
    getScanPrograms(scanprogs);
    Renderer::s_scansys.init(scanprogs);
    //Renderer::s_scansys.test();

    TransformSystem::Programs xformprogs;
    getTransformPrograms(xformprogs);
    m_transformSystem.init(xformprogs);
    

    initRenderer(m_tweak.renderer, m_tweak.strategy);

    return validated;
  }

  void Sample::processUI(double time)
  {
    int width = m_windowState.m_winSize[0];
    int height = m_windowState.m_winSize[1];

    // Update imgui configuration
    auto &imgui_io = ImGui::GetIO();
    imgui_io.DeltaTime = static_cast<float>(time - m_uiTime);
    imgui_io.DisplaySize = ImVec2(width, height);

    m_uiTime = time;

    ImGui::NewFrame();
    ImGui::SetNextWindowSize(ImVec2(350, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("NVIDIA " PROJECT_NAME, nullptr)) {
      m_ui.enumCombobox(GUI_RENDERER, "renderer", &m_tweak.renderer);
      m_ui.enumCombobox(GUI_STRATEGY, "strategy", &m_tweak.strategy);
      m_ui.enumCombobox(GUI_SHADE, "shademode", &m_tweak.shade);
      ImGui::Checkbox("xplode via GPU", &m_tweak.animateActive);
      ImGui::SliderFloat("xplode min", &m_tweak.animateMin, 0, 16.0f);
      ImGui::SliderFloat("xplode delta", &m_tweak.animateDelta, 0, 16.0f);
      ImGuiH::InputIntClamped("clones", &m_tweak.clones, 0, 255, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue);
      ImGui::Checkbox("clone X", &m_tweak.cloneaxisX);
      ImGui::Checkbox("clone Y", &m_tweak.cloneaxisY);
      ImGui::Checkbox("clone Z", &m_tweak.cloneaxisZ);
      m_ui.enumCombobox(GUI_MSAA, "msaa", &m_tweak.msaa);
    }
    if (!m_tweak.cloneaxisX && !m_tweak.cloneaxisY && !m_tweak.cloneaxisZ) {
      m_tweak.cloneaxisX = true;
    }

    ImGui::End();
  }

  void Sample::updatedPrograms()
  {

    CullingSystem::Programs cullprogs;
    getCullPrograms(cullprogs);
    Renderer::s_cullsys.update(cullprogs,true);

    ScanSystem::Programs scanprogs;
    getScanPrograms(scanprogs);
    Renderer::s_scansys.update(scanprogs);

    TransformSystem::Programs xformprogs;
    getTransformPrograms(xformprogs);
    m_transformSystem.update(xformprogs);

    m_resources.programUbo      = m_progManager.get(programs.draw_object);
    m_resources.programUboLine  = m_progManager.get(programs.draw_object_line);
    m_resources.programUboTris  = m_progManager.get(programs.draw_object_tris);
    m_resources.programIdx      = m_progManager.get(programs.draw_object_indexed);
    m_resources.programIdxLine  = m_progManager.get(programs.draw_object_indexed_line);
    m_resources.programIdxTris  = m_progManager.get(programs.draw_object_indexed_tris);

    GLuint groupsizes[3];
    glGetProgramiv(m_progManager.get(programs.xplode),GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)groupsizes);
    m_xplodeGroupSize = groupsizes[0];

    m_resources.stateChangeID++;
  }

  void Sample::think(double time)
  {
    NV_PROFILE_GL_SECTION("Frame");

    processUI(time);

    m_control.processActions(m_windowState.m_winSize,
      nvmath::vec2f(m_windowState.m_mouseCurrent[0],m_windowState.m_mouseCurrent[1]),
      m_windowState.m_mouseButtonFlags, m_windowState.m_mouseWheel);

    if (m_windowState.onPress(KEY_R)){
      m_progManager.reloadPrograms();
      Renderer::getRegistry()[m_tweak.renderer]->updatedPrograms( m_progManager );
      updatedPrograms();
    }

    if (m_tweak.msaa != m_lastTweak.msaa){
      initFramebuffers(m_windowState.m_winSize[0],m_windowState.m_winSize[1]);
    }

    if (m_tweak.clones    != m_lastTweak.clones ||
      m_tweak.cloneaxisX  != m_lastTweak.cloneaxisX ||
      m_tweak.cloneaxisY  != m_lastTweak.cloneaxisY ||
      m_tweak.cloneaxisZ  != m_lastTweak.cloneaxisZ)
    {
      deinitRenderer();
      initScene( m_modelFilename.c_str(), m_tweak.clones, (int(m_tweak.cloneaxisX)<<0) | (int(m_tweak.cloneaxisY)<<1) | (int(m_tweak.cloneaxisZ)<<2) );
    }

    if (m_tweak.renderer != m_lastTweak.renderer ||
        m_tweak.strategy != m_lastTweak.strategy ||
        m_tweak.cloneaxisX  != m_lastTweak.cloneaxisX ||
        m_tweak.cloneaxisY  != m_lastTweak.cloneaxisY ||
        m_tweak.cloneaxisZ  != m_lastTweak.cloneaxisZ ||
        m_tweak.clones   != m_lastTweak.clones)
    {
      initRenderer(m_tweak.renderer,m_tweak.strategy);
    }

    if (!m_tweak.animateActive && m_lastTweak.animateActive )
    {
      m_scene.resetMatrices();
    }
    
    m_lastTweak = m_tweak;

    int width   = m_windowState.m_winSize[0];
    int height  = m_windowState.m_winSize[1];

    {
      // generic state setup
      glViewport(0, 0, width, height);

      if (m_tweak.shade == SHADE_SOLIDWIRE_SPLIT){
        glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene2);
        glClearColor(0.2f,0.2f,0.2f,0.0f);
        glClearDepth(1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
      }

      glBindFramebuffer(GL_FRAMEBUFFER, fbos.scene);
      glClearColor(0.2f,0.2f,0.2f,0.0f);
      glClearDepth(1.0);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

      glEnable(GL_DEPTH_TEST);

      m_sceneUbo.viewport = ivec2(width,height);

      nvmath::mat4 projection = nvmath::perspective((45.f), float(width)/float(height), m_control.m_sceneDimension*0.001f, m_control.m_sceneDimension*10.0f);
      nvmath::mat4 view = m_control.m_viewMatrix;

      m_sceneUbo.viewProjMatrix = projection * view;
      m_sceneUbo.viewMatrix = view;
      m_sceneUbo.viewMatrixIT = nvmath::transpose(nvmath::invert(view));

      m_sceneUbo.viewPos = m_sceneUbo.viewMatrixIT.row(3);
      m_sceneUbo.viewDir = -view.row(2);

      m_sceneUbo.wLightPos = m_sceneUbo.viewMatrixIT.row(3);
      m_sceneUbo.wLightPos.w = 1.0;

      m_sceneUbo.tboMatrices = uvec2(m_scene.m_matricesTexGLADDR & 0xFFFFFFFF, m_scene.m_matricesTexGLADDR >> 32);

      glNamedBufferSubData(buffers.scene_ubo,0,sizeof(SceneData),&m_sceneUbo);

      glDisable(GL_CULL_FACE);
    }

    if (m_tweak.animateActive)
    {
      {
        NV_PROFILE_GL_SECTION("Xplode");

        float speed = 0.5;
        float scale = m_tweak.animateMin + (cosf(float(time) * speed) * 0.5f + 0.5f) * (m_tweak.animateDelta);
        GLuint   totalNodes = GLuint(m_scene.m_matrices.size());
        GLuint   groupsize = m_xplodeGroupSize;

        glUseProgram(m_progManager.get(programs.xplode));
        glUniform1f(0, scale);
        glUniform1i(1, totalNodes);

        nvgl::bindMultiTexture(GL_TEXTURE0, GL_TEXTURE_BUFFER, m_scene.m_matricesOrigTexGL);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_scene.m_matricesGL);

        glDispatchCompute((totalNodes+groupsize-1)/groupsize,1,1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        nvgl::bindMultiTexture(GL_TEXTURE0, GL_TEXTURE_BUFFER, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glUseProgram(0);
      }

      {
        NV_PROFILE_GL_SECTION("Tree");
        TransformSystem::Buffer ids;
        TransformSystem::Buffer world;
        TransformSystem::Buffer object;

        ids.buffer = m_scene.m_parentIDsGL;
        ids.offset = 0;
        ids.size   = sizeof(GLuint)*m_scene.m_matrices.size();

        world.buffer = m_scene.m_matricesGL;
        world.offset = 0;
        world.size   = sizeof(CadScene::MatrixNode)*m_scene.m_matrices.size();

        object.buffer = m_scene.m_matricesGL;
        object.offset = 0;
        object.size   = sizeof(CadScene::MatrixNode)*m_scene.m_matrices.size();
        
        m_transformSystem.process(m_scene.m_nodeTree, ids, object, world);
      }
    }

    {
      NV_PROFILE_GL_SECTION("Render");

      m_resources.cullView.viewPos = m_sceneUbo.viewPos.get_value();
      m_resources.cullView.viewDir = m_sceneUbo.viewDir.get_value();
      m_resources.cullView.viewProjMatrix = m_sceneUbo.viewProjMatrix.get_value();

      m_renderer->draw(m_tweak.shade,m_resources,m_profiler,m_progManager);
    }


    {
      NV_PROFILE_GL_SECTION("Blit");


      if (m_tweak.shade == SHADE_SOLIDWIRE_SPLIT){
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        int wh = width/2;
        int hh = height/2;

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos.scene);
        glBlitFramebuffer(0,0,wh,hh,
          0,0,wh,hh,GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBlitFramebuffer(wh,hh,width,height,
          wh,hh,width,height,GL_COLOR_BUFFER_BIT, GL_NEAREST);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos.scene2);
        glBlitFramebuffer(wh,0,width,hh,
          wh,0,width,hh,GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBlitFramebuffer(0,hh,wh,height,
          0,hh,wh,height,GL_COLOR_BUFFER_BIT, GL_NEAREST);
      }
      else{
        // blit to background
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbos.scene);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0,0,width,height,
          0,0,width,height,GL_COLOR_BUFFER_BIT, GL_NEAREST);
      }
    }
    
    if(!m_tweak.noUI){
      NV_PROFILE_GL_SECTION("GUI");
      ImGui::Render();
      ImGui::RenderDrawDataGL(ImGui::GetDrawData());
    }

    ImGui::EndFrame();

    m_lastTweak = m_tweak;
  }

  void Sample::resize(int width, int height)
  {
    initFramebuffers(width,height);
  }

  void Sample::setRendererFromName()
  {
    if (!m_rendererName.empty()) {
      const Renderer::Registry registry = Renderer::getRegistry();
      for (size_t i = 0; i < m_renderersSorted.size(); i++) {
        if (strcmp(m_rendererName.c_str(), registry[m_renderersSorted[i]]->name()) == 0) {
          m_tweak.renderer = int(i);
        }
      }
    }
  }

  static std::string addPath(std::string const &defaultPath, std::string const &filename)
  {
    if (
#ifdef _WIN32
      filename.find(':') != std::string::npos
#else
      !filename.empty() && filename[0] == '/'
#endif
      )
    {
      return filename;
    }
    else {
      return defaultPath + "/" + filename;
    }
  }

  static bool endsWith(std::string const &s, std::string const &end) {
    if (s.length() >= end.length()) {
      return (0 == s.compare(s.length() - end.length(), end.length(), end));
    }
    else {
      return false;
    }

  }

  void Sample::setupConfigParameters()
  {
    m_parameterList.addFilename(".csf", &m_modelFilename);
    m_parameterList.addFilename(".csf.gz", &m_modelFilename);
    m_parameterList.addFilename(".gltf", &m_modelFilename);

    m_parameterList.add("noui", &m_tweak.noUI, false);

    m_parameterList.add("renderer", (uint32_t*)&m_tweak.renderer);
    m_parameterList.add("renderernamed", &m_rendererName);
    m_parameterList.add("strategy", (uint32_t*)&m_tweak.strategy);
    m_parameterList.add("shademode", (uint32_t*)&m_tweak.shade);
    m_parameterList.add("msaa", &m_tweak.msaa);
    m_parameterList.add("clones", &m_tweak.clones);
    m_parameterList.add("xplode", &m_tweak.animateActive);
    m_parameterList.add("zoom", &m_tweak.zoom);
  }


  bool Sample::validateConfig()
  {
    if (m_modelFilename.empty())
    {
      LOGI("no .csf model file specified\n");
      LOGI("exe <filename.csf/cfg> parameters...\n");
      m_parameterList.print();
      return false;
    }
    return true;
  }

}

using namespace csfviewer;

int main(int argc, const char** argv)
{
  NVPSystem system(argv[0], PROJECT_NAME);

  Sample sample;

  {
    std::vector<std::string> directories;
    directories.push_back(".");
    directories.push_back(NVPSystem::exePath() + std::string(PROJECT_RELDIRECTORY));
    sample.m_modelFilename = nvh::findFile(std::string("geforce.csf.gz"), directories);
  }
  
  return sample.run(
    PROJECT_NAME,
    argc, argv,
    SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT);
}

