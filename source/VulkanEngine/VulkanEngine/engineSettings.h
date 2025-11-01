#define ENGINE_VERSION VK_MAKE_VERSION(3, 0, 0)

// Draw the profiler window
#define LGE_ENABLE_PROFILER

// Ignores the packed level flag stored in the level file
//#define LGE_NO_LEVEL_PACK

//#define ENABLE_RAYTRACING
#define ENABLE_CULLING

// If defined, draw all the bounding boxes and triangles for collision
//#define ENABLE_DEBUG_COLLISION

// You may need to adjust this if you get flickering dark spots on things, it depends on your hardware for some reason
// On my PC, I can set it to 1.0 with no issues, but my laptop needs a really high bias to make it rare
// It can still happen but only if you get *really* close to something and tilt the camera around slowly
// The higher the value is, the less likely the flickering is, but the GPU will do more work shading pixels that will end up covered by other pixels
#define DEPTH_PREPASS_BIAS 20.f

// The maximum size that the shadow map atlas can be
// If the shadow map ends up larger it'll error out
#define MAX_SHADOW_MAP_SIZE 16384

#define BEEG_SHADOWMAP_FORMAT VK_FORMAT_R8G8B8A8_UNORM

#define MAX_OBJECTS 250

// The size of dynamic lights' shadow maps
#define SHADOWMAPSIZE 1600

#define SUBTITLE_BUFFER_SIZE 256
#define MESH_NAME_SIZE 256