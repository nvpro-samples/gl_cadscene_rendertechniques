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

#define USE_VERTEX_ASSIGNS  (!USE_BASEINSTANCE)
#define USE_GPU_INDIRECT    1
#define USE_CPU_INDIRECT    (!USE_GPU_INDIRECT)

namespace csfviewer
{
  //////////////////////////////////////////////////////////////////////////

  class RendererIndexedMDI: public Renderer {
  public:
    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return true;
      }
      const char* name() const
      {
        return "indexedmdi";
      }
      Renderer* create() const
      {
        RendererIndexedMDI* renderer = new RendererIndexedMDI();
        return renderer;
      }
      unsigned int priority() const 
      {
        return 3;
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
        return "indexedmdi_bindless";
      }
      Renderer* create() const
      {
        RendererIndexedMDI* renderer = new RendererIndexedMDI();
        renderer->m_vbum = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 3;
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
        return "indexedmdi_sorted";
      }
      Renderer* create() const
      {
        RendererIndexedMDI* renderer = new RendererIndexedMDI();
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 3;
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
        return "indexedmdi_sorted_bindless";
      }
      Renderer* create() const
      {
        RendererIndexedMDI* renderer = new RendererIndexedMDI();
        renderer->m_vbum = true;
        renderer->m_sort = true;
        return renderer;
      }
      unsigned int priority() const 
      {
        return 3;
      }
    };

  private:
    struct DrawIndirectGL {
      GLuint count;
      GLuint instanceCount;
      GLuint firstIndex;
      GLint  baseVertex;
      GLuint baseInstance;

      DrawIndirectGL ()
        : count(0)
        , instanceCount(1)
        , firstIndex(0)
        , baseVertex(0)
        , baseInstance(0) {}
    };

    struct IndexedCommand {
      DrawIndirectGL  cmd;
    };

    struct ShadeCommand {
      std::vector<IndexedCommand> indirects;
      std::vector<int>      assigns;

      std::vector<size_t>   sizes;
      std::vector<size_t>   offsets;
      std::vector<int>      geometries;
      std::vector<bool>     solids;

#if USE_GPU_INDIRECT
      GLuint    indirectGL;
      GLuint64  indirectADDR;
#endif

#if USE_VERTEX_ASSIGNS
      GLuint    assignGL;
      GLuint64  assignADDR;
#endif

      ShadeCommand() {
#if USE_GPU_INDIRECT
        indirectGL = 0;
#endif
#if USE_VERTEX_ASSIGNS
        assignGL = 0;
#endif
      }
    };

  public:
    void init(const CadScene* NV_RESTRICT scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager);

    bool                        m_vbum;
    bool                        m_sort;


    RendererIndexedMDI()
      : m_vbum(false) 
      , m_sort(false)
    {

    }

  private:

    ShadeCommand    m_shades[NUM_SHADES];
    
    GLuint packBaseInstance( int matrixIndex, int materialIndex )
    {
      assert( materialIndex <= 0xFFF );
      assert( matrixIndex   <= 0xFFFFF );
      return (GLuint(matrixIndex) | (GLuint(materialIndex) << 20));
    }

    void GenerateIndirects(std::vector<DrawItem>& drawItems, ShadeType shade, const CadScene* NV_RESTRICT scene, const Resources& resources )
    {
      int lastMaterial = -1;
      int lastGeometry = -1;
      int lastMatrix   = -1;
      bool lastSolid   = true;

      ShadeCommand& sc = m_shades[shade];
      sc.assigns.clear();
      sc.indirects.clear();

      sc.sizes.clear();
      sc.offsets.clear();
      sc.solids.clear();
      sc.geometries.clear();

      std::vector<int>& assigns = sc.assigns;
      std::vector<IndexedCommand>& indirectStream = sc.indirects;

      size_t begin = 0;

      int numAssigns = 0;

      for (int i = 0; i < drawItems.size(); i++){
        const DrawItem& di = drawItems[i];

        if (shade == SHADE_SOLID && !di.solid){
          if (m_sort) break;
          continue;
        }

        if (lastGeometry != di.geometryIndex || (shade == SHADE_SOLIDWIRE && di.solid != lastSolid)){
          sc.offsets.push_back( begin );
          sc.sizes.  push_back( GLsizei((indirectStream.size()-begin)) );
          sc.solids. push_back( lastSolid );
          sc.geometries.push_back( lastGeometry );

          begin = indirectStream.size();
        }

#if USE_VERTEX_ASSIGNS
        if (lastMatrix != di.matrixIndex || lastMaterial != di.materialIndex)
        {
          // push indices
          assigns.push_back(di.matrixIndex);
          assigns.push_back(di.materialIndex);
          numAssigns++;

          lastMatrix    = di.matrixIndex;
          lastMaterial  = di.materialIndex;
        }
#endif

        IndexedCommand drawelems;
        drawelems.cmd.count = di.range.count;
        drawelems.cmd.firstIndex = GLuint((di.range.offset )/sizeof(GLuint));
#if USE_VERTEX_ASSIGNS
        drawelems.cmd.baseInstance = numAssigns - 1;
#else
        drawelems.cmd.baseInstance = packBaseInstance(di.matrixIndex, di.materialIndex);
#endif
        indirectStream.push_back(drawelems);

        lastGeometry = di.geometryIndex;
        lastSolid = di.solid;
      }

      sc.offsets.push_back( begin );
      sc.sizes.  push_back( GLsizei((indirectStream.size()-begin)) );
      sc.solids. push_back( lastSolid );
      sc.geometries.push_back( lastGeometry );
    }

  };

  static RendererIndexedMDI::Type s_indexed;
  static RendererIndexedMDI::TypeVbum s_indexed_vbum;
  static RendererIndexedMDI::TypeSort s_indexedsort;
  static RendererIndexedMDI::TypeSortVbum s_indexedsort_vbum;

  void RendererIndexedMDI::init( const CadScene* NV_RESTRICT scene, const Resources& resources )
  {
    m_scene = scene;
    resources.usingUboProgram(false);

    std::vector<DrawItem> drawItems;

    fillDrawItems(drawItems,0,scene->m_objects.size(), true, true);

    if (m_sort){
      std::sort(drawItems.begin(),drawItems.end(),DrawItem_compare_groups);
    }

    // build SC

    GenerateIndirects(drawItems, SHADE_SOLID, scene, resources);
    GenerateIndirects(drawItems, SHADE_SOLIDWIRE, scene, resources);

    for (size_t i = 0; i <= SHADE_SOLIDWIRE; i++){
      ShadeCommand& sc = m_shades[i];
#if USE_GPU_INDIRECT
      glCreateBuffers(1,&sc.indirectGL);
      glNamedBufferStorage( sc.indirectGL, sizeof(IndexedCommand) * sc.indirects.size(), &sc.indirects[0], 0 );
      if (m_vbum){
        glGetNamedBufferParameterui64vNV(sc.indirectGL, GL_BUFFER_GPU_ADDRESS_NV, &sc.indirectADDR);
        glMakeNamedBufferResidentNV(sc.indirectGL, GL_READ_ONLY);
      }
#endif
#if USE_VERTEX_ASSIGNS
      glCreateBuffers(1,&sc.assignGL);
      glNamedBufferStorage( sc.assignGL, sizeof(int) * sc.assigns.size(), &sc.assigns[0], 0 );
      if (m_vbum){
        glGetNamedBufferParameterui64vNV(sc.assignGL, GL_BUFFER_GPU_ADDRESS_NV, &sc.assignADDR);
        glMakeNamedBufferResidentNV(sc.assignGL, GL_READ_ONLY);
      }
#endif
    }

    m_shades[SHADE_SOLIDWIRE_SPLIT] = m_shades[SHADE_SOLIDWIRE];

  }

  void RendererIndexedMDI::deinit()
  {
    for (size_t i = 0; i < SHADE_SOLIDWIRE; i++){
      ShadeCommand& sc = m_shades[i];
      if (m_vbum){
#if USE_GPU_INDIRECT
        glMakeNamedBufferNonResidentNV(sc.indirectGL);
#endif
#if USE_VERTEX_ASSIGNS
        glMakeNamedBufferNonResidentNV(sc.assignGL);
#endif
      }
#if USE_GPU_INDIRECT
      glDeleteBuffers(1,&sc.indirectGL);
#endif
#if USE_VERTEX_ASSIGNS
      glDeleteBuffers(1,&sc.assignGL);
#endif
    }
  }

  void RendererIndexedMDI::draw( ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager )
  {
    const CadScene* NV_RESTRICT scene = m_scene;
    bool vbum = m_vbum;

    scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

    glUseProgram(resources.programIdx);

    if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1,1);
    }

    SetWireMode(GL_FALSE);

#if USE_VERTEX_ASSIGNS
    glVertexAttribIFormat(VERTEX_ASSIGNS,2,GL_INT,0);
    glVertexAttribBinding(VERTEX_ASSIGNS,1);
    glEnableVertexAttribArray(VERTEX_ASSIGNS);
    glBindVertexBuffer(1,0,0,sizeof(GLint)*2);
    glVertexBindingDivisor(1,1);
#endif
    if (vbum){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
#if USE_GPU_INDIRECT
      glEnableClientState(GL_DRAW_INDIRECT_UNIFIED_NV);
#endif
    }
    if (vbum && s_bindless_ubo){
      glEnableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
      glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_MATERIAL, scene->m_materialsADDR, sizeof(CadScene::Material) * scene->m_materials.size() );
      glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV, UBO_SCENE,resources.sceneAddr,sizeof(SceneData));
    }
    else{
      glBindBufferBase(GL_UNIFORM_BUFFER, UBO_SCENE, resources.sceneUbo);
      glBindBufferBase(GL_UNIFORM_BUFFER, UBO_MATERIAL, scene->m_materialsGL);
    }

    nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_MATRICES, GL_TEXTURE_BUFFER, scene->m_matricesTexGL);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    {
      ShadeCommand& sc = m_shades[shadetype];
      if (vbum){
  #if USE_GPU_INDIRECT
        glBufferAddressRangeNV(GL_DRAW_INDIRECT_ADDRESS_NV, 0,       sc.indirectADDR, sc.indirects.size() * sizeof(IndexedCommand) );
  #endif
  #if USE_VERTEX_ASSIGNS
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 1, sc.assignADDR, sc.assigns.size() * sizeof(GLint));
  #endif
      }
      else{
  #if USE_GPU_INDIRECT
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, sc.indirectGL);
  #endif
  #if USE_VERTEX_ASSIGNS
        glBindVertexBuffer(1, sc.assignGL, 0, sizeof(GLint)*2);
  #endif
      }
  #if USE_CPU_INDIRECT
      size_t offset = (size_t)&sc.indirects[0];
  #else
      size_t offset = 0;
  #endif

      int lastGeometry = -1;
      bool lastSolid  = true;
      for (size_t i = 0; i < sc.geometries.size(); i++){
        int geometryIndex = sc.geometries[i];

        if (geometryIndex != lastGeometry){
          const CadScene::Geometry& geo = m_scene->m_geometry[ geometryIndex ];
          if (vbum){
            glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0,  geo.vboADDR, geo.numVertices * sizeof(CadScene::Vertex));
            glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,         geo.iboADDR, (geo.numIndexSolid+geo.numIndexWire) * sizeof(GLuint));
          }
          else{
            glBindVertexBuffer(0, geo.vboGL, 0, sizeof(CadScene::Vertex));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.iboGL);
          }
          lastGeometry = geometryIndex;
        }

        bool solid = sc.solids[i];
        if (solid != lastSolid){
          SetWireMode((!solid));
        }

        glMultiDrawElementsIndirect(solid ? GL_TRIANGLES : GL_LINES,GL_UNSIGNED_INT, (const void*)(offset + sc.offsets[i] * sizeof(IndexedCommand)), GLsizei(sc.sizes[i]), 0);

        lastSolid = solid;
      }
    }
#if USE_VERTEX_ASSIGNS
    glDisableVertexAttribArray(VERTEX_ASSIGNS);
    glBindVertexBuffer(1,0,0,0);
    glVertexBindingDivisor(1,0);
#endif

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    nvgl::bindMultiTexture(GL_TEXTURE0 + TEX_MATRICES, GL_TEXTURE_BUFFER, 0);

    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexBuffer(0,0,0,0);

    if (vbum){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
#if USE_GPU_INDIRECT
      glDisableClientState(GL_DRAW_INDIRECT_UNIFIED_NV);
#endif
      if (s_bindless_ubo){
        glDisableClientState(GL_UNIFORM_BUFFER_UNIFIED_NV);
      }
    }

    if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
      glDisable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(0,0);
    }

    SetWireMode(GL_FALSE);

    scene->disableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

  }

}
