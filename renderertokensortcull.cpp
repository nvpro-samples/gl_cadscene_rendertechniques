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
#include "cullingsystem.hpp"

#include <nvmath/nvmath_glsltypes.h>

#include "common.h"

namespace csfviewer
{
  //////////////////////////////////////////////////////////////////////////

#define USE_TEMPORALRASTER      1
#define USE_OBJECTSORT_CULLING  1


  class RendererCullSortToken : public Renderer, public TokenRendererBase {
  public:
    class Shared {
    public:
      nvgl::ProgramID 
        token_sizes,
        token_scan,
        token_cmds;

      static Shared& get()
      {
        static Shared res;
        return res;
      }

      Shared() : loaded(false) {}

      bool load(nvgl::ProgramManager &progManager)
      {
        if (loaded) return true;

        loaded = true;

        token_sizes = progManager.createProgram(
          nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "cull-tokensizes.vert.glsl"));
        token_cmds = progManager.createProgram(
          nvgl::ProgramManager::Definition(GL_VERTEX_SHADER, "cull-tokencmds.vert.glsl"));

        if (!progManager.areProgramsValid()) return false;

        return true;
      }

    private:
      bool loaded;
    };

    class Type : public Renderer::Type 
    {
      bool isAvailable() const
      {
        return TokenRendererBase::hasNativeCommandList();
      }
      const char* name() const
      {
        return "tokenbuffer_cullsorted";
      }
      Renderer* create() const
      {
        RendererCullSortToken* renderer = new RendererCullSortToken();
        return renderer;
      }
      bool loadPrograms( nvgl::ProgramManager &mgr)
      {
        return Shared::get().load(mgr);
      }
      unsigned int priority() const 
      {
        return 9;
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
        return "tokenbuffer_cullsorted_emulated";
      }
      Renderer* create() const
      {
        RendererCullSortToken* renderer = new RendererCullSortToken();
        renderer->m_emulate = true;
        return renderer;
      }
      bool loadPrograms( nvgl::ProgramManager &mgr )
      {
        return Shared::get().load(mgr);
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
    void drawScene(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager, const char*what);

  private:

    static bool DrawItem_compare_groups(const DrawItem& a, const DrawItem& b)
    {
      int diff = 0;
      diff = diff != 0 ? diff : (a.solid == b.solid ? 0 : ( a.solid ? -1 : 1 ));
#if USE_OBJECTSORT_CULLING
      diff = diff != 0 ? diff : (a.objectIndex - b.objectIndex);
#endif
      diff = diff != 0 ? diff : (a.materialIndex - b.materialIndex);
      diff = diff != 0 ? diff : (a.geometryIndex - b.geometryIndex);
      diff = diff != 0 ? diff : (a.matrixIndex - b.matrixIndex);

      return diff < 0;
    }

    struct CullSequence {
      GLuint    offset;
      GLint     endoffset;
      int       first;
      int       num;
    };

    struct CullShade {
      GLuint                    numTokens;
      std::vector<CullSequence> sequnces;

      // static buffers
      ScanSystem::Buffer   tokenOrig;

      // for each command, #cmds rounded to multiple of 4
      ScanSystem::Buffer   tokenSizes;   // in integers
      ScanSystem::Buffer   tokenObjects; // -1 if no drawcall, otherwise object
      ScanSystem::Buffer   tokenOffsets; // offsets for each command

      ScanSystem::Buffer   tokenOutSizes;
      ScanSystem::Buffer   tokenOutScan;
      ScanSystem::Buffer   tokenOutScanOffset;
    };

    class CullJobToken : public CullingSystem::Job
    {
    public:
      void resultFromBits( const CullingSystem::Buffer& bufferVisBitsCurrent );

      GLuint      program_sizes;
      GLuint      program_cmds;

      // dynamic
      ScanSystem::Buffer   tokenOut;

      CullShade* NV_RESTRICT cullshade;
    };

    std::vector<DrawItem>       m_drawItems;

    CullJobToken                m_culljob;
    CullShade                   m_cullshades[NUM_SHADES];
    GLuint                      m_maxGrps;

    void PrepareCullJob(ShadeType shade);


    template <class T>
    static void handleToken(std::vector<GLuint> &tokenSizes, std::vector<GLuint> &tokenOffsets,std::vector<GLint>&  tokenObjects, T &token, size_t stream, int obj=-1)
    {
      tokenSizes.push_back(GLuint(sizeof(T) / sizeof(GLuint) ));
      tokenOffsets.push_back(GLuint( (stream - sizeof(T))/ sizeof(GLuint) ));
      tokenObjects.push_back(obj);
    }

    void GenerateTokens(std::vector<DrawItem>& drawItems, ShadeType shade, const CadScene* NV_RESTRICT scene, const Resources& resources )
    {
      int lastMaterial = -1;
      int lastGeometry = -1;
      int lastMatrix   = -1;
      int lastObject   = -1;
      bool lastSolid   = true;

      ShadeCommand& sc = m_shades[shade];
      CullShade& cull = m_cullshades[shade];

      sc.fbos.clear();
      sc.offsets.clear();
      sc.sizes.clear();
      sc.states.clear();

      std::string& tokenStream = m_tokenStreams[shade];
      tokenStream.clear();


      cull.numTokens = 0;
      GLuint beginToken = 0;

      size_t begin = 0;
      size_t start = begin;

      std::vector<GLuint> tokenSizes;
      std::vector<GLuint> tokenOffsets;
      std::vector<GLint>  tokenObjects;

      {
        NVTokenUbo ubo;
        ubo.cmd.index   = UBO_SCENE;
        ubo.cmd.stage   = UBOSTAGE_VERTEX;
        ubo.setBuffer(resources.sceneUbo, resources.sceneAddr, 0, sizeof(SceneData) );
        nvtokenEnqueue(tokenStream, ubo);
        handleToken(tokenSizes,tokenOffsets,tokenObjects, ubo, tokenStream.size()-start, -1);
        cull.numTokens++;

        ubo.cmd.stage   = UBOSTAGE_FRAGMENT;
        nvtokenEnqueue(tokenStream, ubo);
        handleToken(tokenSizes,tokenOffsets,tokenObjects, ubo, tokenStream.size()-start, -1);
        cull.numTokens++;

#if USE_POLYOFFSETTOKEN
        NVTokenPolygonOffset offset;
        offset.cmd.bias = 1;
        offset.cmd.scale = 1;
        nvtokenEnqueue(tokenStream, offset);
        handleToken(tokenSizes,tokenOffsets,tokenObjects, offset, tokenStream.size()-start, -1);
        cull.numTokens++;
#endif
      }

      for (int i = 0; i < drawItems.size(); i++){
        const DrawItem& di = drawItems[i];

        if (shade == SHADE_SOLID && !di.solid){
          continue;
        }

        int bufferObjIndex = -1;
#if USE_OBJECTSORT_CULLING
        bufferObjIndex = di.objectIndex;
        if (di.objectIndex != lastObject || di.solid != lastSolid){
          // whenever an object changes or we switches from solid to edges (happens only once in this sorted scenario)
          // we have to ensure all buffers are reset as well
          lastObject = di.objectIndex;
          lastMaterial = -1;
          lastGeometry = -1;
          lastMatrix   = -1;
        }
#endif

        if (shade == SHADE_SOLIDWIRE && di.solid != lastSolid){
          sc.offsets.push_back( begin );
          sc.sizes.  push_back( GLsizei((tokenStream.size()-begin)) );
          sc.states. push_back( m_stateObjects[ lastSolid ? STATE_TRISOFFSET : STATE_LINES ] );
          sc.fbos.   push_back( 0 );
          CullSequence cullseq;
          cullseq.num     = cull.numTokens - beginToken;
          cullseq.first   = beginToken;
          cullseq.offset  = GLuint((begin-start)/sizeof(GLuint));
          cullseq.endoffset = GLuint((tokenStream.size()-start)/sizeof(GLuint));
          cull.sequnces.push_back(cullseq);

          beginToken = cull.numTokens;
          begin = tokenStream.size();
        }

        if (lastGeometry != di.geometryIndex){
          const CadScene::Geometry &geo = scene->m_geometry[di.geometryIndex];
          NVTokenVbo vbo;
          vbo.cmd.index = 0;
          vbo.setBuffer(geo.vboGL, geo.vboADDR, 0);

          nvtokenEnqueue(tokenStream, vbo);
          handleToken(tokenSizes,tokenOffsets,tokenObjects, vbo, tokenStream.size()-start, bufferObjIndex);
          cull.numTokens++;

          NVTokenIbo ibo;
          ibo.setBuffer(geo.iboGL, geo.iboADDR);
          ibo.cmd.typeSizeInByte = 4;
          nvtokenEnqueue(tokenStream, ibo);
          handleToken(tokenSizes,tokenOffsets,tokenObjects, vbo, tokenStream.size()-start, bufferObjIndex);
          cull.numTokens++;

          lastGeometry = di.geometryIndex;
        }

        if (lastMatrix != di.matrixIndex){

          NVTokenUbo ubo;
          ubo.cmd.index   = UBO_MATRIX;
          ubo.cmd.stage   = UBOSTAGE_VERTEX;
          ubo.setBuffer(scene->m_matricesGL, scene->m_matricesADDR, sizeof(CadScene::MatrixNode) * di.matrixIndex, sizeof(CadScene::MatrixNode) );
          nvtokenEnqueue(tokenStream, ubo);
          handleToken(tokenSizes,tokenOffsets,tokenObjects, ubo, tokenStream.size()-start, bufferObjIndex);
          cull.numTokens++;

          lastMatrix = di.matrixIndex;
        }

        if (lastMaterial != di.materialIndex){

          NVTokenUbo ubo;
          ubo.cmd.index   = UBO_MATERIAL;
          ubo.cmd.stage   = UBOSTAGE_FRAGMENT;
          ubo.setBuffer(scene->m_materialsGL, scene->m_materialsADDR, sizeof(CadScene::Material) * di.materialIndex, sizeof(CadScene::Material) );
          nvtokenEnqueue(tokenStream, ubo);
          handleToken(tokenSizes,tokenOffsets,tokenObjects, ubo, tokenStream.size()-start, bufferObjIndex);
          cull.numTokens++;

          lastMaterial = di.materialIndex;
        }


        NVTokenDrawElemsUsed drawelems;
        drawelems.setMode(di.solid ? GL_TRIANGLES : GL_LINES);
        drawelems.cmd.count = di.range.count;
        drawelems.cmd.firstIndex = GLuint((di.range.offset )/sizeof(GLuint));
        nvtokenEnqueue(tokenStream, drawelems);
        handleToken(tokenSizes,tokenOffsets,tokenObjects, drawelems, tokenStream.size()-start, di.objectIndex);
        cull.numTokens++;

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

      CullSequence cullseq;
      cullseq.num     = cull.numTokens - beginToken;
      cullseq.first   = beginToken;
      cullseq.offset  = GLuint((begin-start)/sizeof(GLuint));
      cullseq.endoffset = GLuint((tokenStream.size()-start)/sizeof(GLuint));
      cull.sequnces.push_back(cullseq);

      // create buffers for culling
      cull.tokenOrig.create(tokenStream.size() - start,&tokenStream[start], 0);

      cull.tokenOffsets.create(sizeof(GLuint)*cull.numTokens,&tokenOffsets[0], 0);
      cull.tokenSizes.  create(sizeof(GLuint)*cull.numTokens,&tokenSizes[0], 0);
      cull.tokenObjects.create(sizeof(GLint)*cull.numTokens,&tokenObjects[0], 0);

      int round4 = ((cull.numTokens+3)/4)*4;

      cull.tokenOutScan.      create(sizeof(GLuint)*round4,NULL, 0);
      cull.tokenOutScanOffset.create(std::max(ScanSystem::getOffsetSize(round4), size_t(16)),NULL, 0);
      cull.tokenOutSizes.     create(sizeof(GLuint)*round4,NULL, 0);
    }

  };


  // not yet fully implemented
  static RendererCullSortToken::Type s_cullsorttoken;
  static RendererCullSortToken::TypeEmu s_cullsorttoken_emu;


  void RendererCullSortToken::init(const CadScene* NV_RESTRICT scene, const Resources& resources)
  {
    TokenRendererBase::init(s_bindless_ubo, !!has_GL_NV_vertex_buffer_unified_memory);
    resources.usingUboProgram(true);

    m_scene = scene;
    glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT,0,(GLint*)&m_maxGrps);

    std::vector<DrawItem> drawItems;

    fillDrawItems(drawItems,0,scene->m_objects.size(), true, true);

    std::sort(drawItems.begin(),drawItems.end(),DrawItem_compare_groups);

    GenerateTokens(drawItems, SHADE_SOLID, scene, resources);

    TokenRendererBase::printStats(SHADE_SOLID);

    GenerateTokens(drawItems, SHADE_SOLIDWIRE, scene, resources);

    TokenRendererBase::printStats(SHADE_SOLIDWIRE);

    TokenRendererBase::finalize(resources);

    if (m_emulate){
      for (int i = 0; i < NUM_SHADES; i++){
        glNamedBufferStorage(m_tokenBuffers[i], m_tokenStreams[i].size(), &m_tokenStreams[i][0], GL_MAP_READ_BIT);
      }
    }

    m_culljob.m_numObjects = int(m_scene->m_objects.size());

    int roundedBits = (m_culljob.m_numObjects+31)/32;
    int roundedInts = roundedBits*32;

    m_culljob.m_bufferBboxes    = CullingSystem::Buffer(m_scene->m_geometryBboxesGL, sizeof(CadScene::BBox) * m_scene->m_geometryBboxes.size());
    m_culljob.m_bufferMatrices  = CullingSystem::Buffer(m_scene->m_matricesGL, sizeof(CadScene::MatrixNode) * m_scene->m_matrices.size());
    m_culljob.m_bufferObjectMatrix  = CullingSystem::Buffer(m_scene->m_objectAssignsGL, sizeof(GLint)*2* m_scene->m_objectAssigns.size());
    m_culljob.m_bufferObjectMatrix.stride = sizeof(GLint)*2;
    m_culljob.m_bufferObjectBbox    = m_culljob.m_bufferObjectMatrix;
    m_culljob.m_bufferObjectBbox.offset = sizeof(GLint);
    m_culljob.m_bufferObjectBbox.size  -= sizeof(GLint);
    m_culljob.m_bufferObjectBbox.stride = sizeof(GLint)*2;

    m_culljob.m_bufferVisBitsCurrent.create(sizeof(int)*roundedBits,NULL,0);
    GLuint full = ~0;
    glClearNamedBufferData(m_culljob.m_bufferVisBitsCurrent.buffer,GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,&full);
    m_culljob.m_bufferVisBitsLast.create(sizeof(int)*roundedBits,NULL,0);
    glClearNamedBufferData(m_culljob.m_bufferVisBitsLast.buffer,GL_R32UI,GL_RED_INTEGER,GL_UNSIGNED_INT,0);

    m_culljob.m_bufferVisOutput.create(sizeof(int)*roundedInts,NULL,0);
    m_cullshades[SHADE_SOLIDWIRE_SPLIT] = m_cullshades[SHADE_SOLIDWIRE];
  }

  void RendererCullSortToken::deinit()
  {
    for (int i = 0; i < 2; i++){
      CullShade &cs = m_cullshades[i];
      glDeleteBuffers(1,&cs.tokenOrig.buffer);
      glDeleteBuffers(1,&cs.tokenOffsets.buffer);
      glDeleteBuffers(1,&cs.tokenSizes.buffer);
      glDeleteBuffers(1,&cs.tokenObjects.buffer);

      glDeleteBuffers(1,&cs.tokenOutScan.buffer);
      glDeleteBuffers(1,&cs.tokenOutScanOffset.buffer);
      glDeleteBuffers(1,&cs.tokenOutSizes.buffer);
    }

    glDeleteBuffers(1,&m_culljob.m_bufferVisBitsCurrent.buffer);
    glDeleteBuffers(1,&m_culljob.m_bufferVisBitsLast.buffer);
    glDeleteBuffers(1,&m_culljob.m_bufferVisOutput.buffer);


    TokenRendererBase::deinit();
    m_drawItems.clear();
  }

  void RendererCullSortToken::PrepareCullJob(ShadeType shade)
  {
    ShadeCommand& sc = m_shades[shade];
    RendererCullSortToken::CullJobToken& job = m_culljob;

    job.cullshade = &m_cullshades[shade];

    // setup buffer offsets
    job.tokenOut.buffer = m_tokenBuffers[shade];
    job.tokenOut.offset = sc.offsets[0];
    job.tokenOut.size   = m_cullshades[shade].tokenOrig.size;
  }

  void RendererCullSortToken::CullJobToken::resultFromBits( const CullingSystem::Buffer& bufferVisBitsCurrent )
  {
    // first compute sizes based on culling result
    glUseProgram(program_sizes);

    glBindBuffer(GL_ARRAY_BUFFER, cullshade->tokenSizes.buffer);
    glVertexAttribIPointer(0,1,GL_UNSIGNED_INT,0,(const void*)cullshade->tokenSizes.offset);
    glBindBuffer(GL_ARRAY_BUFFER, cullshade->tokenObjects.buffer);
    glVertexAttribIPointer(1,1,GL_INT,0,(const void*)cullshade->tokenObjects.offset);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    cullshade->tokenOutSizes.BindBufferRange(GL_SHADER_STORAGE_BUFFER,0);
    bufferVisBitsCurrent.BindBufferRange(GL_SHADER_STORAGE_BUFFER,1);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    GLuint numTokens = cullshade->numTokens;

    glEnable(GL_RASTERIZER_DISCARD);
    glDrawArrays(GL_POINTS,0, numTokens);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);

    Renderer::s_scansys.scanData(((numTokens+3)/4)*4,cullshade->tokenOutSizes,cullshade->tokenOutScan,cullshade->tokenOutScanOffset);

    glUseProgram(program_cmds);
    glUniform1ui(glGetUniformLocation(program_cmds,"terminateCmd"),s_nvcmdlist_header[GL_TERMINATE_SEQUENCE_COMMAND_NV]);

    glBindBuffer(GL_ARRAY_BUFFER, cullshade->tokenOffsets.buffer);
    glVertexAttribIPointer(0,1,GL_UNSIGNED_INT,0,(const void*)cullshade->tokenOffsets.offset);
    glBindBuffer(GL_ARRAY_BUFFER, cullshade->tokenOutSizes.buffer);
    glVertexAttribIPointer(1,1,GL_UNSIGNED_INT,0,(const void*)cullshade->tokenOutSizes.offset);
    glBindBuffer(GL_ARRAY_BUFFER, cullshade->tokenOutScan.buffer);
    glVertexAttribIPointer(2,1,GL_UNSIGNED_INT,0,(const void*)cullshade->tokenOutScan.offset);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    tokenOut.BindBufferRange(GL_SHADER_STORAGE_BUFFER,0);
    cullshade->tokenOrig.BindBufferRange(GL_SHADER_STORAGE_BUFFER,1);
    cullshade->tokenOutSizes.BindBufferRange(GL_SHADER_STORAGE_BUFFER,2);
    cullshade->tokenOutScan.BindBufferRange(GL_SHADER_STORAGE_BUFFER,3);
    cullshade->tokenOutScanOffset.BindBufferRange(GL_SHADER_STORAGE_BUFFER,4);

    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

    for (GLuint i = 0; i < cullshade->sequnces.size() ; i++){
      glUniform1ui(glGetUniformLocation(program_cmds,"startOffset"),cullshade->sequnces[i].offset);
      glUniform1i (glGetUniformLocation(program_cmds,"startID"),cullshade->sequnces[i].first);
      glUniform1ui(glGetUniformLocation(program_cmds,"endOffset"),cullshade->sequnces[i].endoffset);
      glUniform1i (glGetUniformLocation(program_cmds,"endID"),cullshade->sequnces[i].first + cullshade->sequnces[i].num - 1);
      glDrawArrays(GL_POINTS,cullshade->sequnces[i].first,cullshade->sequnces[i].num);
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER,0);

    for (GLuint i = 0; i < 5; i++){
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER,i,0);
    }

    glDisable(GL_RASTERIZER_DISCARD);
  }

  void RendererCullSortToken::drawScene(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager, const char*what)
  {
    const CadScene* NV_RESTRICT scene = m_scene;

    nvh::Profiler::Section  section(profiler,what);

    // do state setup (primarily for sake of state capturing)
    m_scene->enableVertexFormat(VERTEX_POS,VERTEX_NORMAL);

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


#define CULL_TEMPORAL_NOFRUSTUM 1

  void RendererCullSortToken::draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager)
  {
    // broken in other types atm
    //shadetype = SHADE_SOLID;

    m_culljob.program_cmds  = progManager.get( Shared::get().token_cmds );
    m_culljob.program_sizes = progManager.get( Shared::get().token_sizes );

    PrepareCullJob(shadetype);

    CullingSystem& cullSys = Renderer::s_cullsys;


#if !USE_TEMPORALRASTER

    {
      nvh::Profiler::Section section(profiler,"CullF");
      cullSys.buildOutput( CullingSystem::METHOD_FRUSTUM, m_culljob, resources.cullView );
      cullSys.bitsFromOutput( m_culljob, CullingSystem::BITS_CURRENT );
      {
        nvh::Profiler::Section section(profiler,"ResF");
        cullSys.resultFromBits( m_culljob );
      }

      if (m_emulate){
        nvh::Profiler::Section read(profiler,"Read");
        m_culljob.tokenOut.GetNamedBufferSubData(&m_tokenStream[m_culljob.tokenOut.offset]);
        GLuint* first = (GLuint*)&m_tokenStream[m_culljob.tokenOut.offset];
        first[0] = first[0];
      }
      else {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_culljob.tokenOut.buffer);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        //glFinish();
      }
    }

    drawScene(shadetype,resources,profiler,progManager, "Last");

#else

    {
      nvh::Profiler::Section section(profiler,"CullF");
#if CULL_TEMPORAL_NOFRUSTUM
      {
        nvh::Profiler::Section section(profiler,"ResF");
        cullSys.resultFromBits( m_culljob );
      }
      cullSys.swapBits( m_culljob );  // last/output
#else
      cullSys.buildOutput( CullingSystem::METHOD_FRUSTUM, m_culljob, resources.cullView );
      cullSys.bitsFromOutput( m_culljob, CullingSystem::BITS_CURRENT_AND_LAST );
      {
        nvh::Profiler::Section section(profiler,"ResF");
        cullSys.resultFromBits( m_culljob );
      }
#endif
      if (m_emulate){
        nvh::Profiler::Section read(profiler,"Read");
        void* data = &m_tokenStreams[shadetype][m_culljob.tokenOut.offset];
        m_culljob.tokenOut.GetNamedBufferSubData(data);
      }
      else {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_culljob.tokenOut.buffer);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        //glFinish();
      }
    }

    drawScene(shadetype,resources,profiler,progManager, "Last");

    {
      nvh::Profiler::Section section(profiler,"CullR");
      cullSys.buildOutput( CullingSystem::METHOD_RASTER, m_culljob, resources.cullView );
      cullSys.bitsFromOutput( m_culljob, CullingSystem::BITS_CURRENT_AND_NOT_LAST );
      {
        nvh::Profiler::Section section(profiler,"ResR");
        cullSys.resultFromBits( m_culljob );
      }

      // for next frame
      cullSys.bitsFromOutput( m_culljob, CullingSystem::BITS_CURRENT );
#if !CULL_TEMPORAL_NOFRUSTUM
      cullSys.swapBits( m_culljob );  // last/output
#endif
      if (m_emulate){
        nvh::Profiler::Section read(profiler,"Read");
        void* data = &m_tokenStreams[shadetype][m_culljob.tokenOut.offset];
        m_culljob.tokenOut.GetNamedBufferSubData(data);
      }
      else {
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_culljob.tokenOut.buffer);
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
        //glFinish();
      }
    }

    drawScene(shadetype,resources,profiler,progManager, "New");
#endif
  }

}
