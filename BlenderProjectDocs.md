# Blender Project Documentation

This page will explain the blender project used to export levels, and the addon

The main goal is to allow you to simply make a level in blender, and it should export seamlessly to the game engine.

It's mostly an addon, the reason I included the full project is because there is a bit of setup required, certain textures and objects are needed for features like ```Bake Lighting``` and ```Bake GI``` to work

<br>

## The addon

The addon is in the text editor section of the project, click the play button at the top of the text editor to enable the addon

<img width="71" height="70" alt="Screenshot 2025-08-25 010457" src="https://github.com/user-attachments/assets/23b9c435-2e2b-4070-a977-975c01e30fd3" />

From there, the features can be accessed in the ```Object``` tab in the 3D view
<br>
<img width="186" height="91" alt="Screenshot 2025-08-25 010016" src="https://github.com/user-attachments/assets/15363b69-5ad8-4562-aa04-b151b5bdd3d3" />

<img width="207" height="234" alt="Screenshot 2025-08-25 010110" src="https://github.com/user-attachments/assets/419f1938-f7fc-4f46-b830-f02a10bfa6f7" />

The only ones that are required are ```Export Mesh``` and ```Convert Level```, the rest are for convenience

### Export Mesh
Exports the selected object's meshes to .MSH files in the ```models``` folder of ```x64/Release```

### Convert Level
Exports all selected objects to the desired level, which includes copying all used textures over to the level's texture folder

### Bake Lighting
Bakes lighting on all selected objects. This operator takes the longest by far

### Bake GI
This one's badly named, it renders and exports a cubemap

### Unwrap Lightmap
For all selected objects, it adds a second UV map if it doesn't already have it, switches over to it and unwraps with either lightmap pack or smart uv project based on the settings you choose before it does its thing

### Set Object ID
This is a helper operator to set the pass index (which corrosponds to object ID) on all selected objects

### Fix FO2 UVs
Another helper operator, this one is meant for levels imported from FlatOut 2, where for some reason the lightmap is the first set of UVs, this operator switches them around to match my engine

### Set Active UV
Another helper operator, it sets the currently active UV map for all selected objects, it's used to prepare for unwrapping many objects into a single UV map, without having to manually go through each one

### Mass Replace Materials
Replaces all instances of a group of materials with another material. This one is in case you import a level from another game that has many duplicates of the same material (*cough* Spaceport *cough*)

### Find Objects of Material
It goes through each selected objects, and will deselect objects that don't use the given material, so at the end it'll be highlighting all objects that use a certain material

### Test Operator
This is for testing new operators or for one-time operations I randomly need, don't use this one

<br>

## Meshes
There's only a couple things I need to mention here

First, you'll have to apply modifiers before using ```Export Mesh``` or they won't be there.

Secondly, you need a 2nd UV map for the lightmap UVs, if the first UV map works you can just duplicate it

The engine supports meshes containing multiple materials, so you don't need to separate them out

<br>

## Materials
To make things as seamless as possible, the addon will grab textures that are used in the material, so it'll look the same in blender and in-game

To make this work, every material needs to use the Principled BSDF node that spawns when you create the material.
<img width="266" height="145" alt="Screenshot 2025-08-25 012958" src="https://github.com/user-attachments/assets/23ee390c-621b-486a-ac89-ae05bd9b2d2f" />

It checks for a node named 'Principled BSDF', it won't work if the node doesn't have this name

<br>

The type of shader it uses is determined by the settings on the Principled node.

If ```metallic``` > 0.5, it uses the metal shader.

If ```transmission``` > 0.5, it uses the glass shader.

Otherwise, it uses the diffuse shader

Also, if the ```emission``` > 0, then lightmaps will not be included, making objects that use it unlit

<br>

### Custom Shaders
To use a custom shader, change the ```label``` of the Principled node to the name of the material you want to use.

For example, let's say I wanted to make a ```water``` shader, change the label to 'water'

<img width="265" height="170" alt="Screenshot 2025-08-25 013454" src="https://github.com/user-attachments/assets/d2a4def7-8ada-426e-9e1b-f316f54e80fe" />

The engine will look in the ```materials``` folder for a .MAT file of the same name, so make a ```water.mat``` file with these contents:
```
shaders/water.zlsl
shaders/water_vert.spv
shaders/water_pixl.spv

VK_CULL_MODE_BACK_BIT
VK_POLYGON_MODE_FILL

false	# alpha-blending
true		# depth test
false	# depth write
false	# masked (alpha tested in shadow maps)

1		# stencil write mask
```

Each option is on it's own line
- ZLSL Filename

  Path to the zlsl file the pixel/vertex shader originated from, used to determine the layouts required for the pipeline

- Vertex Shader

  Path to the vertex shader used in the material

- Pixel Shader

  Path to the pixel shader used in the material

- Cull Mode

  One of the options in the VkCullModeFlags enum, where you can set if back-faces are culled, front-faces are culled, or none

- Polygon Mode

  Determines how the triangles are drawn, whether they are outlined, or filled. To use anything other than FILL a device feature has to be enabled in the source code

- Alpha Blending

  Whether or not transparency is supported. Opaque materials are much faster to render

- Depth Test

  Whether or not pixels will be culled when they are behind previously drawn objects
  
- Depth Write

  Whether or not the depth of pixels is written to the depth buffer

- Masked

  If true, the base colour texture is used in the shadow maps to discard pixels below the alpha threshold. Opaque materials are faster to render

- Stencil Write Value

  If non-zero, it's the value that gets written to the stencil buffer, so post processing effects can be limited to certain materials
