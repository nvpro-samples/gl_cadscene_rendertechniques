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



#define VERTEX_POS      0
#define VERTEX_NORMAL   1
#define VERTEX_ASSIGNS  2
#define VERTEX_WIREMODE 3

#define UBO_SCENE     0
#define UBO_MATRIX    1
#define UBO_MATERIAL  2

#define TEX_MATRICES  0

#define USE_BASEINSTANCE  0

//#define UNI_WIREFRAME 0


#ifdef __cplusplus
namespace csfviewer
{
  using namespace nvmath;
#endif

struct SceneData {
  mat4  viewProjMatrix;
  mat4  viewMatrix;
  mat4  viewMatrixIT;

  vec4  viewPos;
  vec4  viewDir;
  
  vec4  wLightPos;
  
  ivec2 viewport;
  uvec2 tboMatrices;
};

#ifdef __cplusplus
}
#endif


#if defined(GL_core_profile) || defined(GL_compatibility_profile) || defined(GL_es_profile)

#extension GL_NV_command_list : enable
#if GL_NV_command_list
layout(commandBindableNV) uniform;
#endif

// prevent this to be used by c++

layout(std140,binding=UBO_SCENE) uniform sceneBuffer {
  SceneData   scene;
};

// must match cadscene!
layout(std140,binding=UBO_MATRIX) uniform matrixBuffer {
  mat4 worldMatrix;
  mat4 worldMatrixIT;
  mat4 objectMatrix;
  mat4 objectMatrixIT;
} object;

#extension GL_ARB_bindless_texture : enable
#extension GL_NV_bindless_texture : enable
#if GL_NV_bindless_texture
#define matricesBuffer  samplerBuffer(scene.tboMatrices)
#else
layout(binding=TEX_MATRICES) uniform samplerBuffer matricesBuffer;
#endif
// must match cadscene!
#define NODE_MATRIX_WORLD     0
#define NODE_MATRIX_WORLDIT   1
#define NODE_MATRIX_OBJECT    2
#define NODE_MATRIX_OBJECTIT  3
#define NODE_MATRICES         4

mat4 getIndexedMatrix(int idx, int what)
{
  int i = idx * NODE_MATRICES + what;
  return mat4(  texelFetch(matricesBuffer, i*4 + 0),
                texelFetch(matricesBuffer, i*4 + 1),
                texelFetch(matricesBuffer, i*4 + 2),
                texelFetch(matricesBuffer, i*4 + 3));
}

#endif