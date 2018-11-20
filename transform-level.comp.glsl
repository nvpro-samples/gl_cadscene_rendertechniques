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
