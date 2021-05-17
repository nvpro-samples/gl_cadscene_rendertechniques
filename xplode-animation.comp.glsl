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
