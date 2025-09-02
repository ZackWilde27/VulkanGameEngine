local lastX = 0.0
local lastY = 0.0

local halfpi = 1.57079

local mainCamera = NewCamera()
local noclip = true

function GameBegin()
	-- The active camera needs to be set, since it allows you to switch between cameras at any point
	SetActiveCamera(mainCamera)

	LoadLevelFromFile("testscene")

	lastX, lastY = glfw.GetCursorPos()

	mainCamera.position = float3(0.0, 0.0, 1.0)--float3(-17.9, 19.5, 5.8)
	mainCamera.target = float3(-0.69513, 0.718, 1.0)--float3(-15.6, 20.8, 5.8)

	--[[
	local scale = float3(1.0, 1.0, 1.0)

	local spawnOffsets = {
		float3(0.44, 0.0, 0.05),
		float3(0.2, 0.0, 0.05),
		float3(-0.04, 0.0, 0.05),
		float3(-0.28, 0.0, 0.05),

		float3(0.44, 0.0, -0.25),
		float3(0.2, 0.0, -0.25),
		float3(-0.04, 0.0, -0.25),
		float3(-0.28, 0.0, -0.25),

		float3(0.44, 0.0, -0.55),
		float3(0.2, 0.0, -0.55),
		float3(-0.04, 0.0, -0.55),
		float3(-0.28, 0.0, -0.55)
	}
	
	-- Spawn all the bins for each shelf
	for k, shelf in pairs(GetObjectsById(1)) do
		-- SpawnObject(position, rotation, scale, meshFilename, shadowMapFilename, materialIndex, texScale, isStatic, id, luaScriptFilename_optional)
		for key, offset in pairs(spawnOffsets) do
			SpawnObject(shelf.position + offset, shelf.rotation, scale, "models/zonbox.msh", "textures/zonbox_shadowmap.png", 1, 1.0, false, 2, nil)
		end
	end
	]]
end

local wKey = false
local sKey = false
local aKey = false
local dKey = false
local shiftKey = false


local baseWalkSpeed = 0.5

local walkSpeed = baseWalkSpeed
local runSpeed = baseWalkSpeed * 2

local targetWalkSpeed = baseWalkSpeed


--[[
function PushedBinCallback(object)
	print("Done Move")
end

function MoveBin(object)
	MoveObjectTo(object, object.position + DirFromAngle(object.rotation.z + halfpi) * 0.25, 0.1, "PushedBinCallback")
end
]]

function KeyCallback(key, scancode, action, mods)
	if action ~= GLFW_PRESS and action ~= GLFW_RELEASE then
		return
	end

	local isPressed = action == GLFW_PRESS

	if key == GLFW_KEY_W then
		wKey = isPressed

	elseif key == GLFW_KEY_S then
		sKey = isPressed

	elseif key == GLFW_KEY_A then
		aKey = isPressed

	elseif key == GLFW_KEY_D then
		dKey = isPressed
	--[[
	elseif key == GLFW_KEY_E and isPressed then
		local result = TraceRay(mainCamera.position, mainCamera.target, 2)
		if result.hit and result.distance < 1.3 then
			MoveBin(result.object)
		end
	]]
	elseif key == GLFW_KEY_LEFT_SHIFT then
		targetWalkSpeed = isPressed and runSpeed or walkSpeed

	end
end

local rotx = 0.0
local roty = 0.0

local LOOK_SENSITIVITY = 0.001

local up = float3(0.0, 0.0, 1.0)

-- locked is whether the game is in focus, so it can stop recieving inputs when you tab out
function GameTick(delta, locked)
	local x, y = glfw.GetCursorPos()

	--print(mainCamera.target.x, mainCamera.target.y, mainCamera.target.z)

	if locked then
		-- Give the walking speed a bit of velocity instead of snapping to the target
		if walkSpeed ~= targetWalkSpeed then
			walkSpeed = walkSpeed + ((targetWalkSpeed - walkSpeed) * 0.1)
		end


		local forward

		-- No-clip mode
		forward = mainCamera.target - mainCamera.position

		-- First-Person mode
		--forward = DirFromAngle(roty)

		local right = glm.cross(forward, up) * walkSpeed
		forward = forward * walkSpeed

		if wKey then
			mainCamera.position = mainCamera.position + forward
		elseif sKey then
			mainCamera.position = mainCamera.position - forward
		end

		if aKey then
			mainCamera.position = mainCamera.position - right
		elseif dKey then
			mainCamera.position = mainCamera.position + right
		end

		roty = roty + ((x - lastX) * LOOK_SENSITIVITY)
		rotx = rotx + ((lastY - y) * LOOK_SENSITIVITY)

		rotx = math.max(math.min(rotx, halfpi), -halfpi)
		mainCamera.TargetFromRotation(rotx, roty)
	end

	lastX = x
	lastY = y
end