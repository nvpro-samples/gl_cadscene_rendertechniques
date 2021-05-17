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



#version 330
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

#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_shader_storage_buffer_object : enable


//#define OCCLUSION

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

#if GL_ARB_shader_storage_buffer_object
layout(std430,binding=0)  writeonly buffer outputBuffer {
  int outstream[];
};

void storeOutput(int value)
{
  outstream[gl_VertexID] = value;
}

#else
flat out int outstream;

void storeOutput(int value)
{
  outstream = value;
}
#endif

uniform mat4              viewProjTM;
uniform samplerBuffer     matricesTex;

#ifdef OCCLUSION
uniform sampler2D         depthTex;
#endif

vec4 getBoxCorner(int n)
{
#if 1
  bvec3 useMax = bvec3((n & 1) != 0, (n & 2) != 0, (n & 4) != 0);
  return vec4(mix(bboxMin.xyz, bboxMax.xyz, useMax),1);
#else
  switch(n){
  case 0:
    return vec4(bboxMin.x,bboxMin.y,bboxMin.z,1);
  case 1:
    return vec4(bboxMax.x,bboxMin.y,bboxMin.z,1);
  case 2:
    return vec4(bboxMin.x,bboxMax.y,bboxMin.z,1);
  case 3:
    return vec4(bboxMax.x,bboxMax.y,bboxMin.z,1);
  case 4:
    return vec4(bboxMin.x,bboxMin.y,bboxMax.z,1);
  case 5:
    return vec4(bboxMax.x,bboxMin.y,bboxMax.z,1);
  case 6:
    return vec4(bboxMin.x,bboxMax.y,bboxMax.z,1);
  case 7:
    return vec4(bboxMax.x,bboxMax.y,bboxMax.z,1);
  }
#endif
}

vec3 projected(mat4 a, vec4 pos) {
  vec4 hpos = (a * pos);
  return hpos.xyz/hpos.w;
}

void main (){
  int isvisible = 0;
  int matindex = (matrixIndex*MATRICES + MATRIX_WORLD)*4;
  mat4 worldTM = mat4(
    texelFetch(matricesTex,matindex + 0),
    texelFetch(matricesTex,matindex + 1),
    texelFetch(matricesTex,matindex + 2),
    texelFetch(matricesTex,matindex + 3));
    
  mat4 worldViewProjTM = (viewProjTM * worldTM);
  
  // clipspace bbox
  vec3 clipmin  = projected(worldViewProjTM, getBoxCorner(0));
  vec3 clipmax  = clipmin;

  for (int n = 1; n < 8; n++){
    vec3 ab = projected(worldViewProjTM, getBoxCorner(n));
    clipmin = min(clipmin,ab);
    clipmax = max(clipmax,ab);
  }

  isvisible = (
    clipmin.x <= 1 &&
    clipmin.y <= 1 &&
    clipmin.z <= 1 &&
    clipmax.x >= -1 &&
    clipmax.y >= -1 &&
    clipmax.z >= -1) ? 1 : 0;

#ifdef OCCLUSION
  if (isvisible != 0){
    clipmin = clipmin * 0.5 + 0.5;
    clipmax = clipmax * 0.5 + 0.5;
    vec2 size = (clipmax.xy - clipmin.xy);
    ivec2 texsize = textureSize(depthTex,0);
    float maxsize = max(size.x, size.y) * float(max(texsize.x,texsize.y));
    float miplevel = ceil(log2(maxsize));
    
    float depth = 0;
    float a = textureLod(depthTex,clipmin.xy,miplevel).r;
    float b = textureLod(depthTex,vec2(clipmax.x,clipmin.y),miplevel).r;
    float c = textureLod(depthTex,clipmax.xy,miplevel).r;
    float d = textureLod(depthTex,vec2(clipmin.x,clipmax.y),miplevel).r;
    depth = max(depth,max(max(max(a,b),c),d));

    isvisible =  clipmin.z <= depth ? 1 : 0;
  }
#endif

  storeOutput(isvisible);
}
