/* Copyright (c) 2014-2018, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "tokenbase.hpp"

#include <nvmath/nvmath_glsltypes.h>

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
    class TypeAddr : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenbuffer_address";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_useaddress = true;
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

    class TypeSort : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenbuffer_sorted";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 9;
      }
    };
    class TypeSortAddr : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenbuffer_sorted_address";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_useaddress = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 9;
      }
    };
    class TypeSortList : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenlist_sorted";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_uselist = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 8;
      }
    };
    class TypeSortEmu : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "tokenbuffer_sorted_emulated";
      }
      Renderer* create() const
      {
        RendererToken* renderer = new RendererToken();
        renderer->m_emulate = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 9;
      }
    };

  public:
    void init(const CadScene* NV_RESTRICT scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager);

  private:

    std::vector<DrawItem>       m_drawItems;

    void GenerateTokens(std::vector<DrawItem>& drawItems, ShadeType shade, const CadScene* NV_RESTRICT scene, const Resources& resources )
    {
      int lastMaterial = -1;
      int lastGeometry = -1;
      int lastMatrix   = -1;
      bool lastSolid   = true;

      ShadeCommand& sc = m_shades[shade];
      sc.fbos.clear();
      sc.offsets.clear();
      sc.sizes.clear();
      sc.states.clear();
      
      std::string& tokenStream = m_tokenStreams[shade];
      tokenStream.clear();

      size_t begin = 0;

      {
        NVTokenUbo ubo;
        ubo.cmd.index   = UBO_SCENE;
        ubo.cmd.stage   = UBOSTAGE_VERTEX;
        ubo.setBuffer(resources.sceneUbo, resources.sceneAddr, 0, sizeof(SceneData));
        nvtokenEnqueue(tokenStream, ubo);

        ubo.cmd.stage   = UBOSTAGE_FRAGMENT;
        nvtokenEnqueue(tokenStream, ubo);

#if USE_POLYOFFSETTOKEN
        NVTokenPolygonOffset offset;
        offset.cmd.bias = 1;
        offset.cmd.scale = 1;
        nvtokenEnqueue(tokenStream, offset);
#endif
      }

      for (int i = 0; i < drawItems.size(); i++){
        const DrawItem& di = drawItems[i];

        if (shade == SHADE_SOLID && !di.solid){
          continue;
        }

        if (shade == SHADE_SOLIDWIRE && di.solid != lastSolid){
          sc.offsets.push_back( begin );
          sc.sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
          sc.states. push_back( m_stateObjects[ lastSolid ? STATE_TRISOFFSET : STATE_LINES ] );
          sc.fbos.   push_back( 0 );

          begin = tokenStream.size();
        }

        if (lastGeometry != di.geometryIndex){
          const CadScene::Geometry &geo = scene->m_geometry[di.geometryIndex];
          NVTokenVbo vbo;
          vbo.cmd.index = 0;
          vbo.setBuffer(geo.vboGL, geo.vboADDR, 0);
          nvtokenEnqueue(tokenStream, vbo);

          NVTokenIbo ibo;
          ibo.setBuffer(geo.iboGL, geo.iboADDR);
          ibo.cmd.typeSizeInByte = 4;
          nvtokenEnqueue(tokenStream, ibo);

          lastGeometry = di.geometryIndex;
        }

        if (lastMatrix != di.matrixIndex){

          NVTokenUbo ubo;
          ubo.cmd.index   = UBO_MATRIX;
          ubo.cmd.stage   = UBOSTAGE_VERTEX;
          ubo.setBuffer(scene->m_matricesGL, scene->m_matricesADDR, sizeof(CadScene::MatrixNode) * di.matrixIndex, sizeof(CadScene::MatrixNode));
          nvtokenEnqueue(tokenStream, ubo);

          lastMatrix = di.matrixIndex;
        }

        if (lastMaterial != di.materialIndex){

          NVTokenUbo ubo;
          ubo.cmd.index   = UBO_MATERIAL;
          ubo.cmd.stage   = UBOSTAGE_FRAGMENT;
          ubo.setBuffer(scene->m_materialsGL, scene->m_materialsADDR, sizeof(CadScene::Material) * di.materialIndex, sizeof(CadScene::Material));
          nvtokenEnqueue(tokenStream, ubo);

          lastMaterial = di.materialIndex;
        }


        NVTokenDrawElemsUsed drawelems;
        drawelems.setMode(di.solid ? GL_TRIANGLES : GL_LINES);
        drawelems.cmd.count = di.range.count;
        drawelems.cmd.firstIndex = GLuint((di.range.offset )/sizeof(GLuint));
        nvtokenEnqueue(tokenStream, drawelems);

        lastSolid = di.solid;
      }

      sc.offsets.push_back( begin );
      sc.sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
      if (shade == SHADE_SOLID){
        sc.states. push_back( m_stateObjects[ STATE_TRIS ] );
      }
      else{
        sc.states. push_back( m_stateObjects[ lastSolid ? STATE_TRISOFFSET : STATE_LINES ] );
      }
      sc.fbos. push_back( 0 );

    }

  };
  static RendererToken::Type      s_token;
  static RendererToken::TypeAddr  s_token_addr;
  static RendererToken::TypeList  s_token_list;
  static RendererToken::TypeEmu   s_token_emu;

  static RendererToken::TypeSort      s_sorttoken;
  static RendererToken::TypeSortAddr  s_sorttoken_addr;
  static RendererToken::TypeSortList  s_sorttoken_list;
  static RendererToken::TypeSortEmu   s_sorttoken_emu;

  void RendererToken::init(const CadScene* NV_RESTRICT scene, const Resources& resources)
  {
    TokenRendererBase::init(s_bindless_ubo, !!has_GL_NV_vertex_buffer_unified_memory);
    resources.usingUboProgram(true);

    m_scene = scene;

    std::vector<DrawItem> drawItems;

    fillDrawItems(drawItems,0,scene->m_objects.size(), true, true);

    if (USE_PERFRAMEBUILD){
      m_drawItems = drawItems;
    }

    if (m_sort){
      std::sort(drawItems.begin(),drawItems.end(),DrawItem_compare_groups);
    }

    GenerateTokens(drawItems, SHADE_SOLID, scene, resources);

    TokenRendererBase::printStats(SHADE_SOLID);

    GenerateTokens(drawItems, SHADE_SOLIDWIRE, scene, resources);

    TokenRendererBase::printStats(SHADE_SOLIDWIRE);

    TokenRendererBase::finalize(resources);
  }

  void RendererToken::deinit()
  {
    TokenRendererBase::deinit();
    m_drawItems.clear();
  }

  void RendererToken::draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager)
  {
    const CadScene* NV_RESTRICT scene = m_scene;

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

    if (USE_PERFRAMEBUILD){

#if 0
      std::vector<DrawItem> drawItems;
      {
        nvh::Profiler::Section _tempTimer(profiler ,"Copy");
        drawItems = m_drawItems;
      }
#else
      std::vector<DrawItem>& drawItems = m_drawItems;
#endif
      {
        nvh::Profiler::Section _tempTimer(profiler ,"Sort");
        std::sort(drawItems.begin(),drawItems.end(),DrawItem_compare_groups);
      }

      {
        nvh::Profiler::Section _tempTimer(profiler ,"Token");
        GenerateTokens(drawItems, shadetype, scene, resources);
      }

      if (!m_emulate && !m_uselist){
        nvh::Profiler::Section _tempTimer(profiler ,"Build");
        ShadeCommand & shade =  m_shades[shadetype];
        glInvalidateBufferData(m_tokenBuffers[shadetype]);
        glNamedBufferSubData(m_tokenBuffers[shadetype],shade.offsets[0], m_tokenStreams[shadetype].size(), &m_tokenStreams[shadetype][0]);
      }
    }

    if (USE_STATEOBJ_REBUILD){
      nvh::Profiler::Section section(profiler,"state");
      for (int i = 0; i < 25; i++){
        m_stateChangeID = resources.stateChangeID + 1;
        m_fboStateChangeID = resources.fboTextureChangeID + 1;
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
        if (m_useaddress){
          glDrawCommandsStatesAddressNV(&shade.addresses[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], int(shade.states.size()) );
        }
        else{
          glDrawCommandsStatesNV(m_tokenBuffers[shadetype], &shade.offsets[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], int(shade.states.size()) );
        }
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
