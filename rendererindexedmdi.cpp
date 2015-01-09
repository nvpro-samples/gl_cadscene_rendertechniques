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
        return !!GLEW_NV_vertex_buffer_unified_memory;
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

  private:
    struct ObjectExtras {
      GLuint    assignGL;
      GLuint    indirectGL;
      GLuint64  assignADDR;
      GLuint64  indirectADDR;

      size_t    offsetSolid;
      size_t    offsetWire;

      int       numSolid;
      int       numWire;

      int       numAssignsSolid;
      int       numAssignsWire;

      ObjectExtras() : assignGL(0) , indirectGL(0) {}
    };


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

  public:
    void init(const CadScene* __restrict scene, const Resources& resources);
    void deinit();
    void draw(ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager);

    bool                        m_vbum;


    RendererIndexedMDI()
      : m_vbum(false) 
    {

    }

  private:

    std::vector<ObjectExtras>   m_xtras;

    void FillCached( const CadScene::DrawRangeCache &cache, const CadScene::Object& obj, std::vector<int> &assigns, std::vector<IndexedCommand> &commands ) 
    {
      int draws = 0;
      int numAssigns = int(assigns.size())/2;

      for (size_t s = 0; s < cache.state.size(); s++)
      {
        const CadScene::DrawStateInfo &state = cache.state[s];
        // push indices
        assigns.push_back(state.matrixIndex);
        assigns.push_back(state.materialIndex);
        numAssigns++;

        // fill indirect
        for (int d = 0; d < cache.stateCount[s]; d++, draws++){
          IndexedCommand  idxcmd;
          idxcmd.cmd.count        = cache.counts[draws];
          idxcmd.cmd.firstIndex   = GLuint(cache.offsets[draws]/sizeof(GLuint));
          idxcmd.cmd.baseInstance = numAssigns-1;
          commands.push_back(idxcmd);
        }
      }
    }

    void FillJoin( const CadScene::Object& obj, const CadScene::Geometry& geo, std::vector<int> &assigns, std::vector<IndexedCommand> &commands, bool solid ) 
    {
      CadScene::DrawRange range;

      // always enforce setting once per new object
      int lastMaterial = -1;
      int lastMatrix   = -1;
      int numAssigns = int(assigns.size())/2;

      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active) continue;

        if (part.materialIndex != lastMaterial || part.matrixIndex != lastMatrix){
          // evict
          if (range.count){
            IndexedCommand  idxcmd;
            idxcmd.cmd.count        = range.count;
            idxcmd.cmd.firstIndex   = GLuint(range.offset/sizeof(GLuint));
            idxcmd.cmd.baseInstance = numAssigns-1;
            commands.push_back(idxcmd);
          }

          range = CadScene::DrawRange();

          // push indices
          assigns.push_back(part.matrixIndex);
          assigns.push_back(part.materialIndex);
          numAssigns++;

          lastMaterial = part.materialIndex;
          lastMatrix   = part.matrixIndex;
        }

        if (!range.count){
          range.offset = solid ? mesh.indexSolid.offset : mesh.indexWire.offset;
        }

        range.count += solid ?  mesh.indexSolid.count : mesh.indexWire.count;
      }

      // evict
      IndexedCommand  idxcmd;
      idxcmd.cmd.count        = range.count;
      idxcmd.cmd.firstIndex   = GLuint(range.offset/sizeof(GLuint));
      idxcmd.cmd.baseInstance = numAssigns-1;
      commands.push_back(idxcmd);
    }

    void FillIndividual( const CadScene::Object& obj, const CadScene::Geometry& geo, std::vector<int> &assigns, std::vector<IndexedCommand> &commands, bool solid ) 
    {
      // always enforce setting once per new object
      int lastMaterial = -1;
      int lastMatrix   = -1;
      int numAssigns = int(assigns.size())/2;
      for (size_t p = 0; p < obj.parts.size(); p++){
        const CadScene::ObjectPart&   part = obj.parts[p];
        const CadScene::GeometryPart& mesh = geo.parts[p];

        if (!part.active) continue;

        if (part.materialIndex != lastMaterial || part.matrixIndex != lastMatrix){
          // push indices
          assigns.push_back(part.matrixIndex);
          assigns.push_back(part.materialIndex);
          numAssigns++;

          lastMaterial = part.materialIndex;
          lastMatrix   = part.matrixIndex;
        }


        IndexedCommand  idxcmd;
        idxcmd.cmd.count        = solid ? mesh.indexSolid.count : mesh.indexWire.count;
        idxcmd.cmd.firstIndex   = GLuint((solid ?  mesh.indexSolid.offset : mesh.indexWire.offset)/sizeof(GLuint));
        idxcmd.cmd.baseInstance = numAssigns-1;
        commands.push_back(idxcmd);
      }
    }

  };

  static RendererIndexedMDI::Type s_indexed;
  static RendererIndexedMDI::TypeVbum s_indexed_vbum;

  void RendererIndexedMDI::init( const CadScene* __restrict scene, const Resources& resources )
  {
    m_scene = scene;

    m_xtras.resize(scene->m_objects.size());

    int vbochanges = 0;
    int ibochanges = 0;
    int ubochanges = 1;
    int drawcalls  = 0;
    int drawcmds   = 0;

    int lastGeometry = -1;
    for (size_t i = 0; i < scene->m_objects.size(); i++){
      ObjectExtras&  xtra = m_xtras[i];
      const CadScene::Object& obj = scene->m_objects[i];
      const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

      std::vector<int>            assigns;
      std::vector<IndexedCommand> commands;

      assigns.reserve( obj.parts.size() * 2 );
      commands.reserve( obj.parts.size() );

      if (obj.geometryIndex != lastGeometry){

        vbochanges++;
        ibochanges++;

        lastGeometry = obj.geometryIndex;
      }

      vbochanges++;

      if (m_strategy == STRATEGY_GROUPS){
        FillCached(obj.cacheSolid, obj, assigns, commands);
      }
      else if (m_strategy == STRATEGY_JOIN) {
        FillJoin(obj, geo, assigns, commands, true);
      }
      else if (m_strategy == STRATEGY_INDIVIDUAL){
        FillIndividual(obj, geo, assigns, commands, true);
      }

      xtra.offsetSolid = 0;
      xtra.numSolid    = int(commands.size());
      xtra.numAssignsSolid = int(assigns.size())/2;

      if (m_strategy == STRATEGY_GROUPS){
        FillCached(obj.cacheWire, obj, assigns, commands);
      }
      else if (m_strategy == STRATEGY_JOIN) {
        FillJoin(obj, geo, assigns, commands, false);
      }
      else if (m_strategy == STRATEGY_INDIVIDUAL){
        FillIndividual(obj, geo, assigns, commands, false);
      }

      xtra.offsetWire = xtra.numSolid * sizeof(IndexedCommand);
      xtra.numWire    = int(commands.size()) - xtra.numSolid;
      xtra.numAssignsWire = int(assigns.size())/2 - xtra.numAssignsSolid;

      glGenBuffers(1,&xtra.assignGL);
      glNamedBufferStorageEXT(xtra.assignGL, assigns.size()*sizeof(GLuint), &assigns[0], 0 );

      glGenBuffers(1,&xtra.indirectGL);
      glNamedBufferStorageEXT(xtra.indirectGL, commands.size()*sizeof(IndexedCommand), &commands[0], 0 );

      drawcalls++;
      drawcmds += xtra.numSolid;

      if (m_vbum){
        glMakeNamedBufferResidentNV(xtra.assignGL,    GL_READ_ONLY);
        glMakeNamedBufferResidentNV(xtra.indirectGL,  GL_READ_ONLY);
        glGetNamedBufferParameterui64vNV(xtra.assignGL,   GL_BUFFER_GPU_ADDRESS_NV, &xtra.assignADDR);
        glGetNamedBufferParameterui64vNV(xtra.indirectGL, GL_BUFFER_GPU_ADDRESS_NV, &xtra.indirectADDR);
      }

    }
    printf("stats:\n");
    printf("  vbochanges: %6d\n",vbochanges);
    printf("  ibochanges: %6d\n",ibochanges);
    printf("  ubochanges: %6d\n",ubochanges);
    printf("  drawcalls : %6d\n",drawcalls);
    printf("  drawcmds  : %6d\n",drawcmds);
    printf("\n");
  }

  void RendererIndexedMDI::deinit()
  {
    for (size_t i = 0; i < m_xtras.size(); i++){
      if (m_vbum){
        glMakeNamedBufferNonResidentNV(m_xtras[i].indirectGL);
        glMakeNamedBufferNonResidentNV(m_xtras[i].assignGL);
      }
      glDeleteBuffers(1,&m_xtras[i].indirectGL);
      glDeleteBuffers(1,&m_xtras[i].assignGL);
    }

    m_xtras.clear();
  }

  void RendererIndexedMDI::draw( ShadeType shadetype, const Resources& resources, nv_helpers_gl::Profiler& profiler, nv_helpers_gl::ProgramManager &progManager )
  {
    const CadScene* __restrict scene = m_scene;
    bool vbum = m_vbum;

    scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

    glUseProgram(resources.programIdx);

    if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
      glEnable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(1,1);
    }

    SetWireMode(GL_FALSE);

    glVertexAttribIFormat(VERTEX_ASSIGNS,2,GL_INT,0);
    glVertexAttribBinding(VERTEX_ASSIGNS,1);
    glEnableVertexAttribArray(VERTEX_ASSIGNS);
    glBindVertexBuffer(1,0,0,sizeof(GLint)*2);
    glVertexBindingDivisor(1,1);

    if (vbum){
      glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glEnableClientState(GL_DRAW_INDIRECT_UNIFIED_NV);
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

    glBindTextures(TEX_MATRICES,1,&scene->m_matricesTexGL);

    int lastGeometry = -1;
    for (size_t i = 0; i < m_scene->m_objects.size(); i++){
      const ObjectExtras& xtra = m_xtras[i];
      const CadScene::Object& obj = scene->m_objects[i];
      const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

      if (obj.geometryIndex != lastGeometry){

        if (vbum){
          glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0,  geo.vboADDR, geo.numVertices * sizeof(CadScene::Vertex));
          glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,         geo.iboADDR, (geo.numIndexSolid+geo.numIndexWire) * sizeof(GLuint));
        }
        else{
          glBindVertexBuffer(0, geo.vboGL, 0, sizeof(CadScene::Vertex));
          glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.iboGL);
        }

        lastGeometry = obj.geometryIndex;
      }


      if (vbum){
        glBufferAddressRangeNV(GL_DRAW_INDIRECT_ADDRESS_NV, 0,       xtra.indirectADDR, sizeof(IndexedCommand) * (xtra.numSolid + xtra.numWire));
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 1, xtra.assignADDR, (xtra.numAssignsSolid + xtra.numAssignsWire) * sizeof(GLint) * 2);
      }
      else{
        glBindVertexBuffer(1, xtra.assignGL, 0, sizeof(GLint)*2);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, xtra.indirectGL);
      }

      if (shadetype == SHADE_SOLID || shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
        glMultiDrawElementsIndirect(GL_TRIANGLES,GL_UNSIGNED_INT, (const void*)xtra.offsetSolid, xtra.numSolid, 0);
      }
      if (shadetype == SHADE_SOLIDWIRE || shadetype == SHADE_SOLIDWIRE_SPLIT){
        SetWireMode(GL_TRUE);
        glMultiDrawElementsIndirect(GL_LINES,GL_UNSIGNED_INT, (const void*)xtra.offsetWire, xtra.numWire, 0);
        SetWireMode(GL_FALSE);
      }

    }

    glDisableVertexAttribArray(VERTEX_ASSIGNS);
    glBindVertexBuffer(1,0,0,0);
    glVertexBindingDivisor(1,0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

    GLuint zero = 0;
    glBindTextures(TEX_MATRICES,1,&zero);

    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_SCENE, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER,UBO_MATERIAL, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexBuffer(0,0,0,0);

    if (vbum){
      glDisableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
      glDisableClientState(GL_DRAW_INDIRECT_UNIFIED_NV);
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
