# gl cadscene render techniques

This sample implements several scene rendering techniques, that target mostly static data such as often found in CAD or DCC applications. In this context static means that the vertex and index buffers for the scene's objects hardly change. It is still fine to edit the geometry of a few objects of the scene, but foremost the matrix and material values would be modified across frames. Imagine making edits to the wheel topology of a car, or positioning an engine, that means the rest of the assembly is not modified.
The principle OpenGL mechanisms hat are used here are described in the presentation slides of [SIGGRAPH 2014](http://on-demand.gputechconf.com/siggraph/2014/presentation/SG4117-OpenGL-Scene-Rendering-Techniques.pdf). It is highly recommended to go through the slides first.

The sample makes use of multiple OpenGL 4 core features, such as **ARB_multi_draw_indirect**, but also showcases OpenGL 3 style rendering techniques.

There is also several techniques built around the **NV_command_list** extension. Please refer to [gl commandlist basic](https://github.com/nvpro-samples/gl_commandlist_basic) for an introduction on NV_command_list.

> Note: This is just a sample to illustrates several techniques and possibilities how to approach rendering, its purpose is not to provide production level, highly optimized implementations.

### Scene Setup

The sample loads a cadscene file (csf), which is a inspired by CAD applications' data organization, just that for simplicity everything is stored in a single RAW file.

The Scene is organized in:

 * Matrices: object transforms as well as concatenated world matrices 
 * TreeNodes: a tree consisting hierarchical information, mapping to Matrix indices

 * Materials: just classic two-sided OpenGL phong material parameters
 * Geometries: storing vertex and index information, and organized in
  * GeometryParts, which reference a sub-range within index buffer, for either "wireframe" or "solid" surfaces

 * Objects, that reference Geometry and have corresponding
  * ObjectParts, that encode Material and Matrix assignment on a part-level. Typically an object uses just one Matrix for all parts.

### Shademodes

![sample screenshot](https://github.com/nvpro-samples/gl_cadscene_rendertechniques/blob/master/doc/sample.jpg)

- **solid**: just triangles are drawn
- **solid with edges**: triangles and edge outlines on top (using polygonoffset to push triangles back). When no global sorting (see later) is performed, it means we toggle between the two modes for every object.
- **solid with edges (split test, only in sorted)**: an artificial mode that also separates triangles and edges into different fbos, and is available in "sorted" and "token" renderers. The implementation has no real use-case character and is more or less for internal benchmarking of fbo toggles.

### Strategies

These influence the number of drawcalls we generate for the hardware and software. Using OpenGL's MultiDraw* functions we can have less software calls than hardware drawcalls, which helps trigger faster paths in the driver as there is less validation overhead. A strategy is applied on a per-object level.

Imagine an object whose parts use two materials, red and blue:

```
material: r b b r
parts:    A B C D
```

- **materialgroups**
Here we create a per-object cache of drawcall ranges for MultiDraw* based on the object's material and matrix assignments. We also "grow" drawcalls if subsequent ranges in the index buffer have the same assignments. Our sample object would be drawn using 2 states one glMultiDrawElements each, which are creating 3 hardware drawcalls: red are ranges A, D and blue is B+C joined together as they are next to each other in the indexbuffer.
- **drawcall join**
As we traverse we combine drawcalls under same state, this means 3 drawcalls for hardware, and 3 for software as well as 3 states: red A, blue B+C, red D.
- **drawcall individual**
We render each piece individually:
red A, blue B, C, red D.

Typically we do all rendering with basic state redundancy filtering so we don't setup a matrix/material change if the same is still active. To keep things simple for state redundancy filtering, you should not go too fine-grained, otherwise all the tracking causes too much memory hopping. In our case we have 3 indices we track: geometry (handles vertex / index buffer setup), material and matrix.

### Renderers
Most renderers will traverse the scene data every frame. The organization of the data is cache friendly foremost, everything is stored in arrays, not too much memory hopping. Some renderers may implement additional caching for rendering.

#### Variants:

 - **bindless**: these variants make use of NVIDIA's bindless extensions NV_vertex_buffer_unified_memory and NV_uniform_buffer_unified_memory, which allows a lower-overhead path in the driver for faster drawcall submission. Classic glBindVertexBuffer or glBindBufferRange are replaced with glBufferAddressRangeNV.
 - **sorted**: indicates we do a global scene sort once, to minimize state changes in subsequent frames.
 - **cullsorted**: next to global sorting by state, we also apply occlusion culling as presented in [end of the slides](http://on-demand.gputechconf.com/siggraph/2014/presentation/SG4117-OpenGL-Scene-Rendering-Techniques.pdf) or in the [gl occlusion culling](https://github.com/nvpro-samples/gl_occlusion_culling) sample.
 - **emulated**: several of the NV_command_list techniques can be run in emulated mode

#### Techniques:

We are mostly looking into accelerating our matrix and material parameter switching performance.

- **uborange**
All matrices and materials are stored in big buffer objects, which allows us to efficiently bind the required sub-range for a drawcall via glBindBufferRange(GL_UNIFORM_BUFFER, usageSlot, buffer, index * itemSize, itemSize). NVIDIA provides optimized paths if you keep the buffer and itemSize for a usageSlot constant for many glBindBufferRange calls. Be aware of GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT which is 256 bytes for most current NVIDIA hardware (Fermi, Kepler, Maxwell).

- **ubosub**
Not as efficient as the above, but maybe appropriate if you cannot afford caching parameter data. We make use of one streaming buffer per usage slot and continously update it via glBufferSubData. NVIDIA's drivers do particularly well if you never bind this buffer as anything but a GL_UNIFORM_BUFFER and keep size and offsets multiple of 4.

- **indexedmdi**
Similar to uborange we make use of all data stored in a bigger buffers in advance. It doesn't make this data "static" you can always update the portions you need, but there is a high chance a lot of data is the same frame to frame. This time we do not bind memory ranges through the OpenGL api, but let the shader do an indirection and only pass the required matrix and material indices. 
For the matrix data we use GL_TEXTURE_BUFFER as it's particularly well for high frequency / potentially divergent access. We typically have far more matrices than materials in our scene. For material data it's a bit "ugly" to use lots of texelFetch instructions decoding all our parameters, it's much easier to write them as structs and store the array either as GL_UNIFORM_BUFFER or GL_SHADER_STORAGE_BUFFER. The latter is only recommended if you have divergent shader access or exceed the 64 KB limit of UBOs.
To pass the indices per-drawcall we make use of GL_ARB_multi_draw_indirect and "instanced" vertex attributes as described at [GTC 2013 on slide 27](http://on-demand.gputechconf.com/gtc/2013/presentations/S3032-Advanced-Scenegraph-Rendering-Pipeline.pdf).
Therefore this renderer requires two additional buffers: one encoding our object's matrix and material index assignments and one encoding the scene's drawcalls as GL_DRAW_INDIRECT_BUFFER. 

A hybrid approach, where the parameter index like "indexedmdi" is used for matrices and uborange bind is used for materials, was not yet implemented, but would be a good compromise.

The following renderers make use of the **NV_command_list** extension. In principle they **behave as "uborange"**, however all buffer bindings and drawcalls are encoded into binary tokens, that are submitted in bulk. In preparation to drawing the appropriate stateobjects are created and reused when rendering (one for lines and for triangles). While stateobject capturing is not crazy expensive, it is still best to cache it across frames.

- **tokenbuffer**
Similar to "indexedmdi" we create a buffer that describes our scene by storing all the relevant token commands. This buffer is filled only once and then later reused.
- **tokenlist**
Instead of storing the tokens inside a buffer we make use of the commandlist object and create and compile one for each shademode for later reuse. Every time our state changes (fbo resizing...) we have to recreate these lists, which makes it less flexible than buffer, but faster when there is lots of statechanges within the list.
- **tokenstream**
This approach does not reuse the tokens across frames, but instead dynamically creates the tokenstream every frame. By default the demo fills and submits tokens in chunks of 256 KB, better values may exist depending on the scene.

### Performance

All timings are preliminary results for *Timer Draw* on a win7-64, i7-860, Quadro K5000 system. 

**Important Note About Timer Query Results:** The GPU time reported below is measured via timer queries, those values however can be skewed by CPU bottlenecks. The "begin" timestamp may be part of a different command submission to the GPU than the "end" timestamp. That means long delay on the CPU side between those submissions will also increase the reported GPU time. That is why in CPU-bottlenecked scenarios with tons of OpenGL commands, the GPU times below are close to the CPU time.

```
scene statistics:
geometries:    110
materials:      66
nodes:        5004
objects:      2497

tokenbuffer/glstream complexities:
type: solid              materialgroups | drawcall individual
commandsize:                     347292 | 1301692
statetoggles:                         1 | 1
tokens:                 
GL_DRAW_ELEMENTS_COMMAND_NV:      11103 |   68452
GL_ELEMENT_ADDRESS_COMMAND_NV:      807 |     807
GL_ATTRIBUTE_ADDRESS_COMMAND_NV:    807 |     807
GL_UNIFORM_ADDRESS_COMMAND_NV:     8988 |   11289
GL_POLYGON_OFFSET_COMMAND_NV:         1 |       1

type: solid w edges
commandsize:                     629644 | 2534412
statetoggles:                      4994 |    4994
tokens:
GL_DRAW_ELEMENTS_COMMAND_NV:      22281 |  136750
GL_ELEMENT_ADDRESS_COMMAND_NV:      807 |     807
GL_ATTRIBUTE_ADDRESS_COMMAND_NV:    807 |     807
GL_UNIFORM_ADDRESS_COMMAND_NV:    15457 |   20036
GL_POLYGON_OFFSET_COMMAND_NV:         1 |       1
```

As one can see from the statistics the key difference is the number of drawcalls for the hardware:
- **materialgroups**: ~ 10 000 drawcalls (inner two columns)
- **drawcall individual**: ~ 70 000 drawcalls (rightmost two columns)

*shademode: solid*

renderer | GPU time | CPU time | GPU time | CPU time (microseconds)
------------ | ------------- | ------------- | ------------- | -------------
**strategy** | **material-** | **-groups** | **drawcall-** | **-individual**
ubosub | 1550 | 1870 |  6000 | 7420
uborange | 1010| 1890 | 3720 | 7660
uborange_bindless | 1010 | 1200 | 2560 | 4900
indexedmdi | 1120 | 1200 | 2080 | 1100
tokenstream | 860 | 300 | 1520 | 1400
tokenbuffer | 780 | <10 | 1230 | <10
tokenlist | 780 | <10 | 880 | <10
tokenbuffer_cullsorted | 540 | 120 | 760 | 120

The results are of course very scene dependent, this model was specfically chosen as it is made of many parts with very few triangles. If the complexity per draw-call was higher (say more triangles or complex shading) then
the CPU impact would be less and we would be GPU-bound. However the CPU time that is "giving back" by faster mechanism can always be used elsewhere. So even if we are GPU-bound, time should not be wasted.

We can see that the "token" techniques do very well and are never CPU-bound, but also the "indexedmdi" technique is quite good. This technique is especially useful for very high frequency parametrs, for example when rendering "id-buffers" for selection, but also matrix indices. For the general use-case working with uborange binds is recommended. 

*shademode: solid with edges*

Unless "sorted", around 5000 toggles are done between triangles/line rendering. The shader
is manipulated through an immediate vertex attribute to toggle between lit/unlit rendering respectively.

renderer | GPU time | CPU time | GPU time | CPU time (microseconds)
------------ | ------------- | ------------- | ------------- | -------------
**strategy** | **material-** | **-groups** | **drawcall-** | **-individual**
ubosub | 2890 | 3350 | 13000 | 15000 | 
uborange | 2150 | 3700 | 12500 | 15200 | 
uborange_bindless | 2150 | 2640 | 8300 | 10000
indexedmdi | 2340 | 2200 | 4050 | 2050
tokenstream | 1860 | 1250 | 3360 | 3200
tokenbuffer | 1750 | 450 | 2650 | 350
tokenlist | 1650 | <10 | 1890 | <10
tokenbuffer_cullsorted | 770 | 120 | 1250 | 120

Compared to the "solid" results, the tokenbuffer and tokenlist techniques show a greater difference in CPU time.


### Model Explosion View

The simple viewer allows you to add animation to the scene and artificially increase scene complexity via "clones".

![xplodeclones](https://github.com/nvpro-samples/gl_cadscene_rendertechniques/blob/master/doc/xplodeclones.jpg)

To "emulate" typical interaction where users might move objects around or have animated scenes, the sample also implements the matrix transform system sketched on [slide 30](http://on-demand.gputechconf.com/siggraph/2014/presentation/SG4117-OpenGL-Scene-Rendering-Techniques.pdf). 

The effect works by first moving all object matrices a bit (*xplode-animation.comp.glsl*), and afterwards the transform hierarchy is updated via a system that is implemented in the *transformsystem.cpp / hpp* files.

The code is not particularly tuned but naively assumes that the upper levels of the hierarchy contain less nodes than lower levels (pyramid). Therefore it uses leaf-processing (which redundantly calculates matrices) over level-wise processing for the first 10 levels, to avoid dependencies (one small compute task waiting for the previous). Later levels are always processed level-wise. A better strategy would be to switch between the two approaches based on actual number of nodes per level. The shaders for this are: *transform-leaves.comp.glsl* and *transform-level.comp.glsl*. 

The hierarchy is managed by the *nodetree.cpp/hpp*, which stores the tree as array of 32bit values. Each value represents a node, and encodes the "level" in the hierarchy in 8 bits and their parent index in the rest of the bits. Which means you can traverse a node up to the root:

``` cpp
// sample traversal of "idx" node to root
self = array[idx];
while( self.level != 0) {
  self = array[self.parent];
}
// self is now the top root for the idx node
```

The nodetree also stores two node index lists for each level: one storing all nodes of a level, and one for all leaves in this level. We feed these two index lists to the appropriate shader. When leave processing is used we append the leaves level-wise, which should minimize divergences within a warp (ideally most threads  have the same number of levels to ascend in the hierarchy).

Many CAD applications tend to use double matrices, the system could be adjusted for this. For rendering, however, float matrices should be used. To account for large translation values, one could run
a concatenation of view-projection (double) and object-world-matrix (double) per-frame and generate the matrices (float) for actual vertex transforms.
To improve memory performance it might be beneficial to use double only for storing translations within the matrices.

> Note: Only the GPU matrices are updated, the CPU techniques such as "ubosub" will not show animations.

### Sample Highlights

This sample is a bit more complex than most others as it contains several subsystems. Don't hesitate to contact the author if something is unclear (commenting was not a priority ;) ).

#### csfviewer.cpp
The principle setup of the sample is in this main file, however most of the interesting bits happen in the renderers.

- Sample::think - prepares the frame and calls the renderer's draw function

#### renderer... and tokenbase...
Each renderer has its own file and is derived from the **Renderer** class in *renderer.hpp*

- Renderer::init - some renderers may allocate extra buffers or create their own data structures for the scene.
- Renderer::deinit 
- Renderer::draw

The renderers may have additional functions, especially the "token" renderers using NV_command_list or "indexedmdi" must create their own scene representation.

#### cadscene...
The "csf" (cadscene file) format is a simple binary format that encodes a scene as its typical for CAD. It closely matches the description at the beginning of the readme. It is not very sophisticated and meant for demo purposes.

> *Note*: The **geforce.csf.gz** assembly binary file that ships with this sample **may NOT be redistributed.**

#### nodetree... and transform...
Implement the matrix hierarchy updates as described in the "model explosion view" section.

#### cull... and scan...
All files related to culling, best is to refer to the [gl occlusion cullling](https://github.com/nvpro-samples/gl_occlusion_cullling) sample, as it leverages the same system and focuses on just that topic. 

*renderertokensortcull.cpp* implements *RendererCullSortToken::CullJobToken::resultFromBits* which contains the details how the occlusion results are handled in this sample. The implementation uses the "raster" "temporal" approach.

#### statesystem... nvtoken... and nvcommandlist...
These files contain helpers when using the NV_command_list extension, please check [gl commandlist basic](https://github.com/nvpro-samples/gl_commandlist_basic) for a smaller sample.

### Building
Ideally clone this and other interesting [nvpro-samples](https://github.com/nvpro-samples) repositories into a common subdirectory. You will always need [shared_sources](https://github.com/nvpro-samples/shared_sources) and on Windows [shared_external](https://github.com/nvpro-samples/shared_external). The shared directories are searched either as subdirectory of the sample or one directory up.

If you are interested in multiple samples, you can use [build_all](https://github.com/nvpro-samples/build_all) CMAKE as entry point, it will also give you options to enable/disable individual samples when creating the solutions.

### Related Samples
[gl commandlist basic](https://github.com/nvpro-samples/gl_commandlist_basic) illustrates the core principle of the NV_command_list extension.
[gl occlusion cullling](https://github.com/nvpro-samples/gl_occlusion_cullling) also uses the occlusion system of this sample, but in a simpler usage scenario.

When using classic scenegraphs, there is typically a lot of overhead in traversing the scene, it is highly recommended to use simpler representations for actual rendering. Flattened hierarchy, arrays... memory friendly data structures, data-oriented design.
If you are still working with a classic scenegraph then [nvpro-pipeline](https://github.com/nvpro-pipeline/pipeline) may provide some acceleration strategies to avoid full scenegraph traversal, of which some are also described in this [GTC 2013 presentation](http://on-demand.gputechconf.com/gtc/2013/presentations/S3032-Advanced-Scenegraph-Rendering-Pipeline.pdf).
