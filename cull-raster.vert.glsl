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

#ifndef MATRIX_WORLD
#define MATRIX_WORLD    0
#endif

#ifndef MATRIX_WORLD_IT
#define MATRIX_WORLD_IT 1
#endif

#ifndef MATRICES
#define MATRICES        2
#endif

layout(std430,binding=0) buffer visibleBuffer {
  int visibles[];
};

uniform samplerBuffer matricesTex;

#ifdef DUALINDEX
layout(location=0) in int  bboxIndex;
layout(location=2) in int  matrixIndex;
uniform samplerBuffer     bboxesTex;

vec4 bboxMin = texelFetch(bboxesTex, bboxIndex*2+0);
vec4 bboxMax = texelFetch(bboxesTex, bboxIndex*2+1);
#else
layout(location=0) in vec4 bboxMin;
layout(location=1) in vec4 bboxMax;
layout(location=2) in int  matrixIndex;
#endif

uniform vec3 viewPos;

out VertexOut{
  vec3 bboxCtr;
  vec3 bboxDim;
  flat int matrixIndex;
  flat int objid;
} OUT;

void main()
{
  int objid = gl_VertexID;
  vec3 ctr =((bboxMin + bboxMax)*0.5).xyz;
  vec3 dim =((bboxMax - bboxMin)*0.5).xyz;
  OUT.bboxCtr = ctr;
  OUT.bboxDim = dim;
  OUT.matrixIndex = matrixIndex;
  OUT.objid = objid;
  
  {
    // if camera is inside the bbox then none of our
    // side faces will be visible, must treat object as 
    // visible
    int matindex = (matrixIndex * MATRICES + MATRIX_WORLD_IT)*4;
    mat4 worldInvTransTM = mat4(
      texelFetch(matricesTex,matindex + 0),
      texelFetch(matricesTex,matindex + 1),
      texelFetch(matricesTex,matindex + 2),
      texelFetch(matricesTex,matindex + 3));
      
    vec3 objPos = (vec4(viewPos,1) * worldInvTransTM).xyz;
    objPos -= ctr;
    if (all(lessThan(abs(objPos),dim))){
      // inside bbox
      visibles[objid] = 1;
    }
  }
}
