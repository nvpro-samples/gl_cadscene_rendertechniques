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

#if USE_INDEXING && USE_BASEINSTANCE
#extension GL_ARB_shader_draw_parameters : require
#endif
in layout(location=VERTEX_POS)      vec3 pos;
in layout(location=VERTEX_NORMAL)   vec3 normal;

#if USE_INDEXING
#if USE_BASEINSTANCE
ivec2 assigns = ivec2( gl_BaseInstanceARB & 0xFFFFF, gl_BaseInstanceARB >> 20);
#else
in layout(location=VERTEX_ASSIGNS)  ivec2 assigns;
#endif
#define matrixIndex assigns.x
#endif

#if !defined(WIREMODE)
in layout(location=VERTEX_WIREMODE) int wireMode;
#endif

out Interpolants {
  vec3 wPos;
  vec3 wNormal;
#if USE_INDEXING
  flat ivec2 assigns;
#endif
#if !defined(WIREMODE)
  flat int wireMode;
#endif
} OUT;



void main()
{
#if USE_INDEXING || USE_MIX
  vec3 wPos     = (getIndexedMatrix(matrixIndex, NODE_MATRIX_WORLD)   * vec4(pos,1)).xyz;
  vec3 wNormal  = mat3(getIndexedMatrix(matrixIndex, NODE_MATRIX_WORLDIT)) * normal;
#else
  vec3 wPos     = (object.worldMatrix   * vec4(pos,1)).xyz;
  vec3 wNormal  = mat3(object.worldMatrixIT) * normal;
#endif
  gl_Position   = scene.viewProjMatrix * vec4(wPos,1);
  OUT.wPos = wPos;
  OUT.wNormal = wNormal;
#if USE_INDEXING
  OUT.assigns = assigns;
#endif
#if !defined(WIREMODE)
  OUT.wireMode = wireMode;
#endif
}
