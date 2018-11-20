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
