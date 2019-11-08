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

#include <assert.h>
#include <algorithm>
#include "renderer.hpp"

#include <nvmath/nvmath_glsltypes.h>

#include "common.h"

namespace csfviewer
{
  //////////////////////////////////////////////////////////////////////////

  class RendererUboSub: public Renderer {
  public:
    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "ubosub";
      }
      Renderer* create() const
      {
        RendererUboSub* renderer = new RendererUboSub();
        return renderer;
      }
      unsigned int priority() const 
      {
        return 2;
      }
    };
    class TypeVbum : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return !!has_GL_NV_vertex_buffer_unified_memory;
      }
      const char* name() const
      {
        return "ubosub_bindless";
      }
      Renderer* create() const
      {
        RendererUboSub* renderer = new RendererUboSub();
        renderer->m_vbum = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 2;
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
        return "ubosub_sorted";
      }
      Renderer* create() const
      {
        RendererUboSub* renderer = new RendererUboSub();
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 2;
      }
    };
    class TypeSortVbum : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return !!has_GL_NV_vertex_buffer_unified_memory;
      }
      const char* name() const
      {
        return "ubosub_sorted_bindless";
      }
      Renderer* create() const
      {
        RendererUboSub* renderer = new RendererUboSub();
        renderer->m_vbum = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 2;
      }
    };

  public:
    void init(const CadScene* NV_RESTRICT scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager);

    bool                        m_sort;
    bool                        m_vbum;

  private:

    std::vector<DrawItem>       m_drawItems;

    GLuint                      m_streamMatrix;
    GLuint                      m_streamMaterial;

    RendererUboSub()
      : m_vbum(false)
      , m_sort(false)
    {

    }

  };

  static RendererUboSub::Type s_ubosub;
  static RendererUboSub::TypeVbum s_ubosub_vbum;
  static RendererUboSub::TypeSort s_ubosub_sort;
  static RendererUboSub::TypeSortVbum s_ubosub_vbum_sort;

  void RendererUboSub::init(const CadScene* NV_RESTRICT scene, const Resources& resources)
  {
    resources.usingUboProgram(true);
    m_scene = scene;

    fillDrawItems(m_drawItems,0,scene->m_objects.size(), true, true);

    if (m_sort){
      std::sort(m_drawItems.begin(),m_drawItems.end(),DrawItem_compare_groups);
    }

    m_scene = scene;
    glCreateBuffers(1,&m_streamMatrix);
    glCreateBuffers(1,&m_streamMaterial);
    glNamedBufferData( m_streamMatrix, sizeof(CadScene::MatrixNode), NULL, GL_STREAM_DRAW);
    glNamedBufferData( m_streamMaterial, sizeof(CadScene::Material), NULL, GL_STREAM_DRAW);
  }

  void RendererUboSub::deinit()
  {
    glDeleteBuffers(1,&m_streamMatrix);
    glDeleteBuffers(1,&m_streamMaterial);
  }

  void RendererUboSub::draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager)
  {
    const CadScene* NV_RESTRICT scene = m_scene;

    bool vbum = m_vbum;

    scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

    glUseProgram(resources.programUbo);

    SetWireMode(GL_FALSE);

    if (shadetype == SHADE_SOLIDWIRE){
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1,1);
    }

    if (vbum){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    }

    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE,     resources.sceneUbo);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATRIX,    m_streamMatrix);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL,  m_streamMaterial);

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
          glNamedBufferSubData(m_streamMatrix, 0, sizeof(CadScene::MatrixNode), &scene->m_matrices[di.matrixIndex]);
          lastMatrix = di.matrixIndex;
        }

        if (lastMaterial != di.materialIndex){
          glNamedBufferSubData(m_streamMaterial, 0, sizeof(CadScene::Material), &scene->m_materials[di.materialIndex]);
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

    if (m_vbum){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
    }

    if (shadetype == SHADE_SOLIDWIRE){
      glDisable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(0,0);
    }

    scene->disableVertexFormat(VERTEX_POS,VERTEX_NORMAL);
  }

}
