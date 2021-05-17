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

#define TEMPORAL_LAST 1
#define TEMPORAL_NEW  2

#ifndef TEMPORAL
#define TEMPORAL 0
#endif

#extension GL_ARB_explicit_attrib_location : require
#extension GL_ARB_shader_storage_buffer_object : enable

layout(location=0) in uvec4 instream[8];

#if TEMPORAL
layout(location=9) in uint last;
#endif

#if GL_ARB_shader_storage_buffer_object
layout(std430,binding=0)  writeonly buffer outputBuffer {
  uint outstream[];
};

void storeOutput(uint value)
{
  outstream[gl_VertexID] = value;
}

#else
flat out uint outstream;

void storeOutput(uint value)
{
  outstream= value;
}
#endif

void main ()
{
  uint bits = 0u;
  int outbit = 0;
  for (int i = 0; i < 8; i++){
    for (int n = 0; n < 4; n++, outbit++){
      uint checkbytes = instream[i][n];
      bits |= (checkbytes & 1u) << outbit;
    }
  }
  
#if TEMPORAL == TEMPORAL_LAST
  // render what was visible in last frame and passes current test
  bits &= last;
#elif TEMPORAL == TEMPORAL_NEW
  // render what was not visible in last frame (already rendered), but is now visible
  bits &= (~last);
#endif

  storeOutput(bits);
}
