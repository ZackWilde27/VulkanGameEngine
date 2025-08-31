# Zack's Vulkan Engine

This is the main repo for my Vulkan-based game engine

I don't have a name for it, but I'm thinking of calling it the 'Last Gen Engine' because of its focus on optimizing for last gen features that systems like VR and mobile are currently stuck with

I'd say it's kinda like a mix of all engines (though I don't think it'll be able to replace any of them any time soon), it's got the focus on the realism art-style like Unreal Engine/CryEngine, while still being entirely flexible, like Godot/Unity

It was made for these purposes:

- It was fun to make (kinda the main reason)
- To be a light-weight, flexible engine for me to test out graphics features
- To replace old versions of Direct X in games like FlatOut 2 to get better graphics out of them (more on that when I make more progress on it).

<br>

## Building
I use Visual Studio 2022 on Windows, but I test the engine on Linux regularly to make sure it stays cross platform, and to catch any seg-faults that Visual Studio's compiler prevents (It's a nice feature but sometimes I wish I could turn that off so I don't need to bust out the linux machine and do all the debugging on there)

### Windows
Just open the solution in Visual Studio and it should compile with no issues, I have all dependencies included and they should be found when compiling

### Linux
Linux however requires a bit of setup

First, you'll need to download or compile these libraries for your specific machine/OS
- Vulkan
- Lua
- GLFW

Create a new project in your IDE of choice and include said libraries in either the IDE's project settings, or just write ```#pragma comment(lib, "path-to-lib.so")``` in the ```main.cpp```

In the IDE, you'll need to explicity add these files to the project so they will be compiled without an include statement:
- ```main.cpp```
- ```engine.cpp```
- ```VulkanBackend.cpp```
- ```BackendUtils.cpp```
- ```engineUtils.cpp```
- ```luafunctions.cpp```
- ```luaUtils.cpp```

You'll also need to add the ImGui source files as well, under ```include/imgui-1.91.8/```
- ```imgui.cpp```
- ```imgui_draw.cpp```
- ```imgui_impl_glfw.cpp```
- ```imgui_impl_vulkan.cpp```
- ```imgui_tables.cpp```
- ```imgui_widgets.cpp```

Then add these to the include directories
- ```include/imgui-1.91.8```
- ```include/lua-5.4.7/src```
- ```include/vulkan```
- ```include/glfw-3.4/include```
- ```include/stb_image/```
- ```include/cgltf-1.15/```

You need to compile with at least the C++17 Standard to get features like std::filesystem

### macOS
I don't have any computers that run macOS, so you're on your own unfortunately

I'd imagine it's a very similar process to linux though

<br>

## Features
- Dynamic shadows from spot lights and sun lights, with baked GI/AO maps to increase fidelity
- 4x the speed of Unreal Engine in CPU time, while graphics time is neck-and-neck (When all the fancy UE5 stuff is turned off of course)
- Graphics features you'd expect in a modern(ish) engine like SSR, motion blur, bloom, chromatic abberation, and vignette.
- The entire post-process pipeline is implemented in Lua, so it can be edited or even re-done from scratch without re-compiling the executable
- My own tone mapper based on AGX used in Blender, which I call Approximate-AGX or AAGX for short
- It's tightly integrated with Blender so porting from there to the engine is fairly seamless and just a matter of pushing a few buttons (and waiting, a whole lot of waiting sometimes)

<br>

## Things I'm hoping to add in the future
Contributions are welcome so if you'd like to take a stab at one of these, go ahead

- Skinned meshes and their animations
- Dynamic shadows for point lights
- Dynamic AO and GI (SSAO/SSGI or something SDF-based)
- Misc. graphics effects like lens flares, film-grain/noise, and auto exposure
- Improvements to existing effects, like how the SSR doesn't reach very far and is on the expensive side, and how there's no auto-focus for the depth of field
- Compressed assets with zlib
- TAA (I know, but it'd be nice to at least have the option for people who don't mind its shortcomings, I don't want any of the other graphical features to be dependent on it though)
- Hardware Raytracing
- Direct X 12 Backend (For comparing performance against Vulkan)


<br>

## Comparisons

Here's some comparison shots

*All of the levels are from other games, I already did plenty of 3D modelling at my day job so I went the easy route this time, which means they aren't the best showcases of what's possible with the engine*

### Unreal Engine vs My Engine
<img width="1469" height="865" alt="Screenshot 2025-07-18 175314_7" src="https://github.com/user-attachments/assets/e75272a4-32f4-449f-a252-59b08f074ee0" />
Unreal Engine has features like lumen and volumetric fog disabled, while things like baked AO are enabled to try and make it an apples to apples comparison.

### Blender vs My Engine
<img width="1919" height="925" alt="Screenshot 2025-07-17 143102_2" src="https://github.com/user-attachments/assets/b22be89b-466a-48cf-a5d0-2a903b66281e" />
Blender is using Cycles

## Other Photos

Here's the other photos I took

<img width="1698" height="957" alt="Screenshot 2025-07-23 163039" src="https://github.com/user-attachments/assets/07c1de16-d0ec-496a-9ccf-8c095799f166" />
<img width="1702" height="959" alt="Screenshot 2025-08-13 032617" src="https://github.com/user-attachments/assets/ccf84c70-8eea-4291-85e9-bf7a300de0b8" />
<img width="1699" height="953" alt="Screenshot 2025-07-24 004123" src="https://github.com/user-attachments/assets/3b077c41-5d56-4710-bf59-2f4c2abff7e0" />
<img width="1699" height="954" alt="Screenshot 2025-07-25 020329" src="https://github.com/user-attachments/assets/86b3ca32-bb81-4afb-b519-95119dd34a34" />
<img width="1700" height="954" alt="Screenshot 2025-08-12 034351" src="https://github.com/user-attachments/assets/47693966-58f0-48c2-948e-158a7c2a0de0" />
<img width="1700" height="950" alt="Screenshot 2025-07-29 032649" src="https://github.com/user-attachments/assets/519e0ef3-243e-413d-abe1-aa75129e3f6b" />
<img width="1699" height="957" alt="Screenshot 2025-08-12 033357" src="https://github.com/user-attachments/assets/25aeb277-dd3f-4566-b81b-47ed491bb347" />


