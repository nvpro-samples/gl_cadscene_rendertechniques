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

#include <GL/glew.h>
#include <nv_helpers/anttweakbar.hpp>
#include <nv_helpers_gl/WindowProfiler.hpp>
#include <nv_math/nv_math_glsltypes.h>

#include <nv_helpers_gl/error.hpp>
#include <nv_helpers_gl/programmanager.hpp>
#include <nv_helpers/geometry.hpp>
#include <nv_helpers/misc.hpp>
#include <nv_helpers_gl/glresources.hpp>
#include <nv_helpers/cameracontrol.hpp>

#include "transformsystem.hpp"

#include "cadscene.hpp"
#include "renderer.hpp"

#include <algorithm>

using namespace nv_helpers;
using namespace nv_helpers_gl;
using namespace nv_math;
#include "common.h"


namespace csfviewer
{
  int const SAMPLE_SIZE_WIDTH(800);
  int const SAMPLE_SIZE_HEIGHT(600);
  int const SAMPLE_MAJOR_VERSION(4);
  int const SAMPLE_MINOR_VERSION(3);

 
  class Sample : public nv_helpers_gl::WindowProfiler 
  {
  public:

    ProgramManager progManager;
    
    struct {
      ProgramManager::ProgramID
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
      ResourceGLuint
        scene,
        scene2;
    } fbos;

    struct {
      ResourceGLuint  
        scene_ubo;
    } buffers;

    struct {
      GLuint64
        scene_ubo;
    } addresses;

    struct {
      ResourceGLuint
        scene_color,
        scene_color2,
        scene_depthstencil,
        scene_depthstencil2;
    } textures;

    struct Tweak {
      int           renderer;
      ShadeType     shade;
      Strategy      strategy;
      int           clones;
      int           cloneaxisX;
      int           cloneaxisY;
      int           cloneaxisZ;
      bool          animateActive;
      float         animateMin;
      float         animateDelta;
      int           zoom;
      int           msaa;
      bool          noUI;

      Tweak() 
        : renderer(0)
        , shade(SHADE_SOLID)
        , strategy(STRATEGY_GROUPS)
        , clones(0)
        , cloneaxisX(1)
        , cloneaxisY(1)
        , cloneaxisZ(0)
        , animateActive(false)
        , animateMin(1)
        , animateDelta(1)
        , zoom(100)
        , msaa(0)
        , noUI(false)
      {}
    };

    Tweak                 tweak;
    Tweak                 lastTweak;

    SceneData             sceneUbo;
    CadScene              cadscene;
    TransformSystem       transformSystem;
    GLuint                xplodeGroupSize;


    std::vector<unsigned int>  sortedRenderers;

    Renderer* NV_RESTRICT  renderer;
    Resources             resources;

    std::string           filename;
    size_t                stateIncarnation;


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

  public:

    Sample() 
    {

    }

    void parse(int argc, const char**argv);

    bool begin();
    void think(double time);
    void resize(int width, int height);

    CameraControl m_control;

    void end() {
      TwTerminate();
    }
    // return true to prevent m_window updates
    bool mouse_pos    (int x, int y) {
      if (tweak.noUI) return false;
      return !!TwEventMousePosGLFW(x,y); 
    }
    bool mouse_button (int button, int action) {
      if (tweak.noUI) return false;
      return !!TwEventMouseButtonGLFW(button, action);
    }
    bool mouse_wheel  (int wheel) {
      if (tweak.noUI) return false;
      return !!TwEventMouseWheelGLFW(wheel); 
    }
    bool key_button   (int button, int action, int mods) {
      if (tweak.noUI) return false;
      return handleTwKeyPressed(button,action,mods);
    }
    
  };

  void Sample::updateProgramDefine()
  {

  }

  void Sample::getTransformPrograms( TransformSystem::Programs &xformPrograms)
  {
    xformPrograms.transform_leaves = progManager.get( programs.transform_leaves );
    xformPrograms.transform_level  = progManager.get( programs.transform_level );
  }

  void Sample::getCullPrograms( CullingSystem::Programs &cullprograms )
  {
    cullprograms.bit_regular      = progManager.get( programs.cull_bit_regular );
    cullprograms.bit_temporallast = progManager.get( programs.cull_bit_temporallast );
    cullprograms.bit_temporalnew  = progManager.get( programs.cull_bit_temporalnew );
    cullprograms.depth_mips       = progManager.get( programs.cull_depth_mips );
    cullprograms.object_frustum   = progManager.get( programs.cull_object_frustum );
    cullprograms.object_hiz       = progManager.get( programs.cull_object_hiz );
    cullprograms.object_raster    = progManager.get( programs.cull_object_raster );
  }

  void Sample::getScanPrograms( ScanSystem::Programs &scanprograms )
  {
    scanprograms.prefixsum  = progManager.get( programs.scan_prefixsum );
    scanprograms.offsets    = progManager.get( programs.scan_offsets );
    scanprograms.combine    = progManager.get( programs.scan_combine );
  }

  bool Sample::initProgram()
  {
    bool validated(true);
    progManager.addDirectory( std::string(PROJECT_NAME));
    progManager.addDirectory( sysExePath() + std::string(PROJECT_RELDIRECTORY));
    progManager.addDirectory( std::string(PROJECT_ABSDIRECTORY));

    progManager.registerInclude("common.h", "common.h");

    updateProgramDefine();

    programs.draw_object = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,          "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,        "scene.frag.glsl"));

    programs.draw_object_tris = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,    "#define WIREMODE 0\n",  "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define WIREMODE 0\n",  "scene.frag.glsl"));

    programs.draw_object_line = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,    "#define WIREMODE 1\n",  "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define WIREMODE 1\n",  "scene.frag.glsl"));

    programs.draw_object_indexed = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n",  "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n",  "scene.frag.glsl"));

    programs.draw_object_indexed_tris = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n#define WIREMODE 0\n",  "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n#define WIREMODE 0\n",  "scene.frag.glsl"));

    programs.draw_object_indexed_line = progManager.createProgram(
      ProgramManager::Definition(GL_VERTEX_SHADER,    "#define USE_INDEXING 1\n#define WIREMODE 1\n",  "scene.vert.glsl"),
      ProgramManager::Definition(GL_FRAGMENT_SHADER,  "#define USE_INDEXING 1\n#define WIREMODE 1\n",  "scene.frag.glsl"));


    programs.cull_object_raster = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,   "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.vert.glsl"),
      nv_helpers_gl::ProgramManager::Definition(GL_GEOMETRY_SHADER, "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.geo.glsl"),
      nv_helpers_gl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-raster.frag.glsl"));

    programs.cull_object_frustum = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define DUALINDEX 1\n#define MATRICES 4\n", "cull-xfb.vert.glsl"));

    programs.cull_object_hiz = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define DUALINDEX 1\n#define MATRICES 4\n#define OCCLUSION\n", "cull-xfb.vert.glsl"));

    programs.cull_bit_regular = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL 0\n", "cull-bitpack.vert.glsl"));
    programs.cull_bit_temporallast = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL TEMPORAL_LAST\n", "cull-bitpack.vert.glsl"));
    programs.cull_bit_temporalnew = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,  "#define TEMPORAL TEMPORAL_NEW\n", "cull-bitpack.vert.glsl"));

    programs.cull_depth_mips = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_VERTEX_SHADER,   "cull-downsample.vert.glsl"),
      nv_helpers_gl::ProgramManager::Definition(GL_FRAGMENT_SHADER, "cull-downsample.frag.glsl"));

    programs.scan_prefixsum = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_SUM\n", "scan.comp.glsl"));
    programs.scan_offsets = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_OFFSETS\n", "scan.comp.glsl"));
    programs.scan_combine = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "#define TASK TASK_COMBINE\n", "scan.comp.glsl"));

    programs.transform_leaves = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "transform-leaves.comp.glsl"));
    programs.transform_level = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "transform-level.comp.glsl"));

    programs.xplode = progManager.createProgram(
      nv_helpers_gl::ProgramManager::Definition(GL_COMPUTE_SHADER,  "xplode-animation.comp.glsl"));

    validated = progManager.areProgramsValid();

    return validated;
  }

  bool Sample::initScene(const char* filename, int clones, int cloneaxis)
  {
    cadscene.unload();

    if (buffers.scene_ubo && GLEW_NV_shader_buffer_load){
      glMakeNamedBufferNonResidentNV(buffers.scene_ubo);
    }

    newBuffer(buffers.scene_ubo);
    glNamedBufferStorageEXT(buffers.scene_ubo, sizeof(SceneData), NULL, GL_DYNAMIC_STORAGE_BIT);

    if (GLEW_NV_shader_buffer_load){
      glGetNamedBufferParameterui64vNV(buffers.scene_ubo, GL_BUFFER_GPU_ADDRESS_NV, &addresses.scene_ubo);
      glMakeNamedBufferResidentNV(buffers.scene_ubo,GL_READ_ONLY);
    }

    resources.sceneUbo  = buffers.scene_ubo;
    resources.sceneAddr = addresses.scene_ubo;

    resources.stateIncarnation++;

    bool status = cadscene.loadCSF(filename, clones, cloneaxis);

    printf("\nscene %s\n", filename);
    printf("geometries: %6d\n", cadscene.m_geometry.size());
    printf("materials:  %6d\n", cadscene.m_materials.size());
    printf("nodes:      %6d\n", cadscene.m_matrices.size());
    printf("objects:    %6d\n", cadscene.m_objects.size());
    printf("\n");

    return status;
  }

  bool Sample::initFramebuffers(int width, int height)
  {
    bool layered = true;
   
    if (!fbos.scene || tweak.msaa != lastTweak.msaa)
    {
      newFramebuffer(fbos.scene);
      newFramebuffer(fbos.scene2);

      resources.fbo = fbos.scene;
      resources.fbo2 = fbos.scene2;

      resources.stateIncarnation++;
    }

    if (layered){

      if (GLEW_NV_bindless_texture && textures.scene_color){
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
      }

      newTexture(textures.scene_color);
      newTexture(textures.scene_depthstencil);

      if (tweak.msaa){
        glTextureStorage3DMultisampleEXT(textures.scene_color,        GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tweak.msaa, GL_RGBA8, width, height, 2, GL_TRUE);
        glTextureStorage3DMultisampleEXT(textures.scene_depthstencil, GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tweak.msaa, GL_DEPTH24_STENCIL8, width, height, 2, GL_TRUE);
      }
      else{
        glTextureStorage3DEXT(textures.scene_color, GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, width, height,2);
        glTextureStorage3DEXT(textures.scene_depthstencil, GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH24_STENCIL8, width, height,2);
      }

      glNamedFramebufferTextureLayerEXT(fbos.scene, GL_COLOR_ATTACHMENT0,     textures.scene_color, 0,0);
      glNamedFramebufferTextureLayerEXT(fbos.scene, GL_DEPTH_STENCIL_ATTACHMENT, textures.scene_depthstencil, 0,0);

      glNamedFramebufferTextureLayerEXT(fbos.scene2, GL_COLOR_ATTACHMENT0,     textures.scene_color, 0,1);
      glNamedFramebufferTextureLayerEXT(fbos.scene2, GL_DEPTH_STENCIL_ATTACHMENT, textures.scene_depthstencil, 0,1);

      if (GLEW_NV_bindless_texture){
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
      }
    }
    else{

      if (GLEW_NV_bindless_texture && textures.scene_color){
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_color2));
        glMakeTextureHandleNonResidentNV(glGetTextureHandleNV(textures.scene_depthstencil2));
      }

      newTexture(textures.scene_color);
      newTexture(textures.scene_depthstencil);

      if (tweak.msaa){
        glTextureStorage2DMultisampleEXT(textures.scene_color, GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, width, height, GL_TRUE);
        glTextureStorage2DMultisampleEXT(textures.scene_depthstencil, GL_TEXTURE_2D_MULTISAMPLE, 1, GL_DEPTH24_STENCIL8, width, height, GL_TRUE);
      }
      else{
        glTextureStorage2DEXT(textures.scene_color, GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        glTextureStorage2DEXT(textures.scene_depthstencil, GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width, height);
      }

      glNamedFramebufferTexture2DEXT(fbos.scene, GL_COLOR_ATTACHMENT0,        GL_TEXTURE_2D, textures.scene_color, 0);
      glNamedFramebufferTexture2DEXT(fbos.scene, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textures.scene_depthstencil, 0);

      newTexture(textures.scene_color2);
      newTexture(textures.scene_depthstencil2);

      if (tweak.msaa){
        glTextureStorage2DMultisampleEXT(textures.scene_color2, GL_TEXTURE_2D_MULTISAMPLE, 1, GL_RGBA8, width, height, GL_TRUE);
        glTextureStorage2DMultisampleEXT(textures.scene_depthstencil2, GL_TEXTURE_2D_MULTISAMPLE, 1, GL_DEPTH24_STENCIL8, width, height, GL_TRUE);
      }
      else{
        glTextureStorage2DEXT(textures.scene_color2, GL_TEXTURE_2D, 1, GL_RGBA8, width, height);
        glTextureStorage2DEXT(textures.scene_depthstencil2, GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, width, height);
      }

      glNamedFramebufferTexture2DEXT(fbos.scene2, GL_COLOR_ATTACHMENT0,        GL_TEXTURE_2D, textures.scene_color2, 0);
      glNamedFramebufferTexture2DEXT(fbos.scene2, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, textures.scene_depthstencil2, 0);

      if (GLEW_NV_bindless_texture){
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_color2));
        glMakeTextureHandleResidentNV(glGetTextureHandleNV(textures.scene_depthstencil2));
      }
    }

    resources.fboTextureIncarnation++;

    return true;
  }

  void Sample::deinitRenderer()
  {
    if (renderer){
      renderer->deinit();
      delete renderer;
      renderer = NULL;
    }
  }

  void Sample::initRenderer(int type, Strategy strategy)
  {
    deinitRenderer();
    Renderer::getRegistry()[sortedRenderers[type]]->updatedPrograms( progManager );
    renderer = Renderer::getRegistry()[sortedRenderers[type]]->create();
    renderer->m_strategy = strategy;
    renderer->init(&cadscene,resources);
  }

  bool Sample::begin()
  {
    renderer = NULL;
    stateIncarnation = 0;

    TwInit(TW_OPENGL_CORE,NULL);
    TwWindowSize(m_window.m_viewsize[0],m_window.m_viewsize[1]);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

#if defined (NDEBUG)
    vsync(false);
#endif

    Renderer::s_bindless_ubo = !!NVPWindow::sysExtensionSupported("GL_NV_uniform_buffer_unified_memory");
    printf("\nNV_uniform_buffer_unified_memory support: %s\n\n", Renderer::s_bindless_ubo ? "true" : "false");

    bool validated(true);

    GLuint defaultVAO;
    glGenVertexArrays(1, &defaultVAO);
    glBindVertexArray(defaultVAO);

    validated = validated && initProgram();
    validated = validated && initScene(filename.c_str(), 0, 3);
    validated = validated && initFramebuffers(m_window.m_viewsize[0],m_window.m_viewsize[1]);

    TwBar *bar = TwNewBar("mainbar");
    TwDefine(" GLOBAL contained=true help='OpenGL samples.\nCopyright NVIDIA Corporation 2013-2014' ");
    TwDefine(" mainbar position='0 0' size='320 220' color='0 0 0' alpha=128 valueswidth=170 ");
    TwDefine((std::string(" mainbar label='") + PROJECT_NAME + "'").c_str());
    
    
    const Renderer::Registry registry = Renderer::getRegistry();
    for (size_t i = 0; i < registry.size(); i++)
    {
      if (registry[i]->isAvailable())
      {
        if (!registry[i]->loadPrograms(progManager)){
          fprintf(stderr,"Failed to load resources for renderer %s\n",registry[i]->name());
          return false;
        }

        uint sortkey = uint(i);
        sortkey |= registry[i]->priority() << 16;
        sortedRenderers.push_back( sortkey );
      }
    }

    std::sort(sortedRenderers.begin(),sortedRenderers.end());

    std::vector<TwEnumVal>  rendererVals;

    for (size_t i = 0; i < sortedRenderers.size(); i++){
      sortedRenderers[i] &= 0xFFFF;

      TwEnumVal eval;
      eval.Value = int(i);
      eval.Label = registry[sortedRenderers[i]]->name();
      rendererVals.push_back(eval);
    }

    TwType rendererType = TwDefineEnum("renderer", &rendererVals[0], int(rendererVals.size()));
    TwEnumVal strategyVals[] = {
      {STRATEGY_INDIVIDUAL, "drawcall individual"},
      {STRATEGY_JOIN,       "drawcall join"},
      {STRATEGY_GROUPS,     "material groups"},
    };
    TwType strategyType = TwDefineEnum("strategy", strategyVals, sizeof(strategyVals)/sizeof(strategyVals[0]));
    TwEnumVal shadeVals[] = {
      {SHADE_SOLID,toString(SHADE_SOLID)},
      {SHADE_SOLIDWIRE,toString(SHADE_SOLIDWIRE)},
      {SHADE_SOLIDWIRE_SPLIT,"solid w edges (split test, only in sorted)"},
    };
    TwType shadeType = TwDefineEnum("shade", shadeVals, sizeof(shadeVals)/sizeof(shadeVals[0]));
    TwEnumVal msaaVals[] = {
      {0,"none"},
      {2,"2x"},
      {4,"4x"},
      {8,"8x"},
    };
    TwType msaaType = TwDefineEnum("msaa", msaaVals, sizeof(msaaVals)/sizeof(msaaVals[0]));
    TwAddVarRW(bar, "renderer", rendererType,  &tweak.renderer,  " label='renderer' ");
    TwAddVarRW(bar, "strategy", strategyType,  &tweak.strategy,  " label='strategy' ");
    TwAddVarRW(bar, "shademode", shadeType,    &tweak.shade,  " label='shademode' ");
    TwAddVarRW(bar, "animate", TW_TYPE_BOOLCPP, &tweak.animateActive, "label='xplode via GPU' ");
    TwAddVarRW(bar, "animateMin", TW_TYPE_FLOAT, &tweak.animateMin, "label='xplode min' ");
    TwAddVarRW(bar, "animateDelta", TW_TYPE_FLOAT, &tweak.animateDelta, "label='xplode delta' ");
    TwAddVarRW(bar, "clones", TW_TYPE_INT32,   &tweak.clones,  " label='clones' min=0 max=255 ");
    TwAddVarRW(bar, "cloneX", TW_TYPE_BOOL32,  &tweak.cloneaxisX,  " label='clone X' ");
    TwAddVarRW(bar, "cloneY", TW_TYPE_BOOL32,  &tweak.cloneaxisY,  " label='clone Y' ");
    TwAddVarRW(bar, "cloneZ", TW_TYPE_BOOL32,  &tweak.cloneaxisZ,  " label='clone Z' ");
    TwAddVarRW(bar, "msaa", msaaType,  &tweak.msaa,  " label='msaa' ");
    

    m_control.m_sceneOrbit = nv_math::vec3f(cadscene.m_bbox.max+cadscene.m_bbox.min)*0.5f;
    m_control.m_sceneDimension = nv_math::length((cadscene.m_bbox.max-cadscene.m_bbox.min));
    m_control.m_viewMatrix = nv_math::look_at(m_control.m_sceneOrbit - (-vec3(1,1,1)*m_control.m_sceneDimension*0.5f*(float(tweak.zoom)/100.0f)), m_control.m_sceneOrbit, vec3(0,1,0));

    sceneUbo.wLightPos = (cadscene.m_bbox.max+cadscene.m_bbox.min)*0.5f + m_control.m_sceneDimension;
    sceneUbo.wLightPos.w = 1.0;

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
    transformSystem.init(xformprogs);
    

    initRenderer(tweak.renderer, tweak.strategy);

    return validated;
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
    transformSystem.update(xformprogs);

    resources.programUbo      = progManager.get(programs.draw_object);
    resources.programUboLine  = progManager.get(programs.draw_object_line);
    resources.programUboTris  = progManager.get(programs.draw_object_tris);
    resources.programIdx      = progManager.get(programs.draw_object_indexed);
    resources.programIdxLine  = progManager.get(programs.draw_object_indexed_line);
    resources.programIdxTris  = progManager.get(programs.draw_object_indexed_tris);

    GLuint groupsizes[3];
    glGetProgramiv(progManager.get(programs.xplode),GL_COMPUTE_WORK_GROUP_SIZE, (GLint*)groupsizes);
    xplodeGroupSize = groupsizes[0];

    resources.stateIncarnation++;
  }


  void Sample::think(double time)
  {
    m_control.processActions(m_window.m_viewsize,
      nv_math::vec2f(m_window.m_mouseCurrent[0],m_window.m_mouseCurrent[1]),
      m_window.m_mouseButtonFlags, m_window.m_wheel);

    if (m_window.onPress(KEY_R)){
      progManager.reloadPrograms();
      Renderer::getRegistry()[tweak.renderer]->updatedPrograms( progManager );
      updatedPrograms();
    }

    if (tweak.msaa != lastTweak.msaa){
      initFramebuffers(m_window.m_viewsize[0],m_window.m_viewsize[1]);
    }

    if (tweak.clones    != lastTweak.clones ||
      tweak.cloneaxisX  != lastTweak.cloneaxisX ||
      tweak.cloneaxisY  != lastTweak.cloneaxisY ||
      tweak.cloneaxisZ  != lastTweak.cloneaxisZ)
    {
      deinitRenderer();
      initScene( filename.c_str(), tweak.clones, (tweak.cloneaxisX<<0) | (tweak.cloneaxisY<<1) | (tweak.cloneaxisZ<<2) );
    }

    if (tweak.renderer != lastTweak.renderer ||
        tweak.strategy != lastTweak.strategy ||
        tweak.cloneaxisX  != lastTweak.cloneaxisX ||
        tweak.cloneaxisY  != lastTweak.cloneaxisY ||
        tweak.cloneaxisZ  != lastTweak.cloneaxisZ ||
        tweak.clones   != lastTweak.clones)
    {
      initRenderer(tweak.renderer,tweak.strategy);
    }

    if (!tweak.animateActive && lastTweak.animateActive )
    {
      cadscene.resetMatrices();
    }
    
    lastTweak = tweak;

    int width   = m_window.m_viewsize[0];
    int height  = m_window.m_viewsize[1];

    {
      // generic state setup
      glViewport(0, 0, width, height);

      if (tweak.shade == SHADE_SOLIDWIRE_SPLIT){
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

      sceneUbo.viewport = ivec2(width,height);

      nv_math::mat4 projection = nv_math::perspective((45.f), float(width)/float(height), m_control.m_sceneDimension*0.001f, m_control.m_sceneDimension*10.0f);
      nv_math::mat4 view = m_control.m_viewMatrix;

      sceneUbo.viewProjMatrix = projection * view;
      sceneUbo.viewMatrix = view;
      sceneUbo.viewMatrixIT = nv_math::transpose(nv_math::invert(view));

      sceneUbo.viewPos = -view.col(3);
      sceneUbo.viewDir = -view.row(2);

      sceneUbo.wLightPos = sceneUbo.viewMatrixIT.row(3);
      sceneUbo.wLightPos.w = 1.0;

      sceneUbo.tboMatrices = uvec2(cadscene.m_matricesTexGLADDR & 0xFFFFFFFF, cadscene.m_matricesTexGLADDR >> 32);

      glNamedBufferSubDataEXT(buffers.scene_ubo,0,sizeof(SceneData),&sceneUbo);

      glDisable(GL_CULL_FACE);
    }

    if (tweak.animateActive)
    {
      {
        NV_PROFILE_SECTION("Xplode");

        float speed = 0.5;
        float scale = tweak.animateMin + (cosf(float(time) * speed) * 0.5f + 0.5f) * (tweak.animateDelta);
        GLuint   totalNodes = GLuint(cadscene.m_matrices.size());
        GLuint   groupsize = xplodeGroupSize;

        glUseProgram(progManager.get(programs.xplode));
        glUniform1f(0, scale);
        glUniform1i(1, totalNodes);

        glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, cadscene.m_matricesOrigTexGL);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cadscene.m_matricesGL);

        glDispatchCompute((totalNodes+groupsize-1)/groupsize,1,1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        glBindMultiTextureEXT(GL_TEXTURE0, GL_TEXTURE_BUFFER, 0);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, 0);
        glUseProgram(0);
      }

      {
        NV_PROFILE_SECTION("Tree");
        TransformSystem::Buffer ids;
        TransformSystem::Buffer world;
        TransformSystem::Buffer object;

        ids.buffer = cadscene.m_parentIDsGL;
        ids.offset = 0;
        ids.size   = sizeof(GLuint)*cadscene.m_matrices.size();

        world.buffer = cadscene.m_matricesGL;
        world.offset = 0;
        world.size   = sizeof(CadScene::MatrixNode)*cadscene.m_matrices.size();

        object.buffer = cadscene.m_matricesGL;
        object.offset = 0;
        object.size   = sizeof(CadScene::MatrixNode)*cadscene.m_matrices.size();
        
        transformSystem.process(cadscene.m_nodeTree, ids, object, world);
      }
    }

    {
      NV_PROFILE_SECTION("Render");

      resources.cullView.viewPos = sceneUbo.viewPos.get_value();
      resources.cullView.viewDir = sceneUbo.viewDir.get_value();
      resources.cullView.viewProjMatrix = sceneUbo.viewProjMatrix.get_value();

      renderer->draw(tweak.shade,resources,m_profiler,progManager);
    }


    {
      NV_PROFILE_SECTION("Blit");


      if (tweak.shade == SHADE_SOLIDWIRE_SPLIT){
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
    
    if (!tweak.noUI){
      NV_PROFILE_SECTION("TwDraw");
      TwDraw();
    }

    lastTweak = tweak;
  }

  void Sample::resize(int width, int height)
  {
    TwWindowSize(width,height);
    initFramebuffers(width,height);
  }

  void Sample::parse( int argc, const char**argv )
  {
    filename = sysExePath() + std::string(PROJECT_RELDIRECTORY) + "/geforce.csf.gz";

    for (int i = 0; i < argc; i++){
      if (strstr(argv[i],".csf")){
        filename = std::string(argv[i]);
      }
      if (strcmp(argv[i],"-renderer")==0 && i+1<argc){
        tweak.renderer = atoi(argv[i+1]);
        i++;
      }
      if (strcmp(argv[i],"-strategy")==0 && i+1<argc){
        tweak.strategy = (Strategy)atoi(argv[i+1]);
        i++;
      }
      if (strcmp(argv[i],"-shademode")==0 && i+1<argc){
        tweak.shade = (ShadeType)atoi(argv[i+1]);
        i++;
      }
      if (strcmp(argv[i],"-xplode")==0){
        tweak.animateActive = true;
      }
      if (strcmp(argv[i],"-zoom")==0 && i+1<argc){
        tweak.zoom = atoi(argv[i+1]);
        i++;
      }
      if (strcmp(argv[i],"-noui")==0){
        tweak.noUI = true;
      }
      if (strcmp(argv[i],"-msaa")==0 && i+1<argc){
        tweak.msaa = atoi(argv[i+1]);
        i++;
      }
    }

    if (filename.empty())
    {
      fprintf(stderr,"no .csf file specified\n");
      exit(EXIT_FAILURE);
    }
  }

}

using namespace csfviewer;

int sample_main(int argc, const char** argv)
{
  Sample sample;
  sample.parse(argc,argv);

  return sample.run(
    PROJECT_NAME,
    argc, argv,
    SAMPLE_SIZE_WIDTH, SAMPLE_SIZE_HEIGHT,
    SAMPLE_MAJOR_VERSION, SAMPLE_MINOR_VERSION);
}

void sample_print(int level, const char * fmt)
{

}


