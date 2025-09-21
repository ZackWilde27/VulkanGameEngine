# Blender Project Documentation

This page will explain the blender project used to export levels, and the addon

The main goal is to allow you to simply make a level in blender, and it should export seamlessly to the game engine.

It's mostly an addon, the reason I included the full project is because there is a bit of setup required, certain textures and objects are needed for features like ```Bake Lighting``` and ```Render Cubemap``` to work

I've set it up to pack all textures into the blend file, the only downside is that it's 2GB large (4 if you count the blend1 file)

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

```Unwrap Lightmap``` will do the same thing as the operator of the same name before exporting the mesh

```Skip Existing Meshes``` will skip objects when the mesh file already exists, this is so you can select multiple objects sharing a mesh and it will only export it once

It's overridden when Unwrap Lightmap is true, since it assumes the lightmap has changed and the old file is outdated

<br>

### Convert Level
Exports all selected objects to the desired level, which includes copying all used textures over to the level's texture folder

Check the Levels section below for info on how everything is interpreted

```Level Name``` includes the .lvl extension automatically, so to export to myLevel.lvl, put in myLevel

```Bake Shadow Maps``` will do the Bake Lighting operator on each object as it's exporting them

```Shadow Map Resolution``` is the resolution to bake shadow maps at, only applicable when Bake Shadow Maps is on

```Unwrap Lightmaps``` and ```Export Meshes``` will perform those operators on each object as it's exporting them

<br>

### Bake Lighting
Bakes lighting on all selected objects. This operator takes the longest by far

*Make sure to hide the skybox before baking lighting or the sky won't cast any light on the scene, don't ask how I know that*

```Specific Map Resolution``` is the desired shadow map resolution baked for each object

```Calculate Map Resolution``` will take into account the surface area of the mesh, and the surface area of the UV map to determine how large the shadow map needs to be.

This operator is not very reliable and can generate maps way too large depending on the object, it is limited to a 4K resolution to stop it from going too far

```Level Name``` determines which folder it puts the shadow maps in, under ```levelName/textures```, it has to be the same as the Level Name in the Convert Level operator

<br>

### Render Cubemap
Renders and exports a cubemap

Before using the operator, place the ```CubemapCreator``` camera in a good spot, go into the collection of the same name and select one of the cube faces

The orientation of the camera and names of each side is illogical, but it works so I'm kinda stuck with it

```Is Skybox``` determines whether or not this cubemap is used in the skybox shader, or referenced in object shaders.

The one in the skybox should typically only include the sky or objects that should appear infinitely far away, while the object one should just be a capture from the centre of the scene for the most realism

```Level Name``` is the name of the folder to place the cubemap in, it'll be the same one used in Convert Level

```Resolution``` is the resolution of each face of the cubemap

<br>

### Unwrap Lightmap
For all selected objects, it adds a second UV map if it doesn't already have it, switches over to it and unwraps with either lightmap pack or smart uv project based on the settings you choose before it does its thing

```Use Smart Project``` will unwrap with Smart UV Project if true, Lightmap Pack if false

```All in One``` will unwrap all selected objects onto a single UV map, there's no reason to do this for my engine (yet at least)

<br>

### Set Object ID
This is a helper operator to set the pass index (which corrosponds to object ID) on all selected objects

<br>

### Fix FO2 UVs
Another helper operator, this one is meant for levels imported from FlatOut 2, where for some reason the lightmap is the first set of UVs, this operator switches them around to match my engine

<br>

### Set Active UV
Another helper operator, it sets the currently active UV map for all selected objects, it's used to prepare for unwrapping many objects into a single UV map, without having to manually go through each one

<br>

### Mass Replace Materials
Replaces all instances of a group of materials with another material. This one is in case you import a level from another game that has many duplicates of the same material (*cough* Spaceport *cough*)

```Material(s) to Find``` is a list of names of materials to replace, each separated by commas, so like 'material1,material2,material3'

```Material to Replace``` is the name of the material to replace with

<br>

### Find Objects of Material
It goes through each selected object, and will deselect any that don't use the given material, so at the end it'll be highlighting all objects that use a certain material

```Name of Material to Find``` is exactly that

```Exact Match``` means it will only highlight that object if the material has the exact name, otherwise it'll find any that simply contain the name

<br>

### Make Font
I haven't implemented the feature this is made for yet, but I should explain it anyways

It'll export each given letter of a font into a mesh, and combine them into a .FNT file, so 3D text can be drawn in the game world

```Letter Resolution``` doesn't do anything yet

```Font Name``` only determines the output file name, but when I get the feature implemented it will let you pick a particular font to bake

```Characters to Bake``` is a string of characters you want included in the FNT file. This is to make it language-agnostic, so you can choose to restrict it to roman letters for english text, or include accented letters, or only include katakana or something.

When drawing text it will use the legend written in the FNT file to figure out which character corrosponds to which mesh, and draw them accordingly

I have it encoded in UTF-8, I'm not sure if that'll cause issues, I might have to change it to full-on unicode when I finish implementing it

<br>

### Test Operator
This is for testing new operators or for one-time operations I randomly need, don't use this one

<br>

## Meshes
All meshes need to have a 2nd UV map for the lightmap UVs, if the first UV map works you can just duplicate it

The engine supports meshes containing multiple materials, and will temporarily apply modifiers and triangulate meshes when exporting

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

To give something the skybox shader, its name has to start with ```skybox_```, it doesn't matter what the rest of the name is

<img width="559" height="245" alt="image" src="https://github.com/user-attachments/assets/dee73b10-7559-4d18-8e59-7e9f36e98a64" />


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

  Whether or not the depth of new pixels is written to the depth buffer

- Masked

  If true, the base colour texture is used in the shadow maps to discard pixels below the alpha threshold. Opaque materials are faster to render

- Stencil Write Value

  If non-zero, it's the value that gets written to the stencil buffer, so post processing effects can be limited to certain materials (1 for example is the stencil mask value for materials using SSR)

Blender will then include all textures in the node-tree, the bindings in the shader are based on the height of the nodes, from top to bottom

<br>

## Levels
When converting a level the script will only look at selected objects, so you can divide those objects up into collections however you want.

It will skip anything that can't be added to the level, so you can select absolutely everything and it will know what to do

<br>

### Static vs Dynamic
Things and lights can either be dynamic or static. This is determined by whether the active render flag is enabled (The camera icon next to its name filled in)

<img width="288" height="32" alt="Screenshot 2025-09-02 173833" src="https://github.com/user-attachments/assets/037dd94b-732f-4979-a737-d783f4c30cd1" />

If the camera's filled in, it's static, if not, dynamic

<br>

Static things can't move but they will get baked shadows, and use static GPU memory which is faster and more abundant.

Static lights can't move, cast light/shadows on dynamic objects, or get specular highlights, but they are the fastest to render, the shadows look the best, and you can have as many as you want with no performance hit

Only sun and spot lights can be dynamic, all other lights will be considered static regardless of the flag since there's no support for them

Also, only 1 dynamic sun light is supported, it's not built for rendering tatooine

Just like with Unreal Engine, dynamic spot lights should not overlap if possible, and keep the attenutation distance as short as you can, since it increases the cost of rendering

<br>

### Object ID
The ID of a thing is determined by its pass index, located in the properties tab under Object->Relations

<img width="436" height="509" alt="image" src="https://github.com/user-attachments/assets/032e2282-003e-4390-b3b7-8e5de20ed24c" />

The only thing it affects is which objects will be returned in Lua when calling ```GetThingsById()```

<br>

### Casts Shadows
Whether or not a thing casts dynamic shadows is set by the Shadow flag in the properties under Object->Viewport Display

<img width="399" height="506" alt="image" src="https://github.com/user-attachments/assets/2a9caca9-0131-444c-bd8d-3a55829c1e67" />

If an object is so far away it doesn't have to cast shadows, or it's something like a floor that won't cast shadows on anything, then set this to false for better performance

<br>

### Lua Script
By default, things don't have their own lua script, but you can add a custom string property called ```lua``` to the object, and set it to the name of the script. The engine will look in the ```scripts``` folder for it

<img width="405" height="650" alt="image" src="https://github.com/user-attachments/assets/6e302f47-4e66-49c8-a1bf-23c384315d1c" />
<br>
<img width="383" height="161" alt="image" src="https://github.com/user-attachments/assets/eecdb69a-a0d0-42ee-9fe8-7aed27da1b7b" />
<br>
<img width="363" height="388" alt="image" src="https://github.com/user-attachments/assets/53c93466-8475-4fc5-9cc6-1c30d0b4ba85" />
<br>
<img width="405" height="146" alt="image" src="https://github.com/user-attachments/assets/c7ae45fb-4c91-422a-9614-002b689dda9a" />



When exporting a level, blender will create the script with a template if it doesn't already exist
