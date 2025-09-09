# Game.lua Documentation

This section will go over how to write the Game.lua file

The goal is to leverage the power of modern computers to allow you to write the game entirely in Lua, so you don't have to recompile the executable each time you change something

It's far from complete, subject to change, though I think it'll mostly be additions

<br>

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

### MeshObject (equivalent to Unreal Engine's Actor)
Properties
- position (float3)
- rotation (float3)
- scale (float3)
- id (number)
- isStatic (boolean)

Methods

-none-

<br>

### TraceRayResult
Properties
- hit (boolean)
- object (MeshObject)
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
local var3 = glm.distance(var1, float3(0.0, 0.0, 0.0))
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

## Global Functions

- NewCamera()

Allocates a new camera and returns it

<br>

- GetActiveCamera()

Returns the camera currently being viewed from

<br>

- SetActiveCamera()

Sets the camera being viewed from

<br>

- DirFromAngle(rotation (number))

Calculates a forward vector based on a Z axis rotation, a shortcut for first person games

<br>

- GetObjectsById(ID (number))

Returns a table of all objects in the level with a particular ID

Don't call this one per frame if you can avoid it, as it has to loop through every single object.

<br>

- TraceRay(start (float3), end (float3), ID (number))

Traces a ray against the bounding boxes of all objects with the given ID and returns a TraceRayResult

<br><br>

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
