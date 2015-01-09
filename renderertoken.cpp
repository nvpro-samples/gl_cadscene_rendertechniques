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


#include "tokenbase.hpp"

#include <nv_math/nv_math_glsltypes.h>

using namespace nv_math;
#include "common.h"

namespace csfviewer
{

  //////////////////////////////////////////////////////////////////////////


  class RendererToken: public Renderer, public TokenRendererBase {
  public:
    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenbuffer";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        return renderer;
      }
      unsigned int priority() const 
      {
        return 9;
      }
    };
    class TypeList : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenlist";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_uselist = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 8;
      }
    };
    class TypeEmu : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "tokenbuffer_emulated";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_emulate = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 9;
      }
    };


  public:
    void init(const CadScene* __restrict scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager);

    void FillCache( std::string& tokenStream, const CadScene::Object& obj, const CadScene::Geometry& geo, int& lastMaterial, int&lastMatrix, const CadScene* __restrict scene, bool solid ) 
    {
      int begin = 0;
      const CadScene::DrawRangeCache &cache = solid ? obj.cacheSolid : obj.cacheWire;

      for (size_t s = 0; s < cache.state.size(); s++)
      {
        const CadScene::DrawStateInfo &state = cache.state[s];
        if (state.matrixIndex != lastMatrix){
          NVTokenUbo ubo;
          ubo.setBinding(UBO_MATRIX, NVTOKEN_STAGE_VERTEX);
          ubo.setBuffer(scene->m_matricesGL, scene->m_matricesADDR, sizeof(CadScene::MatrixNode) * state.matrixIndex, sizeof(CadScene::MatrixNode) );
          nvtokenEnqueue(tokenStream, ubo);
          lastMatrix   = state.matrixIndex;
        }
        if (state.materialIndex != lastMaterial){
          NVTokenUbo ubo;
          ubo.setBinding(UBO_MATERIAL, NVTOKEN_STAGE_FRAGMENT);
          ubo.setBuffer(scene->m_materialsGL, scene->m_materialsADDR, sizeof(CadScene::Material) * state.materialIndex, sizeof(CadScene::Material) );
          nvtokenEnqueue(tokenStream, ubo);
          lastMaterial = state.materialIndex;
        }
        for (int d = 0; d < cache.stateCount[s]; d++){
          // evict
          NVTokenDrawElemsUsed drawelems;
          drawelems.setMode(solid ? GL_TRIANGLES : GL_LINES);
          drawelems.cmd.count = cache.counts[begin + d];
          drawelems.cmd.firstIndex = GLuint(cache.offsets[begin + d]/sizeof(GLuint));
          nvtokenEnqueue(tokenStream, drawelems);
        }
        begin += cache.stateCount[s];
      }
    }

    void FillJoin( std::string& tokenStream, const CadScene::Object& obj, const CadScene::Geometry& geo, int &lastMaterial, int&lastMatrix, const CadScene* __restrict scene, bool solid ) 
    {
      CadScene::DrawRange range;

      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active || !(solid ? mesh.indexSolid.count : mesh.indexWire.count)) continue;

        if (part.materialIndex != lastMaterial || part.matrixIndex != lastMatrix){

          if (range.count){
            NVTokenDrawElemsUsed drawelems;
            drawelems.setMode(solid ? GL_TRIANGLES : GL_LINES);
            drawelems.cmd.count = range.count;
            drawelems.cmd.firstIndex = GLuint(range.offset/sizeof(GLuint));
            nvtokenEnqueue(tokenStream, drawelems);
          }

          range = CadScene::DrawRange();

          if (part.matrixIndex != lastMatrix){
            NVTokenUbo ubo;
            ubo.setBinding(UBO_MATRIX, NVTOKEN_STAGE_VERTEX);
            ubo.setBuffer(scene->m_matricesGL, scene->m_matricesADDR, sizeof(CadScene::MatrixNode) * part.matrixIndex, sizeof(CadScene::MatrixNode) );
            nvtokenEnqueue(tokenStream, ubo);
            lastMatrix   = part.matrixIndex;
          }

          if (part.materialIndex != lastMaterial){
            NVTokenUbo ubo;
            ubo.setBinding(UBO_MATERIAL, NVTOKEN_STAGE_FRAGMENT);
            ubo.setBuffer(scene->m_materialsGL, scene->m_materialsADDR, sizeof(CadScene::Material) * part.materialIndex, sizeof(CadScene::Material) );
            nvtokenEnqueue(tokenStream, ubo);
            lastMaterial = part.materialIndex;
          }
        }

        if (!range.count){
          range.offset = solid ? mesh.indexSolid.offset : mesh.indexWire.offset;
        }

        range.count += solid ? mesh.indexSolid.count : mesh.indexWire.count;
      }

      // evict
      NVTokenDrawElemsUsed drawelems;
      drawelems.setMode(solid ? GL_TRIANGLES : GL_LINES);
      drawelems.cmd.count = range.count;
      drawelems.cmd.firstIndex = GLuint(range.offset/sizeof(GLuint));
      nvtokenEnqueue(tokenStream, drawelems);
    }

    void FillIndividual( std::string& tokenStream, const CadScene::Object& obj, const CadScene::Geometry& geo, int& lastMaterial, int&lastMatrix, const CadScene* __restrict scene, bool solid ) 
    {
      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active || !(solid ? mesh.indexSolid.count : mesh.indexWire.count)) continue;

        if (part.matrixIndex != lastMatrix || USE_NOFILTER){

          NVTokenUbo ubo;
          ubo.setBinding(UBO_MATRIX, NVTOKEN_STAGE_VERTEX);
          ubo.setBuffer(scene->m_matricesGL, scene->m_matricesADDR, sizeof(CadScene::MatrixNode) * part.matrixIndex, sizeof(CadScene::MatrixNode) );
          nvtokenEnqueue(tokenStream, ubo);
          lastMatrix   = part.matrixIndex;
        }

        if (part.materialIndex != lastMaterial || USE_NOFILTER){

          NVTokenUbo ubo;
          ubo.setBinding(UBO_MATERIAL, NVTOKEN_STAGE_FRAGMENT);
          ubo.setBuffer(scene->m_materialsGL, scene->m_materialsADDR, sizeof(CadScene::Material) * part.materialIndex, sizeof(CadScene::Material) );
          nvtokenEnqueue(tokenStream, ubo);
          lastMaterial = part.materialIndex;
        }

        NVTokenDrawElemsUsed drawelems;
        drawelems.setMode(solid ? GL_TRIANGLES : GL_LINES);
        drawelems.cmd.count = solid ? mesh.indexSolid.count : mesh.indexWire.count;
        drawelems.cmd.firstIndex = GLuint((solid ? mesh.indexSolid.offset : mesh.indexWire.offset )/sizeof(GLuint));
        nvtokenEnqueue(tokenStream, drawelems);
      }
    }
  };


  static RendererToken::Type s_token;
  static RendererToken::TypeList s_token_list;
  static RendererToken::TypeEmu s_token_emu;


  void RendererToken::init(const CadScene* __restrict scene, const Resources& resources)
  {
    TokenRendererBase::init(s_bindless_ubo, !!GLEW_NV_vertex_buffer_unified_memory);

    m_scene = scene;

    {
      size_t begin = 0;

      int lastMatrix = -1;
      int lastMaterial = -1;
      int lastGeometry = -1;

      std::string& tokenStream = m_tokenStreams[SHADE_SOLID];
      tokenStream.clear();

      {
        NVTokenUbo ubo;
        ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_VERTEX);
        ubo.setBuffer(resources.sceneUbo, resources.sceneAddr, 0, sizeof(SceneData) );
        nvtokenEnqueue(tokenStream, ubo);

        ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_FRAGMENT);
        nvtokenEnqueue(tokenStream, ubo);

#if USE_POLYOFFSETTOKEN
        NVTokenPolygonOffset offset;
        offset.cmd.bias = 1;
        offset.cmd.scale = 1;
        nvtokenEnqueue(tokenStream, offset);
#endif
      }

      for (size_t i = 0; i < scene->m_objects.size(); i++){
        const CadScene::Object& obj = scene->m_objects[i];
        const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

        if (USE_NOFILTER){
          lastMaterial = -1;
        }

        if (obj.geometryIndex != lastGeometry || USE_NOFILTER){

          NVTokenVbo vbo;
          vbo.cmd.index = 0;
          vbo.setBuffer(geo.vboGL, geo.vboADDR, 0);
          nvtokenEnqueue(tokenStream, vbo);
          NVTokenIbo ibo;
          ibo.setBuffer(geo.iboGL, geo.iboADDR);
          ibo.cmd.typeSizeInByte = 4;
          nvtokenEnqueue(tokenStream, ibo);
          lastGeometry = obj.geometryIndex;
        }

        if (m_strategy == STRATEGY_GROUPS){
          FillCache(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }
        else if (m_strategy == STRATEGY_JOIN) {
          FillJoin(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }
        else if (m_strategy == STRATEGY_INDIVIDUAL){
          FillIndividual(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }
      }

      m_shades[SHADE_SOLID].offsets.push_back( begin );
      m_shades[SHADE_SOLID].sizes.  push_back( GLsizei((tokenStream.size()- begin)) );
      m_shades[SHADE_SOLID].states .push_back( m_stateObjects[STATE_TRIS] );
      m_shades[SHADE_SOLID].fbos   .push_back( 0);

      TokenRendererBase::printStats(SHADE_SOLID);
    }
  
    // SHADE_SOLIDWIRE pass

    {
      size_t begin = 0;

      int lastMatrix = -1;
      int lastMaterial = -1;
      int lastGeometry = -1;

      std::string& tokenStream = m_tokenStreams[SHADE_SOLIDWIRE];
      tokenStream.clear();

      for (size_t i = 0; i < scene->m_objects.size(); i++){
        const CadScene::Object& obj = scene->m_objects[i];
        const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

        begin = tokenStream.size();

        if (USE_NOFILTER){
          lastMaterial = -1;
        }

        if (i == 0){
          NVTokenUbo ubo;
          ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_VERTEX);
          ubo.setBuffer(resources.sceneUbo, resources.sceneAddr, 0, sizeof(SceneData) );
          nvtokenEnqueue(tokenStream, ubo);

          ubo.setBinding(UBO_SCENE, NVTOKEN_STAGE_FRAGMENT);
          nvtokenEnqueue(tokenStream, ubo);

#if USE_POLYOFFSETTOKEN
          NVTokenPolygonOffset offset;
          offset.cmd.bias = 1;
          offset.cmd.scale = 1;
          nvtokenEnqueue(tokenStream, offset);
#endif
        }

        if (obj.geometryIndex != lastGeometry || USE_NOFILTER){

          NVTokenVbo vbo;
          vbo.cmd.index = 0;
          vbo.setBuffer(geo.vboGL, geo.vboADDR, 0);
          nvtokenEnqueue(tokenStream, vbo);
          NVTokenIbo ibo;
          ibo.setBuffer(geo.iboGL, geo.iboADDR);
          ibo.cmd.typeSizeInByte = 4;
          nvtokenEnqueue(tokenStream, ibo);
          lastGeometry = obj.geometryIndex;
        }

        // push triangles

        if (m_strategy == STRATEGY_GROUPS){
          FillCache(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }
        else if (m_strategy == STRATEGY_JOIN) {
          FillJoin(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }
        else if (m_strategy == STRATEGY_INDIVIDUAL){
          FillIndividual(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, true);
        }

        m_shades[SHADE_SOLIDWIRE].offsets.push_back( begin );
        m_shades[SHADE_SOLIDWIRE].sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
        m_shades[SHADE_SOLIDWIRE].states .push_back( m_stateObjects[STATE_TRISOFFSET] );
        m_shades[SHADE_SOLIDWIRE].fbos   .push_back( 0);

        // push line

        begin = tokenStream.size();

        if (m_strategy == STRATEGY_GROUPS){
          FillCache(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, false);
        }
        else if (m_strategy == STRATEGY_JOIN) {
          FillJoin(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, false);
        }
        else if (m_strategy == STRATEGY_INDIVIDUAL){
          FillIndividual(tokenStream, obj, geo, lastMaterial, lastMatrix, scene, false);
        }

        m_shades[SHADE_SOLIDWIRE].offsets.push_back( begin );
        m_shades[SHADE_SOLIDWIRE].sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
        m_shades[SHADE_SOLIDWIRE].states .push_back( m_stateObjects[STATE_LINES] );
        m_shades[SHADE_SOLIDWIRE].fbos   .push_back( 0 );
      }
    }

    TokenRendererBase::printStats(SHADE_SOLIDWIRE);

    TokenRendererBase::finalize(resources);
  }

  void RendererToken::deinit()
  {
    TokenRendererBase::deinit();
  }

  void RendererToken::draw(ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager)
  {
    const CadScene* __restrict scene = m_scene;

    // do state setup (primarily for sake of state capturing)
    scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

    if (m_bindlessVboUbo){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    }
    else{
      glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE,resources.sceneUbo);
    }

    if (USE_STATEOBJ_REBUILD){
      nv_helpers_gl::Profiler::Section section(profiler,"state");
      for (int i = 0; i < 25; i++){
        m_stateIncarnation = resources.stateIncarnation + 1;
        m_fboStateIncarnation = resources.fboTextureIncarnation + 1;
        captureState(resources);
      }
    }
    else{
      captureState(resources);
    }

    if (!USE_POLYOFFSETTOKEN && (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT)){
      glPolygonOffset(1,1);
    }

    if (m_hwsupport){
      if (m_uselist){
        glCallCommandListNV(m_commandLists[shadetype]);
      }
      else{
        ShadeCommand & shade =  m_shades[shadetype];
        glDrawCommandsStatesNV(m_tokenBuffers[shadetype], &shade.offsets[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], int(shade.states.size()) );
      }
    }
    else{
      ShadeCommand & shade =  m_shades[shadetype];
      std::string& stream  =  m_tokenStreams[shadetype];
      renderShadeCommandSW(&stream[0], stream.size(), shade);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexBuffer(0,0,0,0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0,0);

    if (m_bindlessVboUbo){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
    }

    scene->disableVertexFormat(VERTEX_POS,VERTEX_NORMAL);
  }

}

