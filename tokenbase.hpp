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
