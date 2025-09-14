# Game.lua Documentation

This section will go over how to write the Game.lua file

The goal is to leverage the power of modern computers to allow you to write the game entirely in Lua, so you don't have to recompile the executable each time you change something

It's far from complete, subject to change, though I think it'll mostly be additions

I'll quickly go over the 3 functions that the script needs to have, and then get into the API

<br>

# GameBegin()

This function is called when the game first starts up, there's no LevelBegin (yet at least) so here's where you'd put all the LevelBegin stuff

The first thing you need to do is create your camera(s) and call ```SetActiveCamera()``` to pick one to render from
```lua
local mainCamera = NewCamera()

function GameBegin()
  -- You can change the active camera at any time
  SetActiveCamera(mainCamera)
end
```

Oh yeah and set its position and target
```lua
SetActiveCamera(mainCamera)

mainCamera.position = float3(0.0, 0.0, 0.0)
mainCamera.target = float3(0.0, 1.0, 0.0)
```

Now to load a level. Call ```LoadLevelFromFile()``` with the name of the level to play
```lua
function GameBegin()
  SetActiveCamera(mainCamera)
  mainCamera.position = float3(0.0, 0.0, 0.0)
  mainCamera.target = float3(0.0, 1.0, 0.0)

  LoadLevelFromFile("testscene")
end
```

<br><br>

# KeyCallback()
Every game needs controls, so GLFW's key callback is exposed to lua

```lua
-- In order to register a key being held, you'll have to implement that yourself
local key_held = false

function KeyCallback(key, scancode, action, mods)
  -- It's the exact same as writing a callback for GLFW

  if action ~= GLFW_PRESS and action ~= GLFW_RELEASE then return end

  if key == GLFW_KEY_W then
    key_held = action == GLFW_PRESS
  end
end
```

<br><br>

# GameTick()
This function is called per-frame, where you'd update things like camera location

It takes 2 parameters, the first one is the delta time, and the second is whether or not the mouse is locked to the window, this is so that controls can be ignored if the user pressed ```TAB``` to unlock the cursor

```lua
function GameTick(delta, locked)
  if not locked then
    local forward = glm.normalize(mainCamera.target - mainCamera.position)
  
    if key_held then
      mainCamera.position = mainCamera.position + forward
      mainCamera.target = mainCamera.target + forward
    end
  end
end
```

<br><br>

# API
The API uses metatables to make working with things like vectors fairly straight-forward, for example
```lua
local myVector = float3(1.0, 2.0, 3.0)

-- Why doesn't lua have += again?
myVector.x = myVector.x + 2.0

myVector = glm.normalize(myVector + float3(4.0, 5.0, 6.0))
```

<br>

## Types

### float2
Properties
- x/r (number)
- y/g (number)

Methods

-none-

<br>

### float3
Properties
- x/r (number)
- y/g (number)
- z/b (number)

Methods

-none-

<br>

### float4
Properties
- x/r (number)
- y/g (number)
- z/b (number)
- w/a (number)

Methods

-none-

<br>

### Camera
Properties
- position (float3)
- target (float3)

Methods
- TargetFromRotation(rotX (number), rotY (number))

<br>

### Material
Properties

-none-

Methods

-none-

<br>

### Thing (equivalent to Unreal Engine's Actor)
Properties
- position (float3)
- rotation (float3)
- scale (float3)
- id (number)
- materials (table of Material)
- isStatic (boolean)

Methods

-none-

<br>

### TraceRayResult
Properties
- hit (boolean)
- object (Thing)
- distance (number)

Methods

-none-

<br>

## Libraries

### glm
You have access to all vector functions from GLM in lua, that includes:
- abs
- cross
- distance
- dot
- length
- normalize
- reflect
- refract
```lua
local var1 = glm.cross(float3(0.0, 1.0, 0.0), float3(1.0, 0.0, 0.0))
local var2 = glm.length(var1)
local var3 = glm.distance(var1, float3(0))
```

### glfw
Several window functions have been brought over from GLFW, though there's still quite a few to add
- GetCursorPos
- SetCursorPos
- GetInputMode
- SetInputMode
- GetWindowPos
- SetWindowPos
- GetWindowTitle
- SetWindowTitle
- GetTime
- FocusWindow (why did I think it was SetFocus)

On top of that I've added quite a few keywords for using in functions like SetInputMode()
```lua
local x, y = glfw.GetCursorPos()

local oldInputMode = glfw.GetInputMode(GLFW_RAW_MOUSE_MOTION)
glfw.SetInputMode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)

glfw.SetWindowTitle("My Game")

local wx, wy = glfw.GetWindowPos()
glfw.SetWindowPos(wx + 50, wy)

local time = glfw.GetTime()

glfw.FocusWindow()
```

<br>

## Global Functions (from engine.lua)
Since Game.lua runs after engine.lua and they share the same state, all the functions and Vulkan stuff can be accessed in Game.lua

### LoadImage(filename (string))

Opens the image and returns a Texture

<br>

### CreateImage(imageType, imageViewType, format, width, height, mipLevels, arrayLayers, sampleCount, imageTiling, imageUsage, aspectFlags, magFilter, minFilter, addressMode)

Creates a new blank image for rendering on or transferring to

```imageType``` is a VkImageType and can be one of the following:

- VK_IMAGE_TYPE_1D
- VK_IMAGE_TYPE_2D
- VK_IMAGE_TYPE_3D

```imageViewType``` is a VkImageViewType and usually corrosponds with the image type:

- VK_IMAGE_VIEW_TYPE_1D
- VK_IMAGE_VIEW_TYPE_2D
- VK_IMAGE_VIEW_TYPE_3D
- VK_IMAGE_VIEW_TYPE_CUBE
- VK_IMAGE_VIEW_TYPE_1D_ARRAY
- VK_IMAGE_VIEW_TYPE_2D_ARRAY
- VK_IMAGE_VIEW_TYPE_CUBE_ARRAY

This one basically tells shaders how it should interpret the texture

The reason it needs this is for situations like cubemaps, they are an array of 6 2D images, so here you can specify whether it's just an array or a cubemap

```format``` is a VkFormat, there's too many to list here but I'll at least show the most common ones

- VK_FORMAT_R8G8B8A8_SRGB - Used for colour images
- VK_FORMAT_R8G8B8A8_UNORM - Used for non-colour images
- VK_FORMAT_R16G16B16A16_SFLOAT - Used for HDR render targets

```width``` and ```height``` are self-explanatory

```mipLevels``` can just be left at 1, there's no way to generate mips from lua right now

```arrayLayers``` if your texture is an array of images, this is how many images are in that array. Normally it would just be 1

```sampleCount``` is the MSAA samples, just leave it at ```VK_SAMPLE_COUNT_1_BIT```

```imageTiling``` is one of the VkImageTiling enum values, and can be one of the following:

- VK_IMAGE_TILING_OPTIMAL
- VK_IMAGE_TILING_LINEAR

I don't know what linear means exactly, I always just go with optimal

```imageUsage``` is a set of flags describing how the image is going to be used, like with the formats there are way too many to list here but I'll do a few, it can be a combination of the following:

- VK_IMAGE_USAGE_SAMPLED_BIT - This texture is going to be sampled in a shader
- VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT - This texture is going to be rendered to
- VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT - This texture will be used as a depth/stencil buffer
- VK_IMAGE_USAGE_TRANSFER_SRC_BIT - This texture will be blit'd to another texture
- VK_IMAGE_USAGE_TRANSFER_DST_BIT - This texture will have another texture blit'd onto it

```aspectFlags``` tells vulkan what kind of data this texture will contain, yet again there are too many to list but here's the ones I use:

- VK_IMAGE_ASPECT_COLOR_BIT - This image will store colour information
- VK_IMAGE_ASPECT_DEPTH_BIT - This image will store depth information
- VK_IMAGE_ASPECT_STENCIL_BIT - This image will store stencil information

```magFilter``` is what the GPU should do when it needs to upscale this texture

- VK_FILTER_LINEAR - Blur the result
- VK_FILTER_NEAREST - Pixelate the result

```minFilter``` is what the GPU should do when it needs to downscale this texture

- VK_FILTER_LINEAR - Blur the result
- VK_FILTER_NEARESET - Pixelate the result

```addressMode``` is what the GPU should do when sampling outside the bounds of the texture, these ones are fairly self-explanatory

- VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
- VK_SAMPLER_ADDRESS_MODE_REPEAT
- VK_SAMPLER_ADDRESS_MODE_MIRROR

There's more but they require extra extensions or features to be enabled so you're stuck with these 3 by default

<br>

### OneTimeBlit(srcImage, dstImage, filter, srcInitialLayout, srcFinalLayout, dstFinalLayout)

Quickly copies the source texture to the destination texture, this is if you want the blit done right now, instead of during drawing

```srcImage``` and ```dstImage``` are both Textures

```filter``` is the min/mag filter applied when scaling the source texture up or down to fit on the destination texture, it can be one of the following:

- VK_FILTER_LINEAR - Blurs pixels
- VK_FILTER_NEAREST - Picks the closest pixel

```srcInitialLayout``` is the current layout of the image, before the transfer happens. Usually it's ```VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL```

```srcFinalLayout``` is the layout you want the source texture to be after the transfer, you'd usually have it be ```VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL``` so it can be sampled in a shader, or nil if you don't need the source texture after this and don't care what layout it ends up in

```dstFinalLayout``` is the layout you want the destination texture to be after the transfer, again this should usually be READ_ONLY_OPTIMAL so it can be sampled in a shader

<br>

## Global Functions (from Game.lua)

While everything from engine.lua can be accessed in Game.lua, it doesn't work in reverse so these are Game.lua only

### NewCamera()

Allocates a new camera and returns it

<br>

### GetActiveCamera()

Returns the camera currently being viewed from

<br>

### SetActiveCamera()

Sets the camera being viewed from

<br>

### DirFromAngle(rotation (number))

Calculates a forward vector based on a Z axis rotation, a shortcut for first person games

<br>

### GetThingsById(ID (number))

Returns a table of all objects in the level with a particular ID

Don't call this one per frame if you can avoid it, as it has to loop through every single object.

<br>

### GetThingsInRadius(position (float3), distance (number), ID (number))

Gets all things within a given distance from the sample point with a matching ID

<br>

### GetAllThingsInRadius(position (float3), distance (number))

Gets all things within a given distance from the sample point, regardless of ID

<br>

### TraceRay(start (float3), end (float3), ID (number))

Traces a ray against the bounding boxes of all things with the given ID and returns a TraceRayResult

<br>

### LoadLevelFromFile(levelName (string))

Looks in the levels folder for a folder with the given name, and loads its .lvl file

<br>

### Material(shader, textures, roughness)

Creates a new material and returns it. 

```shader``` can either be a path to a .MAT file, or a shader index, which can be one of the following:

- SHADER_DIFFUSE
- SHADER_METAL
- SHADER_GLASS
- SHADER_SKYBOX
- SHADER_DIFFUSE_MASKED
- SHADER_METAL_MASKED

```textures``` is a table of tables, containing the filename, and whether it's non-colour

```lua
textures = {
  { "path_to_colour.png", false },
  { "path_to_normal.png", true  }
}
```

```roughness``` is a number from 0-1, it's unused but eventually it will be a roughness multiplier

<br>

### SpawnThing(position, rotation, scale, meshName, materials, shadowMap, isStatic, castsShadows, ID, luaScriptPath)

Spawns a thing, keep in mind it's a bit expensive to do during the tick, as a whole bunch of things need to be re-made, like GPU buffers, and descriptor sets. Spawning it during GameBegin is fine though, just make sure to do it after calling LoadLevelFromFile

```position```, ```rotation```, and ```scale``` are all float3s

```meshName``` is a string, it can be found in blender, it's the name of the data parented to an object. Replace ```.``` with ```_``` (I'll add a picture for this)

```materials``` is a table of materials, each made with the ```Material()``` function. It needs to match the number of material slots in blender

```shadowMap``` is a Texture, it can be loaded with the ```LoadImage()``` function

```isStatic``` is whether or not the thing can move, if not it'll use static GPU memory which is faster and more abundant

```castsShadows``` is whether or not it casts dynamic shadows

```luaScriptPath``` is a string, it can be nil if it won't run its own lua code