# Zack's Vulkan Engine

This is the main repo for my Vulkan-based game engine

I don't have a name for it, but I'm thinking of calling it the 'Last Gen Engine' because of its focus on optimizing for last gen features that systems like VR and mobile are currently stuck with

It was made for these purposes:

- It was fun to make (kinda the main reason)
- To be a light-weight, flexible engine for me to test out graphics features
- To replace old versions of Direct X in games like FlatOut 2 to get better graphics out of them (more on that when I make more progress on it).

<br>

The plan is to get this running on VR headsets to hopefully use this in a real game, but so far getting anything made with Vulkan to run in Android Studio has been a nightmare of Gradle/JDK mismatching, outdated page-sizes, and cache corruption so I don't know if that will ever happen

<br>

There's a few README's you'll want to take a look at if you plan on working with the engine
- ```README.md``` (this one)
- [BlenderProjectDocs.md](https://github.com/ZackWilde27/VulkanGameEngine/blob/main/BlenderProjectDocs.md) (Explains how to export levels with the blender project)
- [ShaderDocs.md](https://github.com/ZackWilde27/VulkanGameEngine/blob/main/ShaderDocs.md) (Explains how to write shaders in ZLSL)
- [LuaDocs.md](https://github.com/ZackWilde27/VulkanGameEngine/blob/main/LuaDocs.md) (Explains how to edit ```engine.lua``` to add, remove, or change post processing steps)
- [GameDocs.md](https://github.com/ZackWilde27/VulkanGameEngine/blob/main/GameDocs.md) (Explains how to use the API in ```Game.lua``` to make a game)

<br><br>

## Building
I use Visual Studio 2022 on Windows, but I test the engine on Linux regularly to make sure it stays cross platform, and to catch any seg-faults that Visual Studio's compiler prevents (It's a nice feature but sometimes I wish I could turn that off so I don't need to bust out the linux machine and do all the debugging on there)

<br>

### Windows
Just open ```VulkanEngine.sln``` located in ```source/VulkanEngine``` in Visual Studio and it *should* compile with no issues, I have all libraries included with everything set up to find them

<br>

### Linux
If you have a Debian-based OS and don't mind using Code::Blocks (I know, not the most modern IDE in the world but CLion requires an account) then it's already set up

Open ```VulkanEngine.cbp``` located in ```source/VulkanEngine/VulkanEngine``` in Code::Blocks and just like with Visual Studio it *should* compile with no extra setup, all libraries included and all that

<br>

Using a different IDE or OS however requires a bit of setup

First, if the OS is not debian-based you'll need to download or compile these libraries for your specific machine/OS
- Vulkan
- Lua
- GLFW

Create a new project in your IDE of choice and include said libraries in either the IDE's project settings, or just put ```#pragma comment(lib, "path-to-lib.so")``` for each library in ```main.cpp```

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
- ```include/imgui-1.91.8/```
- ```include/lua-5.4.7/src/```
- ```include/vulkan/```
- ```include/glfw-3.4/include/```
- ```include/stb_image/```
- ```include/cgltf-1.15/```

You need to compile with at least the C++17 Standard to get features like std::filesystem

<br>

### macOS
I don't have any computers that run macOS, so you're on your own unfortunately

I'd imagine it's a very similar process to linux though

<br><br>

## Features
- Dynamic shadows from spot lights and sun lights, with baked GI/AO maps to increase fidelity
- 4x the speed of Unreal Engine in CPU time, while graphics time is neck-and-neck (When all the fancy UE5 stuff is turned off of course)
- Graphics features you'd expect in a modern(ish) engine like SSR, motion blur, bloom, chromatic abberation, and vignette.
- The entire post-process pipeline is implemented in Lua, so it can be edited or even re-done from scratch without re-compiling the executable
- My own tone mapper based on AGX used in Blender, which I call Approximate-AGX or AAGX for short
- It's tightly integrated with Blender so porting from there to the engine is fairly seamless and just a matter of pushing a few buttons (and waiting, a whole lot of waiting sometimes)
- Baked lighting uses cycles from blender, so you are guaranteed to get photo-realistic results, but you get what you wait for, since the baking process takes a lot longer than other proprietary solutions

<br><br>

## Known Issues

- The ```fo2City``` level has a culling issue, I think it's because I combined almost everything sharing a material into a single mesh
- The lighting isn't fully baked in ```soleanna``` so some areas become pitch black in the shadows, and there's 1 shadow map straight up missing
- Speaking of baked lighting, most things are going to need to be re-baked with denoising turned off, I recently learned it's the reason there's seams in quite a few places
- Depending on your hardware and the level the prepass depth bias might not be enough, you can increase it in ```VulkanBackend.cpp```
- The engine has a feature where it will not render when it loses focus, that's not an issue but it might look like it froze so I thought I'd mention it, just click on the window again and it will resume

<br><br>

## Things I'm hoping to add in the future
Contributions are welcome so if you'd like to take a stab at one of these, go ahead

- Skinned meshes and their animations
- Dynamic shadows for point lights
- Dynamic AO and GI (SSAO/SSGI or something SDF-based)
- Misc. graphics effects like lens flares, film-grain/noise, and auto exposure
- Improvements to existing effects, like how the SSR doesn't reach very far and is on the expensive side, and how there's no auto-focus for the depth of field
- Compressed assets with zlib
- TAA (I know, but it'd be nice to at least have the option for people who don't mind its shortcomings, I don't want any of the other graphical features to be dependent on it though)
- Hardware Raytracing (Just for experimentation / comparing traditional graphics to ground-truth)
- Direct X 12 Backend (For comparing performance against Vulkan)

<br><br>

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




