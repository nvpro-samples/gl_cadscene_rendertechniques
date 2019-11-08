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

/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#include <assert.h>
#include <algorithm>
#include "renderer.hpp"

#include <nvmath/nvmath_glsltypes.h>

#include "common.h"

#pragma pack(1)


namespace csfviewer
{

  //////////////////////////////////////////////////////////////////////////

  bool Renderer::s_bindless_ubo = false;

  CullingSystem   Renderer::s_cullsys;
  ScanSystem      Renderer::s_scansys;

  const char* toString( enum ShadeType st )
  {
    switch(st){
    case SHADE_SOLID: return "solid";
    case SHADE_SOLIDWIRE: return "solid w edges";
    case SHADE_SOLIDWIRE_SPLIT: return "solid w edges (split)";
    }

    return NULL;
  }


  static void FillCache( std::vector<Renderer::DrawItem>& drawItems, const CadScene::Object& obj, const CadScene::Geometry& geo,  bool solid, int objectIndex ) 
  {
    int begin = 0;
    const CadScene::DrawRangeCache &cache = solid ? obj.cacheSolid : obj.cacheWire;

    for (size_t s = 0; s < cache.state.size(); s++)
    {
      const CadScene::DrawStateInfo &state = cache.state[s];
      for (int d = 0; d < cache.stateCount[s]; d++){
        // evict
        Renderer::DrawItem di;
        di.geometryIndex = obj.geometryIndex;
        di.matrixIndex   = state.matrixIndex;
        di.materialIndex = state.materialIndex;
        di.objectIndex   = objectIndex;

        di.solid = solid;
        di.range.offset = cache.offsets[begin + d];
        di.range.count  = cache.counts [begin + d];

        drawItems.push_back(di);
      }
      begin += cache.stateCount[s];
    }
  }

  static void FillJoin( std::vector<Renderer::DrawItem>& drawItems, const CadScene::Object& obj, const CadScene::Geometry& geo,  bool solid, int objectIndex ) 
  {
    CadScene::DrawRange range;

    int lastMaterial = -1;
    int lastMatrix   = -1;

    for (size_t p = 0; p < obj.parts.size(); p++){
      const CadScene::ObjectPart&   part = obj.parts[p];
      const CadScene::GeometryPart& mesh = geo.parts[p];

      if (!part.active) continue;

      if (part.materialIndex != lastMaterial || part.matrixIndex != lastMatrix){

        if (range.count){
          // evict
          Renderer::DrawItem di;
          di.geometryIndex = obj.geometryIndex;
          di.matrixIndex   = lastMatrix;
          di.materialIndex = lastMaterial;
          di.objectIndex   = objectIndex;

          di.solid = solid;
          di.range = range;

          drawItems.push_back(di);
        }

        range = CadScene::DrawRange();

        lastMaterial = part.materialIndex;
        lastMatrix   = part.matrixIndex;
      }

      if (!range.count){
        range.offset = solid ? mesh.indexSolid.offset : mesh.indexWire.offset;
      }

      range.count += solid ? mesh.indexSolid.count : mesh.indexWire.count;
    }

    // evict
    Renderer::DrawItem di;
    di.geometryIndex = obj.geometryIndex;
    di.matrixIndex   = lastMatrix;
    di.materialIndex = lastMaterial;
    di.objectIndex   = objectIndex;

    di.solid = solid;
    di.range = range;

    drawItems.push_back(di);
  }

  static void FillIndividual( std::vector<Renderer::DrawItem>& drawItems, const CadScene::Object& obj, const CadScene::Geometry& geo, bool solid, int objectIndex ) 
  {
    for (size_t p = 0; p < obj.parts.size(); p++){
      const CadScene::ObjectPart&   part = obj.parts[p];
      const CadScene::GeometryPart& mesh = geo.parts[p];

      if (!part.active) continue;

      Renderer::DrawItem di;
      di.geometryIndex = obj.geometryIndex;
      di.matrixIndex   = part.matrixIndex;
      di.materialIndex = part.materialIndex;
      di.objectIndex   = objectIndex;

      di.solid = solid;
      di.range = solid ? mesh.indexSolid : mesh.indexWire;

      drawItems.push_back(di);
    }
  }


  void Renderer::fillDrawItems( std::vector<DrawItem>& drawItems, size_t from, size_t to, bool solid, bool wire )
  {
    const CadScene* NV_RESTRICT scene = m_scene;
    for (size_t i = from; i < scene->m_objects.size() && i < to; i++){
      const CadScene::Object& obj = scene->m_objects[i];
      const CadScene::Geometry& geo = scene->m_geometry[obj.geometryIndex];

      if (m_strategy == STRATEGY_GROUPS){
        if (solid)  FillCache(drawItems, obj, geo, true,  int(i));
        if (wire)   FillCache(drawItems, obj, geo, false, int(i));
      }
      else if (m_strategy == STRATEGY_JOIN) {
        if (solid)  FillJoin(drawItems, obj, geo, true,  int(i));
        if (wire)   FillJoin(drawItems, obj, geo, false, int(i));
      }
      else if (m_strategy == STRATEGY_INDIVIDUAL){
        if (solid)  FillIndividual(drawItems, obj, geo, true,  int(i));
        if (wire)   FillIndividual(drawItems, obj, geo, false, int(i));
      }
    }
  }

}



