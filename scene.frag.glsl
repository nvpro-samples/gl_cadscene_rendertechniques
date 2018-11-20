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
