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



#version 430
/**/

#ifndef USE_COMPUTE
#define USE_COMPUTE 1
#endif

#define LEVELBITS 8

#define MATRIX_BASE         0
#define MATRIX_INVTRANS     1

#define MATRIX_BEGIN_WORLD  0
#define MATRIX_BEGIN_OBJECT 2
#define MATRICES            4

#if USE_COMPUTE

  layout (local_size_x = 256) in;

  layout(std430,binding=2) buffer scratchBuffer {
    int nodes[];
  };

  layout(location=0) uniform int count;

  #define BAILOUT gl_GlobalInvocationID.x >= count
  int self = nodes[gl_GlobalInvocationID.x];

#else

  #define BAILOUT false
  layout(location=0) in int self;

#endif

layout(binding=0) uniform isamplerBuffer parentsBuffer;

layout(std430,binding=0) restrict buffer worldMatricesBuffer {
  mat4 worldMatrices[];
};

layout(binding=1) uniform samplerBuffer texWorldMatrices;
layout(binding=2) uniform samplerBuffer texObjectMatrices;

mat4 getMatrix(samplerBuffer texbuffer, int idx)
{
  return mat4(texelFetch(texbuffer,idx*4 + 0),
              texelFetch(texbuffer,idx*4 + 1),
              texelFetch(texbuffer,idx*4 + 2),
              texelFetch(texbuffer,idx*4 + 3));
}

mat4 getObjectMatrix(int idx, int what){
  return getMatrix(texObjectMatrices,idx*MATRICES + what + MATRIX_BEGIN_OBJECT);
};

mat4 getWorldMatrix(int idx, int what){
  return getMatrix(texWorldMatrices,idx*MATRICES + what + MATRIX_BEGIN_WORLD);
};


void main()
{
  if (BAILOUT){
    return;
  }

  int parent = texelFetch(parentsBuffer,self).x >> LEVELBITS;
  
  // world base matrix
  mat4 world = 
    getWorldMatrix(parent,MATRIX_BASE) *
    getObjectMatrix(self,MATRIX_BASE);


#if 0
  // world inv trans matrix
  mat4 parentInv = transpose(getWorldMatrix(parent,MATRIX_INVTRANS));
  mat4 objectInv = transpose(getObjectMatrix(self, MATRIX_INVTRANS));

  mat4 worldInv  = objectInv * parentInv;
#else
  mat4 worldInv = inverse(world);
#endif

  worldMatrices[self*MATRICES + MATRIX_BEGIN_WORLD + MATRIX_BASE]     = world;
  worldMatrices[self*MATRICES + MATRIX_BEGIN_WORLD + MATRIX_INVTRANS] = transpose(worldInv);
}
