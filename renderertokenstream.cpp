/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */


/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include "tokenbase.hpp"

#include "common.h"

namespace csfviewer
{
  //////////////////////////////////////////////////////////////////////////

  class RendererTokenStream: public Renderer, public TokenRendererBase {
  public:
    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenstream";
      }
      Renderer* create() const
      {
        RendererTokenStream* renderer = new RendererTokenStream();
        return renderer;
      }
      unsigned int priority() const 
      {
        return 10;
      }
    };
    class TypeEmu : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return !!has_GL_NV_vertex_buffer_unified_memory;
      }
      const char* name() const
      {
        return "tokenstream_emulated";
      }
      Renderer* create() const
      {
        RendererTokenStream* renderer = new RendererTokenStream();
        renderer->m_emulate = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 10;
      }
    };

  public:
    void init(const CadScene* NV_RESTRICT scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager);

  private:

    static const size_t bufferSize = 1024*16;

    std::vector<DrawItem>       m_drawItems;

    size_t GenerateTokens(NVPointerStream& tokenStream, std::vector<DrawItem>& drawItems, size_t from, ShadeType shade, const CadScene* NV_RESTRICT scene, const Resources& resources )
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

      size_t i = from;
      for (; i < drawItems.size(); i++){
        const DrawItem& di = drawItems[i];

        if (tokenStream.size() + sizeof(NVTokenIbo) + sizeof(NVTokenVbo) + sizeof(NVTokenUbo)*2 + sizeof(NVTokenDrawElemsUsed) > tokenStream.capacity()){
          break;
        }

        if (shade == SHADE_SOLID && !di.solid){
          continue;
        }

        if ((shade == SHADE_SOLIDWIRE || shade == SHADE_SOLIDWIRE_SPLIT) && di.solid != lastSolid){
          sc.offsets.push_back( begin );
          sc.sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
          sc.states. push_back( m_stateObjects[ lastSolid ? STATE_TRISOFFSET : STATE_LINES ] );
          if ( shade == SHADE_SOLIDWIRE_SPLIT ){
            sc.fbos.   push_back( USE_STATEFBO_SPLIT ? 0 : ( di.solid ? resources.fbo : resources.fbo2  ) );
          }
          else{
            sc.fbos.push_back(0);
          }


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
      if ( shade == SHADE_SOLIDWIRE_SPLIT ){
        sc.fbos.   push_back( USE_STATEFBO_SPLIT ? 0 : ( lastSolid ? resources.fbo : resources.fbo2  ) );
      }
      else{
        sc.fbos.push_back(0);
      }

      return i;
    }

  };

  static RendererTokenStream::Type s_sorttoken;
  static RendererTokenStream::TypeEmu s_sorttoken_emu;

  void RendererTokenStream::init(const CadScene* NV_RESTRICT scene, const Resources& resources)
  {
    TokenRendererBase::init(s_bindless_ubo, !!has_GL_NV_vertex_buffer_unified_memory);
    resources.usingUboProgram(true);

    m_scene = scene;

    fillDrawItems(m_drawItems,0,scene->m_objects.size(), true, true);

    TokenRendererBase::finalize(resources,false);

    for (int i = 0; i < NUM_SHADES; i++){
      m_tokenStreams[i].resize(bufferSize);
      glNamedBufferData(m_tokenBuffers[i], bufferSize, 0, GL_DYNAMIC_DRAW);
    }
  }

  void RendererTokenStream::deinit()
  {
    TokenRendererBase::deinit();
    m_drawItems.clear();
  }

  void RendererTokenStream::draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager)
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

    captureState(resources);

    if (!USE_POLYOFFSETTOKEN && (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT)){
      glPolygonOffset(1,1);
    }

    bool useSub = true;
    bool usePersistent = false;

    size_t begin = 0;
    while (begin < m_drawItems.size())
    {
      NVPointerStream stream;
      GLuint buffer;

      void* bufferPtr = NULL;
      if (m_hwsupport && !useSub){
        if (usePersistent){
          // not ideal, best would be finding max frame usage and then keep * 4 the size to account for driver/gpu
          // race
          glCreateBuffers(1,&buffer);
          glNamedBufferStorage(buffer, bufferSize, NULL, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_CLIENT_STORAGE_BIT);
          bufferPtr = glMapNamedBufferRange(buffer, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT);
        }
        else{
          buffer = m_tokenBuffers[shadetype];
          bufferPtr = glMapNamedBufferRange(buffer, 0, bufferSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
        }
      }
      else{
        bufferPtr = &m_tokenStreams[shadetype][0];
      }

      stream.init(bufferPtr,bufferSize);

      {
        nvh::Profiler::Section _tempTimer(profiler ,"Token");
        begin = GenerateTokens(stream, m_drawItems, begin, shadetype, scene, resources);
      }

      if (useSub){
        buffer = m_tokenBuffers[shadetype];

        nvh::Profiler::Section _tempTimer(profiler ,"Send");
        glInvalidateBufferData(buffer);
        glNamedBufferSubData(buffer,0,stream.size(), stream.m_begin);
      }

      {
        nvh::Profiler::Section _tempTimer(profiler ,"Draw");
        if (m_hwsupport){
          ShadeCommand & shade =  m_shades[shadetype];
          glDrawCommandsStatesNV(buffer, &shade.offsets[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], int(shade.states.size()) );
        }
        else{
          ShadeCommand & shade =  m_shades[shadetype];
          renderShadeCommandSW(stream.m_begin, stream.size(), shade);
        }
      }
      
      if (m_hwsupport && !useSub){
        if (usePersistent){
          glDeleteBuffers(1,&buffer);
        }
        else{
          glUnmapNamedBuffer(buffer);
        }
      }
    }

    profiler.accumulationSplit();

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
