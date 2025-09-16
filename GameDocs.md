# Game.lua Documentation

This section will go over how to write the Game.lua file

The goal is to leverage the power of modern computers to allow you to write the game entirely in Lua, so you don't have to recompile the executable each time you change something

It's far from complete, subject to change, though I think it'll mostly be additions

I'll quickly go over the functions that the script needs to have, and then get into the API

<br>

# GameBegin()

This function is called when the game first starts up

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

# LevelBegin()
This function is called on the first frame of gameplay

The GameBegin is called before setting up things like the shadow map atlas, render stages, and more, as it gives you the opportunity to load a level and spawn things without needing to re-create anything.

LevelBegin however is called after all the setup, so spawning things has the same cost as doing it during the tick, but it's a much better place to put sound-related stuff for example

```lua
function LevelBegin()
  sound.PlayMusic("sound/test.mp3", false)
end
```

<br><br>

# KeyCallback()
Every game needs controls, so GLFW's key callback is exposed to lua

```lua
-- There is an action for holding a key, but it's like typing where it waits half a second before considering it held
-- you'll have to implement something yourself for a proper solution
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

# MouseCallback()
It's just like the KeyCallback, except for mouse buttons

```lua
-- You know the drill

function MouseCallback(button, action, mods)
  if action ~= GLFW_PRESS then return end

  if button == GLFW_MOUSE_BUTTON_LEFT then
    -- Do stuff when clicking
  end
end
```

<br><br>

# GameTick()
This function is called per-frame, where you'd update things like camera location

It takes 2 parameters, the first one is the delta time, and the second is whether or not the mouse is locked to the window, this is so that controls can be ignored if the user pressed ```TAB``` to unlock the cursor

```lua
function GameTick(delta, locked)
  if locked then
    local forward = glm.normalize(mainCamera.target - mainCamera.position)
  
    if key_held then
      mainCamera.position = mainCamera.position + forward
      mainCamera.target = mainCamera.target + forward
    end
  end
end
```

<br><br>

# Types
The API uses metatables to make working with things like vectors fairly straight-forward, for example
```lua
local myVector = float3(1.0, 2.0, 3.0)

-- Why doesn't lua have += again?
myVector.x = myVector.x + 2.0

myVector = glm.normalize(myVector + float3(4.0, 5.0, 6.0))
```

<br>

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

# Libraries

## glm
You have access to all vector functions from GLM in lua, that includes:
### abs(x) -> float3
### cross(x, y) -> float3
### distance(x, y) -> number
### dot(x, y) -> number
### length(x) -> number
### normalize(x) -> float3
### reflect(incident, normal) -> float3
### refract(incident, normal) -> float3

All of the functions take float3s

```lua
local var1 = glm.cross(float3(0.0, 1.0, 0.0), float3(1.0, 0.0, 0.0))
local var2 = glm.length(var1)
local var3 = glm.distance(var1, float3(0))
```

<br>

## glfw
Several window functions have been brought over from GLFW, though there's still quite a few to add
### GetCursorPos() -> number, number
### SetCursorPos(x, y)
### GetInputMode(mode) -> integer
### SetInputMode(mode, value)
### GetWindowPos() -> number, number
### SetWindowPos(x, y)
### GetWindowTitle() -> string
### SetWindowTitle(title)
### GetWindowAttrib(attrib) -> integer
### SetWindowAttrib(attrib, value)
### GetWindowOpacity() -> number
### SetWindowOpacity(opactiy)
### GetTime() -> integer
### FocusWindow()

It works exactly like writing in C, except you put a ```.``` in between glfw and the rest of the function name, and the glWindow parameter is already implied to be the game window
```lua
local x, y = glfw.GetCursorPos()

local mouseMotion = glfw.GetInputMode(GLFW_RAW_MOUSE_MOTION)
glfw.SetInputMode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)

glfw.SetWindowTitle("My Game")

local wx, wy = glfw.GetWindowPos()
glfw.SetWindowPos(wx + 50, wy)

local time = glfw.GetTime()

glfw.FocusWindow()

-- I added HideCursor() and ShowCursor() as well as shortcuts
glfw.HideCursor()

-- Which is the same as
glfw.SetInputMode(GLFW_RAW_MOUSE_MOTION, GLFW_TRUE)
glfw.SetInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED)
```

<br>

## sound
```sound``` is my wrapper for miniaudio, it's just streamlines a lot of basic sound-effect/music playing

The supported formats are .WAV, .MP3, and .FLAC

### PlaySimple2D(filename)
Plays a sound once with no extra bells and whistles

<br>

### PlayLooping2D(filename)
Same as PlaySimple2D, except the sound will loop indefinitely.

All ```Play``` functions except for Simple2D can only play up to 32 sounds at once, if you try and play more it'll interrupt one of the other sounds to add a new one

It'll try to find a sound that has already finished first, so it'll only interrupt one if all 32 sounds are still playing

<br>

### PlayComplex2D(filename, delayMilliseconds, playTimeMilliseconds, volume, pitch, pan, loop)

```delayMilliseconds``` is an integer, how long does it wait before playing the sound

```playTimeMilliseconds``` is also an integer, how long the sound is played

A playTime of 0 means to play the whole sound

```volume``` is a number, from 0-1

```pitch``` is a number, less than 1.0 means pitch down, greater than 1.0 means pitch up

This will affect the speed of the sound, double the pitch means double the speed, and same goes for pitching down

Also, 0.0 is interpreted as no pitching, so if you want it to go as low as possible it has to be non-zero, like 0.1 or something

```pan``` is a number, 0.0 is the center, -1.0 pans to the left, and 1.0 pans to the right

```loop``` is a boolean

<br>

### PlaySimple3D(filename, position, attenuationModel, minDistance, maxDistance)
This one plays a sound with spatial awareness, so it will get quieter as the camera gets further away, and pan depending on which way the camera faces

```position``` is a float3, the world location of the sound

```attenuationModel``` is an ma_attenuation_model, and can be one of the following:

- ma_attenuation_model_none
- ma_attenuation_model_linear
- ma_attenuation_model_exponential
- ma_attenuation_model_inverse

The exponential attenuation model appears to have an issue where setting minDistance to 0 means you won't hear the sound regardless of location, and the maxDistance doesn't seem to affect anything.

I'd just go with linear

```minDistance``` is a number, how close you need to be to hear it at full volume

```maxDistance``` is a number, how far the sound travels



<br>

### PlayLooping3D(filename, position, attenuationModel, minDistance, maxDistance)
The same as PlaySimple3D except the sound will loop indefinitely

<br>

### PlayComplex3D(filename, position, attenuationModel, minDistance, maxDistance, delayMilliseconds, playTimeMilliseconds, volume, pitch, loop)
The first set of parameters are the same as PlaySimple3D, and the rest are the same as PlayComplex2D

<br>

### AttachToThingSimple(filename, thing, attenuationModel, minDistance, maxDistance)
These functions are the same as Play___3D, except instead of a static point that can't move it'll attach to a Thing, so when the thing moves, the sound will move with it

<br>

### AttachToThingLooping(filename, thing, attenuationModel, minDistance, maxDistance)

<br>

### AttachToThingComplex(filename, position, attenuationModel, minDistance, maxDistance, delayMilliseconds, playTimeMilliseconds, volume, pitch, loop)

<br>

### PlayMusic(filename, loop)
While PlaySimple2D can be used for this, music is separated from sounds so it'll never be interrupted

The only way this will get interrupted is if music is already playing, as there can only be one at a time

<br>

### StopMusic()
Stops the music, destroying the sound object

If all you want to do is pause the music, you can use the next 2 functions:

<br>

### PauseMusic()
Stops the music, but doesn't destroy it, expecting it to be resumed at some point

<br>

### ResumeMusic()
Resumes paused music

<br>

```lua
sound.PlayMusic("sound/test.mp3")

for index, thing in pairs(GetThingById(2)) do
  sound.AttachToThingSimple("sound/otherSound.wav", thing, ma_attenuation_model_linear, 0.1, 350.0)
end
```

<br>

## ma
```ma``` gives lua access to the miniaudio API directly, for situations that my wrapper does not handle

### sound_init_file(filename, flags) -> ma_result, ma_sound

<br>

### sound_start(ma_sound)
### sound_stop(ma_sound)

<br>

### sound_is_playing(ma_sound) -> boolean
### sound_is_looping(ma_sound) -> boolean
### sound_is_spatialization_enabled(ma_sound) -> boolean

<br>

All of the ```sound_set``` and ```sound_get``` functions are available (there's too many to list but here's some of them)
### sound_set_pitch(ma_sound, number)
### sound_set_position(ma_sound, float3)
All of the API functions that take an x, y, and z use a float3 in lua, such as this one
### sound_set_volume(ma_sound, number)

<br>

### sound_get_pitch(ma_sound) -> number
### sound_get_position(ma_sound) -> float3
Similarly, all of the API functions that return an ma_vec3f return a float3 in lua
### sound_get_volume(ma_sound) -> number

<br>

```lua
-- Make sure to initialize the sound outside of the begin/tick
-- The sound will be cut short if lua's garbage collector gets involved
result, mySound = ma.sound_init_from_file("sound/test.mp3", 0)

function GameBegin()

  -- Check if the sound was init'd properly
  if result == MA_SUCCESS then

    -- Set some parameters for the sound
    ma.sound_set_volume(mySound, 0.5)
    ma.sound_set_looping(mySound, true)

    -- Play the sound
    ma.sound_start(mySound)
  
  end

end
```

Now I'm not going to try and explain much of the API, I'm still figuring it out myself, so instead I'll just point you to [the official documentation](https://miniaud.io/docs/manual/index.html#Engine)

There isn't a dedicated section for sound functions so the link points to the Engine section, you'll have to scroll down a bit to find the sound stuff.

Pretty much every function can be accessed, just replace ```ma_``` with ```ma.```, like how ```ma_sound_start``` becomes ```ma.sound_start```

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

Spawns a thing, keep in mind it's a bit more expensive to do during GameTick and LevelBegin, as some things need to be re-made, like GPU buffers, and descriptor sets. Spawning it during GameBegin is basically free though, just make sure to do it after calling LoadLevelFromFile

```position```, ```rotation```, and ```scale``` are all float3s

```meshName``` is a string, it can be found in blender, it's the name of the data parented to an object. Replace ```.``` with ```_```

<img width="399" height="131" alt="image" src="https://github.com/user-attachments/assets/f94c90a9-c9bf-4b98-afa8-f49c42069679" />


```materials``` is a table of materials, each made with the ```Material()``` function. It needs to match the number of material slots in blender

```shadowMap``` is a Texture, it can be loaded with the ```LoadImage()``` function

```isStatic``` is whether or not the thing can move, if not it'll use static GPU memory which is faster and more abundant

```castsShadows``` is whether or not it casts dynamic shadows

```luaScriptPath``` is a string, it can be nil if it won't run its own lua code
