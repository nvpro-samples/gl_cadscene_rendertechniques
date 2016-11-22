/*-----------------------------------------------------------------------
  Copyright (c) 2011-2016, NVIDIA. All rights reserved.

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

#pragma once


extern "C" {

#ifdef _WIN32
  #if defined(CSFAPI_EXPORTS)
    #define CSFAPI __declspec(dllexport)
  #elif defined(CSFAPI_IMPORTS)
    #define CSFAPI __declspec(dllimport)
  #else
    #define CSFAPI
  #endif
#else
  #define CSFAPI
#endif

  enum {
    CADSCENEFILE_VERSION                = 5,
    CADSCENEFILE_VERSION_COMPAT         = 2,
    CADSCENEFILE_VERSION_META           = 5,

    CADSCENEFILE_NOERROR                = 0,
    CADSCENEFILE_ERROR_NOFILE           = 1,
    CADSCENEFILE_ERROR_VERSION          = 2,
    CADSCENEFILE_ERROR_OPERATION        = 3,

    CADSCENEFILE_FLAG_UNIQUENODES       = 1,
    CADSCENEFILE_FLAG_STRIPS            = 2,
    CADSCENEFILE_FLAG_META_NODE         = 4,
    CADSCENEFILE_FLAG_META_GEOMETRY     = 8,
    CADSCENEFILE_FLAG_META_FILE         = 16,

    CADSCENEFILE_LENGTH_GUID            = 4,
    CADSCENEFILE_LENGTH_STRING          = 128,
  };

  #define CADSCENEFILE_RESTARTINDEX     (~0)

  /*
    version changes:
    1 initial
    2 !binary break
      material allows custom payload
    3 hasUniqueNodes became a bitflag
      added strip indices flag, file is either strip or non-strip
    4 lineWidth changed to nodeIDX, allows per-part sub-transforms. sub-transforms should be below
      object in hierarchy and not affect geometry bbox
    5 meta information handling
  */

  /*
  
  // example structure
  
  CSFMaterials   
  0 Red          
  1 Green        
  2 Blue         
                
                 
  CSFGeometries (index,vertex & "parts")
  0 Box
  1 Cylinder
  e.g. parts  (CSFGeometryPart define a subset of vertices/indices of CSFGeometry):
    0 mantle
    1 top cap
    2 bottom cap

  There is no need to have multiple parts, but for experimenting with
  rendering some raw CAD data, having each patch/surface feature individually
  can be useful.

  CSFNodes  (hierarchy of nodes)

  A node can also reference a geometry, this way the same geometry
  data can be instanced multiple times.
  If the node references geometry, then it must have an
  array of "CSFNodePart" matching refernced CSFGeometryParts.
  The CSFNodePart encodes the materials/matrix as well as it's
  "visibility" (active) state.

  */


  typedef unsigned long long CSFoffset;
  typedef unsigned int CSFGuid[CADSCENEFILE_LENGTH_GUID];

  // optional, if one wants to pack
  // additional meta information into the bytes arrays
  typedef struct _CSFBytePacket {
    CSFGuid               guid;
    int                   numBytes; // includes size of this header
  }CSFBytePacket;

  typedef struct _CSFMeta {
    char    name[CADSCENEFILE_LENGTH_STRING];
    int     flags;
    CSFoffset             numBytes;
    union {
      CSFoffset           bytesOFFSET;
      unsigned char*      bytes;
    };
  }CSFMeta;

  // FIXME should move meta outside material, but breaks binary
  // compatibility
  typedef struct _CSFMaterial {
    char    name[CADSCENEFILE_LENGTH_STRING];
    float   color[4];
    int     type;         // arbitrary data
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
    float                       _deprecated[16];
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
    int                   nodeIDX;  // -1 if own node's transform, 
                                    // otherwise nodeIDX should be child of own node
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
    //----------------------------------
    // only available for version >= CADSCENEFILE_VERSION_META and if flag is set
    // use functions to access for safety check
    
    union {
      CSFoffset           nodeMetasOFFSET;
      CSFMeta*            nodeMetas; // one per node
    };
    union {
      CSFoffset           geometryMetasOFFSET;
      CSFMeta*            geometryMetas; // one per geometry
    };
    union {
      CSFoffset           fileMetaOFFSET;
      CSFMeta*            fileMeta; // only one per file
    };
    //----------------------------------
  }CSFile;

  typedef struct CSFileMemory_s* CSFileMemoryPTR;

  CSFAPI CSFileMemoryPTR CSFileMemory_new();
  CSFAPI void*  CSFileMemory_alloc(CSFileMemoryPTR mem, size_t sz, const void*fill);
  CSFAPI void   CSFileMemory_delete(CSFileMemoryPTR mem);

  CSFAPI int    CSFile_loadRaw (CSFile** outcsf, size_t sz, void* data);
  CSFAPI int    CSFile_load    (CSFile** outcsf, const char* filename, CSFileMemoryPTR mem);
  CSFAPI int    CSFile_save    (const CSFile* csf, const char* filename);

  // safer to use these
  CSFAPI const  CSFMeta*          CSFile_getNodeMetas(const CSFile* csf);
  CSFAPI const  CSFMeta*          CSFile_getGeometryMetas(const CSFile* csf);
  CSFAPI const  CSFMeta*          CSFile_getFileMeta(const CSFile* csf);
  CSFAPI const  CSFBytePacket*    CSFile_getBytePacket(const unsigned char* bytes, CSFoffset numBytes, CSFGuid guid);

  CSFAPI int    CSFile_transform(CSFile *csf);  // requires unique nodes

#if CSF_ZIP_SUPPORT
  CSFAPI int    CSFile_loadExt(CSFile** outcsf, const char* filename, CSFileMemoryPTR mem);
  CSFAPI int    CSFile_saveExt(CSFile* csf, const char* filename);
#endif

};

