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

#include <assert.h>
#include <algorithm>
#include "renderer.hpp"

#include "common.h"

namespace csfviewer
{
  //////////////////////////////////////////////////////////////////////////

  class RendererUboRange: public Renderer {
  public:
    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "uborange";
      }
      Renderer* create() const
      {
        RendererUboRange* renderer = new RendererUboRange();
        return renderer;
      }
      unsigned int priority() const 
      {
        return 0;
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
        return "uborange_bindless";
      }
      Renderer* create() const
      {
        RendererUboRange* renderer = new RendererUboRange();
        renderer->m_vbum = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 0;
      }
    };
    class TypeSort : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "uborange_sorted";
      }
      Renderer* create() const
      {
        RendererUboRange* renderer = new RendererUboRange();
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 1;
      }
    };
    class TypeSortEmu : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return !!has_GL_NV_vertex_buffer_unified_memory;
      }
      const char* name() const
      {
        return "uborange_sorted_bindless";
      }
      Renderer* create() const
      {
        RendererUboRange* renderer = new RendererUboRange();
        renderer->m_vbum = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 1;
      }
    };

  public:
    void init(const CadScene* NV_RESTRICT scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager);

    RendererUboRange()
      : m_vbum(false)
      , m_sort(false)
    {

    }

    bool                        m_vbum;
    bool                        m_sort;

  private:

    std::vector<DrawItem>       m_drawItems;

  };
  static RendererUboRange::Type         s_uborange;
  static RendererUboRange::TypeEmu      s_uborange_emu;

  static RendererUboRange::TypeSort     s_sortuborange;
  static RendererUboRange::TypeSortEmu  s_sortuborange_emu;

  void RendererUboRange::init(const CadScene* NV_RESTRICT scene, const Resources& resources)
  {
    m_scene = scene;

    fillDrawItems(m_drawItems,0,scene->m_objects.size(), true, true);

    if (m_sort){
      std::sort(m_drawItems.begin(),m_drawItems.end(),DrawItem_compare_groups);
    }
  }

  void RendererUboRange::deinit()
  {
    m_drawItems.clear();
  }

  void RendererUboRange::draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager)
  {
    const CadScene* NV_RESTRICT scene = m_scene;

    bool vbum = m_vbum;

    scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

    if (vbum){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      if (s_bindless_ubo){
        glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_SCENE,resources.sceneAddr,sizeof(SceneData));
      }
      else{
        glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE,resources.sceneUbo);
      }
    }
    else{
      glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE,resources.sceneUbo);
    }

    glUseProgram(resources.programUbo);

    SetWireMode(GL_FALSE);

    if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1,1);
    }

    {
      int lastMaterial = -1;
      int lastGeometry = -1;
      int lastMatrix   = -1;
      bool lastSolid   = true;

      GLenum mode = GL_TRIANGLES;

      for (int i = 0; i < m_drawItems.size(); i++){
        const DrawItem& di = m_drawItems[i];

        if (shadetype == SHADE_SOLID && !di.solid){
          if (m_sort) break;
          continue;
        }

        if (lastSolid != di.solid){
          SetWireMode( di.solid ? GL_FALSE : GL_TRUE );
          if (shadetype == SHADE_SOLIDWIRE_SPLIT){
            glBindFramebuffer(GL_FRAMEBUFFER, di.solid ? resources.fbo : resources.fbo2);
          }
        }

        if (lastGeometry != di.geometryIndex){
          const CadScene::Geometry &geo = scene->m_geometry[di.geometryIndex];

          if (vbum){
            glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0,  geo.vboADDR, geo.numVertices * sizeof(CadScene::Vertex));
            glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,         geo.iboADDR, (geo.numIndexSolid+geo.numIndexWire) * sizeof(GLuint));
          }
          else{
            glBindVertexBuffer(0, geo.vboGL, 0, sizeof(CadScene::Vertex));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.iboGL);
          }

          lastGeometry = di.geometryIndex;
        }

        if (lastMatrix != di.matrixIndex){

          if (vbum && s_bindless_ubo){
            glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_MATRIX, scene->m_matricesADDR + sizeof(CadScene::MatrixNode) * di.matrixIndex, sizeof(CadScene::MatrixNode));
          }
          else{
            glBindBufferRange(GL_UNIFORM_BUFFER,UBO_MATRIX, scene->m_matricesGL, sizeof(CadScene::MatrixNode) * di.matrixIndex, sizeof(CadScene::MatrixNode));
          }

          lastMatrix = di.matrixIndex;
        }

        if (lastMaterial != di.materialIndex){

          if (m_vbum && s_bindless_ubo){
            glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_MATERIAL, scene->m_materialsADDR +sizeof(CadScene::Material) * di.materialIndex, sizeof(CadScene::Material));
          }
          else{
            glBindBufferRange(GL_UNIFORM_BUFFER,UBO_MATERIAL, scene->m_materialsGL, sizeof(CadScene::Material) * di.materialIndex, sizeof(CadScene::Material));
          }

          lastMaterial = di.materialIndex;
        }

        glDrawElements( di.solid ? GL_TRIANGLES : GL_LINES, di.range.count, GL_UNSIGNED_INT, (void*) di.range.offset);

        lastSolid = di.solid;
      }
    }

    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexBuffer(0,0,0,0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(0,0);

    if (vbum){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      if (s_bindless_ubo){
        glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
      }
    }

    scene->disableVertexFormat(VERTEX_POS,VERTEX_NORMAL);
  }

}
