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

#define MATRIX_BASE         0
#define MATRIX_INVTRANS     1

#define MATRIX_BEGIN_WORLD  0
#define MATRIX_BEGIN_OBJECT 2
#define MATRICES            4

layout(location=0) uniform float scale;

#if USE_COMPUTE

  layout (local_size_x = 256) in;

  layout(location=1) uniform int count;

  #define BAILOUT gl_GlobalInvocationID.x >= count
  int self = int(gl_GlobalInvocationID.x);

#else

  #define BAILOUT false
  int self = int(gl_VertexID);

#endif

layout(std430,binding=0) restrict buffer matricesBuffer {
  mat4 matrices[];
};

layout(binding=0) uniform samplerBuffer texMatricesOrig;
mat4 getMatrix(samplerBuffer texbuffer, int idx)
{
  return mat4(texelFetch(texbuffer,idx*4 + 0),
              texelFetch(texbuffer,idx*4 + 1),
              texelFetch(texbuffer,idx*4 + 2),
              texelFetch(texbuffer,idx*4 + 3));
}

mat4 getObjectMatrixOrig(int idx, int what){
  return getMatrix(texMatricesOrig,idx*MATRICES + what + MATRIX_BEGIN_OBJECT);
};

mat4 getWorldMatrixOrig(int idx, int what){
  return getMatrix(texMatricesOrig,idx*MATRICES + what + MATRIX_BEGIN_WORLD);
};

void main()
{
  if (BAILOUT){
    return;
  }
  
  mat4 matrixOrig     = getObjectMatrixOrig(self,MATRIX_BASE);
  mat4 matrixITOrig   = getObjectMatrixOrig(self,MATRIX_INVTRANS);
  
#if 0
  // compiler bug
  mat4 matrixBase = matrixOrig;
  mat4 matrixIT   = matrixITOrig;
  matrixBase[3].xyz *= scale;
  matrixIT[0].w /= scale;
  matrixIT[1].w /= scale;
  matrixIT[2].w /= scale;
#else
  vec4 basescale  = vec4(scale,scale,scale,1);
  vec4 itscale    = vec4(1,1,1,1/scale);
  mat4 matrixBase = mat4(matrixOrig[0], matrixOrig[1], matrixOrig[2], matrixOrig[3]*basescale);
  mat4 matrixIT   = mat4(matrixITOrig[0]*itscale,matrixITOrig[1]*itscale,matrixITOrig[2]*itscale,matrixITOrig[3]);
#endif

  matrices[self*MATRICES + MATRIX_BEGIN_OBJECT + MATRIX_BASE]     = matrixBase;
  matrices[self*MATRICES + MATRIX_BEGIN_OBJECT + MATRIX_INVTRANS] = matrixIT;
}
