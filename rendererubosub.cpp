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

#include <assert.h>
#include <algorithm>
#include "renderer.hpp"
#include <main.h>

#include <nv_math/nv_math_glsltypes.h>

using namespace nv_math;
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
        return !!GLEW_NV_vertex_buffer_unified_memory;
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

  public:
    void init(const CadScene* __restrict scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager);


    bool                        m_vbum;
    GLuint                      m_streamMatrix;
    GLuint                      m_streamMaterial;

    RendererUboSub()
      : m_vbum(false)
    {

    }

  private:

    void RenderCache( const CadScene::DrawRangeCache& cache, const CadScene* __restrict scene, bool solid, int& lastMaterial, int&lastMatrix ) 
    {
      int begin = 0;
      for (size_t s = 0; s < cache.state.size(); s++)
      {
        const CadScene::DrawStateInfo &state = cache.state[s];

        if (state.matrixIndex != lastMatrix){
          glNamedBufferSubDataEXT(m_streamMatrix, 0, sizeof(CadScene::MatrixNode), &scene->m_matrices[state.matrixIndex]);
          lastMatrix = state.matrixIndex;
        }
        if (state.materialIndex != lastMaterial){
          glNamedBufferSubDataEXT(m_streamMaterial, 0, sizeof(CadScene::Material), &scene->m_materials[state.materialIndex]);
          lastMaterial = state.materialIndex;
        }
        glMultiDrawElements(solid ? GL_TRIANGLES : GL_LINES,&cache.counts[begin],GL_UNSIGNED_INT,(const GLvoid**) &cache.offsets[begin], cache.stateCount[s]);
        begin += cache.stateCount[s];
      }
    }

    void RenderJoin(const CadScene::Object& obj, const CadScene::Geometry& geo, const CadScene* __restrict scene, bool solid, int &lastMaterial, int&lastMatrix)
    {
      CadScene::DrawRange range;

      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active) continue;

        if (part.materialIndex != lastMaterial || part.matrixIndex != lastMatrix){
          // evict
          if (range.count){
            glDrawElements(solid ? GL_TRIANGLES : GL_LINES, range.count, GL_UNSIGNED_INT, (const GLvoid*)range.offset);
          }

          range = CadScene::DrawRange();

          if (part.matrixIndex != lastMatrix){
            glNamedBufferSubDataEXT(m_streamMatrix, 0, sizeof(CadScene::MatrixNode), &scene->m_matrices[part.matrixIndex]);
          }
          if (part.materialIndex != lastMaterial) {
            glNamedBufferSubDataEXT(m_streamMaterial, 0, sizeof(CadScene::Material), &scene->m_materials[part.materialIndex]);
          }

          lastMaterial = part.materialIndex;
          lastMatrix   = part.matrixIndex;
        }

        if (!range.count){
          range.offset = solid ? mesh.indexSolid.offset : mesh.indexWire.offset;
        }

        range.count += solid ? mesh.indexSolid.count : mesh.indexWire.count;
      }

      // evict
      glDrawElements(solid ? GL_TRIANGLES : GL_LINES, range.count, GL_UNSIGNED_INT, (const GLvoid*)range.offset);
    }

    void RenderIndividual (const CadScene::Object& obj, const CadScene::Geometry& geo, const CadScene* __restrict scene, bool solid, int &lastMaterial, int&lastMatrix)
    {
      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active) continue;

        if (part.matrixIndex != lastMatrix){
          glNamedBufferSubDataEXT(m_streamMatrix, 0, sizeof(CadScene::MatrixNode), &scene->m_matrices[part.matrixIndex]);
          lastMatrix = part.matrixIndex;
        }

        if (part.materialIndex != lastMaterial){
          glNamedBufferSubDataEXT(m_streamMaterial, 0, sizeof(CadScene::Material), &scene->m_materials[part.materialIndex]);
          lastMaterial = part.materialIndex;
        }

        if (solid){
          glDrawElements(GL_TRIANGLES, mesh.indexSolid.count, GL_UNSIGNED_INT, (const GLvoid*)mesh.indexSolid.offset);
        }
        else{
          glDrawElements(GL_LINES, mesh.indexWire.count, GL_UNSIGNED_INT, (const GLvoid*)mesh.indexWire.offset);
        }

      }
    }

  };

  static RendererUboSub::Type s_ubosub;
  static RendererUboSub::TypeVbum s_ubosub_vbum;

  void RendererUboSub::init(const CadScene* __restrict scene, const Resources& resources)
  {
    m_scene = scene;
    glGenBuffers(1,&m_streamMatrix);
    glGenBuffers(1,&m_streamMaterial);
    glNamedBufferDataEXT( m_streamMatrix, sizeof(CadScene::MatrixNode), NULL, GL_STREAM_DRAW);
    glNamedBufferDataEXT( m_streamMaterial, sizeof(CadScene::Material), NULL, GL_STREAM_DRAW);
  }

  void RendererUboSub::deinit()
  {
    glDeleteBuffers(1,&m_streamMatrix);
    glDeleteBuffers(1,&m_streamMaterial);
  }

  void RendererUboSub::draw(ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager)
  {
    const CadScene* __restrict scene = m_scene;

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

    int lastMatrix = -1;
    int lastMaterial = -1;
    int lastGeometry = -1;
    size_t numObjects = scene->m_objects.size();
    for (size_t i = 0; i < numObjects; i++){
      const CadScene::Object& obj = scene->m_objects[i];
      const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

      if (obj.geometryIndex != lastGeometry){

        if (vbum){
          glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0, geo.vboADDR, geo.numVertices * sizeof(CadScene::Vertex));
          glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0, geo.iboADDR, (geo.numIndexSolid+geo.numIndexWire) * sizeof(GLuint));
        }
        else{
          glBindVertexBuffer(0, geo.vboGL, 0, sizeof(CadScene::Vertex));
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.iboGL);
        }

        lastGeometry = obj.geometryIndex;
      }

      if (m_strategy == STRATEGY_GROUPS){
        if (shadetype == SHADE_SOLID || shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          RenderCache(obj.cacheSolid, scene, true, lastMaterial, lastMatrix);
        }

        if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          SetWireMode(GL_TRUE);
          RenderCache(obj.cacheWire, scene, false, lastMaterial, lastMatrix);

          SetWireMode(GL_FALSE);
        }
      }
      else if (m_strategy == STRATEGY_JOIN) {
        if (shadetype == SHADE_SOLID || shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          RenderJoin(obj, geo, scene, true, lastMaterial, lastMatrix);
        }
        if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          SetWireMode(GL_TRUE);
          RenderJoin(obj, geo, scene, false, lastMaterial, lastMatrix);

          SetWireMode(GL_FALSE);
        }
      }
      else if (m_strategy == STRATEGY_INDIVIDUAL){
        if (shadetype == SHADE_SOLID || shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          RenderIndividual(obj, geo, scene, true, lastMaterial, lastMatrix);
        }
        if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
          SetWireMode(GL_TRUE);
          RenderIndividual(obj, geo, scene, false, lastMaterial, lastMatrix);

          SetWireMode(GL_FALSE);
        }
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
