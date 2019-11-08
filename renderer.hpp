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


#ifndef RENDERER_H__
#define RENDERER_H__

// bindless UBO
#ifndef GL_UNIFORM_BUFFER_UNIFIED_NV
#define GL_UNIFORM_BUFFER_UNIFIED_NV                        0x936E
#endif
#ifndef GL_UNIFORM_BUFFER_ADDRESS_NV
#define GL_UNIFORM_BUFFER_ADDRESS_NV                        0x936F
#endif
#ifndef GL_UNIFORM_BUFFER_LENGTH_NV
#define GL_UNIFORM_BUFFER_LENGTH_NV                         0x9370
#endif

#include "cadscene.hpp"
#include <NvFoundation.h>
#include <nvgl/programmanager_gl.hpp>
#include <nvgl/base_gl.hpp>
#include <nvh/profiler.hpp>
#include "cullingsystem.hpp"
#include "scansystem.hpp"

namespace csfviewer {
  #define USE_NOFILTER           0  // some renderers support turning off redundancy filter

  #define USE_WIRE_SHADERSWITCH  0  // If set we use two different shaders for tris and lines,
                                    // otherwise we use an immediate mode vertexattrib as pseudo uniform toggle.
                                    // Enable this to stress shader switching in app (becomes primary bottleneck)
  enum Strategy {
    STRATEGY_GROUPS,
    STRATEGY_JOIN,
    STRATEGY_INDIVIDUAL,
  };

  enum ShadeType {
    SHADE_SOLID,
    SHADE_SOLIDWIRE,
    SHADE_SOLIDWIRE_SPLIT, // this mode is not "sane" it is only meant for performance testing of fbo toggles
    NUM_SHADES,
  };

  const char* toString(enum ShadeType st);

  struct Resources {
    GLuint    sceneUbo;
    GLuint64  sceneAddr;

    GLuint    programUbo;
    GLuint    programUboTris;
    GLuint    programUboLine;

    GLuint    programIdx;
    GLuint    programIdxTris;
    GLuint    programIdxLine;

    GLuint    fbo;
    GLuint    fbo2;

    size_t    stateChangeID;
    size_t    fboTextureChangeID;

    CullingSystem::View cullView;

    // ugly hack
    mutable GLuint programUsed;
    mutable GLuint programUsedTris;
    mutable GLuint programUsedLine;

    void usingUboProgram(bool ubo=true) const
    {
      programUsed     = ubo ? programUbo     : programIdx;
      programUsedTris = ubo ? programUboTris : programIdxTris;
      programUsedLine = ubo ? programUboLine : programIdxLine;
    }

    Resources() {
      stateChangeID = 0;
      fboTextureChangeID = 0;
    }
  };

#if USE_WIRE_SHADERSWITCH
  #define SetWireMode(state) glUseProgram((state) ? resources.programUsedLine : resources.programUsedTris )
#else
  #define SetWireMode(state) glVertexAttribI1i(VERTEX_WIREMODE,(state))
#endif

  class Renderer {
  public:

    struct DrawItem {
      bool                solid;
      int                 materialIndex;
      int                 geometryIndex;
      int                 matrixIndex;
      int                 objectIndex;
      CadScene::DrawRange range;
    };

    static bool DrawItem_compare_groups(const DrawItem& a, const DrawItem& b)
    {
      int diff = 0;
      diff = diff != 0 ? diff : (a.solid == b.solid ? 0 : ( a.solid ? -1 : 1 ));
      diff = diff != 0 ? diff : (a.materialIndex - b.materialIndex);
      diff = diff != 0 ? diff : (a.geometryIndex - b.geometryIndex);
      diff = diff != 0 ? diff : (a.matrixIndex - b.matrixIndex);

      return diff < 0;
    }

    class Type {
    public:
      Type() {
        getRegistry().push_back(this);
      }

    public:
      virtual bool loadPrograms( nvgl::ProgramManager &mgr ) { return true; }
      virtual void updatedPrograms( nvgl::ProgramManager &mgr ) { }
      virtual bool isAvailable() const = 0;
      virtual const char* name() const = 0;
      virtual Renderer* create() const = 0;
      virtual unsigned int priority() const { return 0xFF; } 
    };

    typedef std::vector<Type*> Registry;

    static bool s_bindless_ubo;
    static Registry& getRegistry()
    {
      static Registry s_registry;
      return s_registry;
    }

    static CullingSystem   s_cullsys;
    static ScanSystem      s_scansys;

  public:
    virtual void init(const CadScene* NV_RESTRICT scene, const Resources& resources) {}
    virtual void deinit() {}
    virtual void draw(ShadeType shadetype, const Resources& resources, nvh::Profiler& profiler, nvgl::ProgramManager &progManager ) {}
    virtual ~Renderer() {}


    void fillDrawItems( std::vector<DrawItem>& drawItems, size_t from, size_t to, bool solid, bool wire);

    Strategy                    m_strategy;
    const CadScene* NV_RESTRICT  m_scene;
  };
}

#endif
