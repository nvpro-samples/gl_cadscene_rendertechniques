/*-----------------------------------------------------------------------
  Copyright (c) 2014, NVIDIA. All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
   * Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
   * Neither the name of its contributors may be used to endorse 
     or promote products derived from this software without specific
     prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------*/
/* Contact ckubisch@nvidia.com (Christoph Kubisch) for feedback */

#pragma once


extern "C" {

#define CSFAPI __declspec(dllexport)

  enum {
    CADSCENEFILE_VERSION = 4,
    CADSCENEFILE_VERSION_COMPAT = 2,

    CADSCENEFILE_NOERROR = 0,
    CADSCENEFILE_ERROR_NOFILE = 1,
    CADSCENEFILE_ERROR_VERSION = 2,
    CADSCENEFILE_ERROR_OPERATION = 3,

    CADSCENEFILE_FLAG_UNIQUENODES = 1,
    CADSCENEFILE_FLAG_STRIPS      = 2,

    CADSCENEFILE_RESTARTINDEX = -1,
  };

  /*
    version changes:
    1 initial
    2 !binary break
      material allows custom payload
    3 hasUniqueNodes became a bitflag
      added strip indices flag, file is either strip or non-strip
    4 lineWidth changed to nodeIDX, allows per-part sub-transforms. sub-transforms should be below
      object in hierarchy and not affect geometry bbox
  */


  typedef unsigned long long CSFoffset;

  typedef struct _CSFMaterial {
    char    name[128];
    float   color[4];
    int     type;
    int     numBytes;
    union {
      CSFoffset           bytesOFFSET;
      unsigned char*      bytes;
    };
  }CSFMaterial;

  typedef struct _CSFGeometryPart {
    int     vertex;
    int     indexSolid;
    int     indexWire;
    // fixme: should add offsets, and strip/nostrip, vertexSolid/vertexWire for non-indexed, or drop vertex
  }CSFGeometryPart;

  typedef struct _CSFGeometry{
    float                       matrix[16];
    int                         numParts;
    int                         numVertices;
    int                         numIndexSolid;
    int                         numIndexWire;

    union {
      CSFoffset                 vertexOFFSET;
      float*                    vertex;
    };

    union {
      CSFoffset                 normalOFFSET;
      float*                    normal;
    };

    union {
      CSFoffset                 texOFFSET;
      float*                    tex;
    };

    union {
      CSFoffset                 indexSolidOFFSET;
      unsigned int*             indexSolid;
    };

    union {
      CSFoffset                 indexWireOFFSET;
      unsigned int*             indexWire;
    };

    union {
      CSFoffset                 partsOFFSET;
      CSFGeometryPart*          parts;
    };
  }CSFGeometry;

  typedef struct _CSFNodePart {
    int                   active;
    int                   materialIDX;
    int                   nodeIDX;  // -1 if own transform
  }CSFNodePart;

  typedef struct _CSFNode{
    float                 objectTM[16];
    float                 worldTM[16];
    int                   geometryIDX;
    int                   numParts;
    int                   numChildren;
    union {
      CSFoffset           partsOFFSET;
      CSFNodePart*        parts;
    };
    union {
      CSFoffset           childrenOFFSET;
      int*                children;
    };
  }CSFNode;


  typedef struct _CSFile {
    int                   magic;
    int                   version;
    unsigned int          fileFlags;
    int                   numPointers;
    int                   numGeometries;
    int                   numMaterials;
    int                   numNodes;
    int                   rootIDX;

    union {
      CSFoffset           pointersOFFSET;
      CSFoffset*          pointers;
    };

    union {
      CSFoffset           geometriesOFFSET;
      CSFGeometry*        geometries;
    };

    union {
      CSFoffset           materialsOFFSET;
      CSFMaterial*        materials;
    };

    union {
      CSFoffset           nodesOFFSET;
      CSFNode*            nodes;
    };
  }CSFile;

  typedef struct CSFileMemory_s* CSFileMemoryPTR;

  CSFAPI CSFileMemoryPTR CSFileMemory_new();
  CSFAPI void*  CSFileMemory_alloc(CSFileMemoryPTR mem, size_t sz, const void*fill);
  CSFAPI void   CSFileMemory_delete(CSFileMemoryPTR mem);

  CSFAPI int    CSFile_loadRaw (CSFile** outcsf, size_t sz, void* data);
  CSFAPI int    CSFile_load    (CSFile** outcsf, const char* filename, CSFileMemoryPTR mem);
  CSFAPI int    CSFile_save    (const CSFile* csf, const char* filename);

  CSFAPI int    CSFile_transform(CSFile *csf);  // requires unique nodes

#if CSF_ZIP_SUPPORT
  CSFAPI int    CSFile_loadExt(CSFile** outcsf, const char* filename, CSFileMemoryPTR mem);
  CSFAPI int    CSFile_saveExt(CSFile* csf, const char* filename);
#endif

};

