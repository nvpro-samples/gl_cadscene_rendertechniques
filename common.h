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