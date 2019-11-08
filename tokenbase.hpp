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

// a few performance tests
// only affect TOKEN techniques
#define USE_RESETADDRESSES    1
#define USE_FASTDRAWS         1
#define USE_STATEFBO_SPLIT    0 //otherwise fbo[] as used
#define USE_POLYOFFSETTOKEN   1

// only affects TOKEN
#define USE_STATEOBJ_REBUILD  0 // does 100 statecaptures per frame
#define USE_NOFILTER          0

// only affects TOKENSORT
#define USE_PERFRAMEBUILD     0




#include <assert.h>
#include <algorithm>
#include "renderer.hpp"
#include "nvtoken.hpp"

using namespace nvtoken;

namespace csfviewer
{
#define UBOSTAGE_VERTEX     (nvtoken::s_nvcmdlist_stages[NVTOKEN_STAGE_VERTEX])
#define UBOSTAGE_FRAGMENT   (nvtoken::s_nvcmdlist_stages[NVTOKEN_STAGE_FRAGMENT])



#if USE_FASTDRAWS
  #define NVTokenDrawElemsUsed  NVTokenDrawElems
#else
  #define NVTokenDrawElemsUsed  NVTokenDrawElemsInstanced
#endif

  class TokenRendererBase {
  public:
    enum StateType {
      STATE_TRIS,
      STATE_TRISOFFSET,
      STATE_LINES,
      STATE_LINES_SPLIT,
      NUM_STATES,
    };

    struct ShadeCommand {
      std::vector<GLuint64>   addresses;
      std::vector<GLintptr>   offsets;
      std::vector<GLsizei>    sizes;
      std::vector<GLuint>     states;
      std::vector<GLuint>     fbos;
    };

    bool  m_emulate;
    bool  m_sort;
    bool  m_uselist;
    bool  m_useaddress;

    TokenRendererBase()
      : m_hwsupport(false)
      , m_bindlessVboUbo(false)
      , m_useaddress(false)
      , m_emulate(false)
      , m_uselist(false)
      , m_sort(false)
      , m_stateChangeID(~0)
      , m_fboStateChangeID(~0)
    {

    }

    static bool hasNativeCommandList();

  protected:

    bool                        m_hwsupport;
    bool                        m_bindlessVboUbo;

    GLuint                      m_tokenBuffers[NUM_SHADES];
    GLuint64                    m_tokenAddresses[NUM_SHADES];
    std::string                 m_tokenStreams[NUM_SHADES];
    GLuint                      m_commandLists[NUM_SHADES];
    ShadeCommand                m_shades[NUM_SHADES];

    size_t                      m_stateChangeID;
    size_t                      m_fboStateChangeID;

    StateSystem                 m_stateSystem;
    StateSystem::StateID        m_stateIDs[NUM_STATES];
    GLuint                      m_stateObjects[NUM_STATES];

    void init(bool bindlessUbo, bool bindlessVbo);
    void printStats(ShadeType shadeType);
    void finalize(const Resources &resources, bool fillBuffers=true);
    void deinit();

    void captureState(const Resources &resources);

    void renderShadeCommandSW( const void* NV_RESTRICT stream, size_t streamSize, ShadeCommand &shade );
  };
}
