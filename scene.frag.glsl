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

#extension GL_ARB_shading_language_include : enable
#include "common.h"

// must match cadscene
struct Side {
  vec4 ambient;
  vec4 diffuse;
  vec4 specular;
  vec4 emissive;
};

struct Material {
  Side  sides[2];
  Side  _pad[2];
};

layout(std140,binding=UBO_MATERIAL) uniform materialBuffer {
#if USE_INDEXING
  Material  materials[256];
#else
  Material  materials[1];
#endif
};


in Interpolants {
  vec3 wPos;
  vec3 wNormal;
#if USE_INDEXING
  flat ivec2 assigns;
#endif
#if !defined(WIREMODE)
  flat int wireMode;
#endif
} IN;


#if !defined(WIREMODE)
int wireMode = IN.wireMode;
#else
int wireMode = WIREMODE;
#endif

layout(location=0,index=0) out vec4 out_Color;

vec4 shade(const Side side)
{
  vec4 color = side.ambient + side.emissive;
  
  vec3 eyePos = vec3(scene.viewMatrixIT[0].w,scene.viewMatrixIT[1].w,scene.viewMatrixIT[2].w);

  vec3 lightDir = normalize( scene.wLightPos.xyz - IN.wPos);
  vec3 viewDir  = normalize( eyePos - IN.wPos);
  vec3 halfDir  = normalize(lightDir + viewDir);
  vec3 normal   = normalize(IN.wNormal) * (gl_FrontFacing ? 1 : -1);
  
  color += side.diffuse * max(dot(normal,lightDir),0);
  color += side.specular * pow(max(0,dot(normal,halfDir)),16);
  
  return color;
}

void main()
{
  int mi = 0;
#if USE_INDEXING
  mi = IN.assigns.y;
#endif

  out_Color = shade(materials[mi].sides[gl_FrontFacing ? 1 : 0]);

  if (wireMode != 0){
    out_Color = materials[mi].sides[0].diffuse*1.5 + 0.3;
  }
}
