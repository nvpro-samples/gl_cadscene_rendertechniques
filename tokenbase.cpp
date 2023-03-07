/*
 * Copyright (c) 2014-2023, NVIDIA CORPORATION.  All rights reserved.
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

using namespace nvtoken;

#include <nvmath/nvmath_glsltypes.h>

#include "common.h"

namespace csfviewer
{

  bool TokenRendererBase::hasNativeCommandList()
  {
    return !!has_GL_NV_command_list;
  }

  void TokenRendererBase::init(bool bindlessUbo, bool bindlessVbo)
  {
    m_bindlessVboUbo = bindlessVbo && bindlessUbo;
    m_hwsupport = hasNativeCommandList() && !m_emulate;

    for (int i = 0; i < NUM_SHADES; i++){
      m_tokenAddresses[i] = 0;
    }

    if (m_hwsupport){
      glCreateStatesNV(NUM_STATES,m_stateObjects);

      if (m_uselist){
        glCreateCommandListsNV(NUM_SHADES,m_commandLists);
      }
    }
    else{
      // we use a fast mode for glBufferAddressRangeNV where we ignore precise buffer boundaries
      // this will trigger the driver to throw warnings, which may cause a crash
#if !defined (NDEBUG)
      if (m_bindlessVboUbo){
        glDisable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDisable(GL_DEBUG_OUTPUT);
      }
#endif

      m_stateSystem.init(false);
      m_stateSystem.generate(NUM_STATES,m_stateIDs);
      for (int i = 0; i < NUM_STATES; i++){
        m_stateObjects[i]  = m_stateIDs[i];
      }
    }

    nvtokenInitInternals(m_hwsupport, m_bindlessVboUbo);
  }

  void TokenRendererBase::printStats( ShadeType shadeType )
  {
    int stats[NVTOKEN_TYPES] = {0};

    ShadeCommand& sc = m_shades[shadeType];

    size_t num = sc.states.size();
    size_t size = sc.offsets[num-1] + sc.sizes[num-1] - sc.offsets[0];

    nvtokenGetStats(&m_tokenStreams[shadeType][sc.offsets[0]], size, stats);

    LOGI("type: %s\n",toString(shadeType));
    LOGI("commandsize: %zu\n",size);
    LOGI("state toggles: %zu\n", num);
    LOGI("tokens:\n");
    for (int i = 0; i < NVTOKEN_TYPES; i++){
      const char* what = nvtokenCommandToString(i);
      if (what && stats[i]){
        LOGI("%s:\t %6d\n", what,stats[i]);
      }
    }
    LOGI("\n");
  }

  void TokenRendererBase::finalize(const Resources &resources, bool fillBuffers)
  {
    {
      m_tokenStreams[SHADE_SOLIDWIRE_SPLIT] = m_tokenStreams[SHADE_SOLIDWIRE];
      m_shades[SHADE_SOLIDWIRE_SPLIT] = m_shades[SHADE_SOLIDWIRE];
      if (USE_STATEFBO_SPLIT){
        ShadeCommand& sc = m_shades[SHADE_SOLIDWIRE_SPLIT];
        for (size_t i = 0; i < sc.sizes.size(); i++){
          if (sc.states[i] == m_stateObjects[STATE_LINES]){
            sc.states[i] = m_stateObjects[STATE_LINES_SPLIT];
          }
        }
      }
      else{
        ShadeCommand& sc = m_shades[SHADE_SOLIDWIRE_SPLIT];
        for (size_t i = 0; i < sc.sizes.size(); i++)
        {
          if (sc.states[i] == m_stateObjects[STATE_LINES]){
            sc.fbos[i] = resources.fbo2;
          }
          else{
            sc.fbos[i] = resources.fbo;
          }
        }
      }
    }

    glCreateBuffers(NUM_SHADES,m_tokenBuffers);
    if (m_hwsupport && fillBuffers){
      for (int i = 0; i < NUM_SHADES; i++){
        glNamedBufferStorage(m_tokenBuffers[i],m_tokenStreams[i].size(), &m_tokenStreams[i][0], 0);
        if (m_useaddress){
          glGetNamedBufferParameterui64vNV(m_tokenBuffers[i], GL_BUFFER_GPU_ADDRESS_NV, &m_tokenAddresses[i]);
          glMakeNamedBufferResidentNV(m_tokenBuffers[i], GL_READ_ONLY);

          ShadeCommand& sc = m_shades[i];
          sc.addresses.clear();
          sc.addresses.reserve( sc.offsets.size() );
          for (size_t n = 0; n < sc.offsets.size(); n++){
            sc.addresses.push_back( m_tokenAddresses[i] + sc.offsets[n] );
          }
        }
      }
    }
  }

  void TokenRendererBase::deinit()
  {
    if (m_useaddress){
      for (int i = 0; i < NUM_SHADES; i++){
        if (m_tokenAddresses[i]){
          glMakeNamedBufferNonResidentNV( m_tokenBuffers[i] );
        }
      }
    }

    glDeleteBuffers(NUM_SHADES,m_tokenBuffers);

    if (m_hwsupport){
      glDeleteStatesNV(NUM_STATES,m_stateObjects);
      if (m_uselist){
        glDeleteCommandListsNV(NUM_SHADES,m_commandLists);
      }
    }
    else {
#if !defined (NDEBUG)
      if (m_bindlessVboUbo){
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
      }
#endif
    }

    m_stateSystem.deinit();
  }


  void TokenRendererBase::captureState( const Resources &resources )
  {
    bool stateChanged  = m_stateChangeID != resources.stateChangeID;
    bool fboTexChanged = m_fboStateChangeID != resources.fboTextureChangeID;

    m_stateChangeID = resources.stateChangeID;
    m_fboStateChangeID = resources.fboTextureChangeID;

    if (stateChanged){
      StateSystem::State state;
      state.verteximm.data[VERTEX_WIREMODE].mode = StateSystem::VERTEXMODE_INT; // need to set this properly


      if (m_bindlessVboUbo){
        // temp workaround
#if USE_RESETADDRESSES
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV,0,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_MATERIAL,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_MATRIX,0,0);
        glBufferAddressRangeNV(GL_UNIFORM_BUFFER_ADDRESS_NV,UBO_SCENE,0,0);
#endif
      }

      // we will do a series of state captures
      glBindFramebuffer(GL_FRAMEBUFFER, resources.fbo);
      glUseProgram(resources.programUsed);

      SetWireMode(GL_FALSE);

      if (m_hwsupport){
        glStateCaptureNV(m_stateObjects[STATE_TRIS],GL_TRIANGLES);
      }
      else {
        state.getGL(); // very costly, smarter would be setting this manually
        m_stateSystem.set(m_stateIDs[STATE_TRIS], state, GL_TRIANGLES);
      }

      glEnable(GL_POLYGON_OFFSET_FILL);
      // glPolygonOffset(1,1); //not captured


      if (m_hwsupport){
        glStateCaptureNV(m_stateObjects[STATE_TRISOFFSET],GL_TRIANGLES);
      }
      else {
        state.getGL(); // very costly, smarter would be setting this manually
        m_stateSystem.set(m_stateIDs[STATE_TRISOFFSET], state, GL_TRIANGLES);
      }

      SetWireMode(GL_TRUE);

      if (m_hwsupport){
        glStateCaptureNV(m_stateObjects[STATE_LINES],GL_LINES);
      }
      else {
        state.getGL(); // very costly, smarter would be setting this manually
        m_stateSystem.set(m_stateIDs[STATE_LINES], state, GL_LINES);
      }

      glBindFramebuffer(GL_FRAMEBUFFER, resources.fbo2);

      if (m_hwsupport){
        glStateCaptureNV(m_stateObjects[STATE_LINES_SPLIT], GL_LINES);
      }
      else {
        state.getGL(); // very costly, smarter would be setting this manually
        m_stateSystem.set(m_stateIDs[STATE_LINES_SPLIT], state, GL_LINES);
      }

      if (!m_hwsupport){
        m_stateSystem.prepareTransition(m_stateIDs[STATE_TRISOFFSET], m_stateObjects[STATE_LINES]);
        m_stateSystem.prepareTransition(m_stateIDs[STATE_LINES],      m_stateObjects[STATE_TRISOFFSET]);
        m_stateSystem.prepareTransition(m_stateIDs[STATE_TRISOFFSET], m_stateObjects[STATE_LINES_SPLIT]);
        m_stateSystem.prepareTransition(m_stateIDs[STATE_LINES_SPLIT],m_stateObjects[STATE_TRISOFFSET]);
      }

      // reset, stored in stateobjects
      glUseProgram(0);
      glDisable(GL_POLYGON_OFFSET_FILL);
      glPolygonOffset(0,0); 
#if 1
      // workaround
      glBindFramebuffer(GL_FRAMEBUFFER, resources.fbo);
#else
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif
    }

    if (m_hwsupport && m_uselist && (stateChanged || fboTexChanged)){
      for (int i = 0; i < NUM_SHADES; i++){
        ShadeCommand& shade = m_shades[i];

        std::vector<const void*>  ptrs;
        ptrs.reserve(shade.offsets.size());
        for (size_t p = 0; p < shade.offsets.size(); p++){
          ptrs.push_back(&m_tokenStreams[i][shade.offsets[p]]);
        }

        glCommandListSegmentsNV(m_commandLists[i],1);
        glListDrawCommandsStatesClientNV(m_commandLists[i],0, &ptrs[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], int(shade.states.size()) );
        glCompileCommandListNV(m_commandLists[i]);
      }
    }
  }

  void TokenRendererBase::renderShadeCommandSW( const void* NV_RESTRICT stream, size_t streamSize, ShadeCommand &shade )
  {
    nvtokenDrawCommandsStatesSW(stream, streamSize, &shade.offsets[0], &shade.sizes[0], &shade.states[0], &shade.fbos[0], GLuint(shade.states.size()), m_stateSystem);
  }

}
