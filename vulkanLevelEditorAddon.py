bl_info = {
    "name": "Frame Blender For Blender",
    "blender": (2, 80, 0),
    "category": "Editor",
    "author": "Zack"
}



import bpy
import os
import math
import bmesh
import mathutils
from math import floor
from bpy.props import EnumProperty

wm = bpy.context.window_manager




def RadsToAngle(rads):
    return rads * 57.2958

def convert(x):
    return round(x, 4)

def ConvertLoc(loc):
    return "[" + str(round(loc[0], 3)) + ", " + str(round(-loc[2], 3)) + ", " + str(round(loc[1], 3)) + "]"

def ListConvertLoc(loc):
    return (round(loc[0], 3), round(-loc[2], 3), round(loc[1], 3))

def ListConvertRot(rot):
    return (round(RadsToAngle(rot[0]) - 90, 3), round(RadsToAngle(rot[2]), 3), round(RadsToAngle(rot[1]), 3))

def ConvertCol(col):
    return "[" + str(round(col.r * 255)) + ", " + str(round(col.g * 255)) + ", " + str(round(col.b * 255)) + "]"

def ConvertScale(scale):
    return "[" + str(scale.x) + ", " +  str(scale.z) + ", " + str(scale.y) + "]"

def Distance(v1, v2):
    dx = v1.x - v2.x
    dy = v1.y - v2.y
    dz = v1.z - v2.z
    return math.sqrt((dx * dx) + (dy * dy) + (dz * dz)) + dz

def Lerp(a, b, c):
    return ((b - a) * c) + a

def Lerp3(v1, v2, c):
    return [Lerp(v1.x, v2.x, c), Lerp(v1.y, v2.y, c), Lerp(v1.z, v2.z, c)]

blends = 0

def TriArea(p1, p2, p3):
    minX = min([p1.x, p2.x, p3.x])
    maxX = max([p1.x, p2.x, p3.x])
    minY = min([p1.y, p2.y, p3.y])
    maxY = max([p1.y, p2.y, p3.y])
    side1 = abs(maxX - minX)
    side2 = abs(maxY - minY)
    return (side1 * side2) / 2

def ShadowMapSizeFromData(data):
    meshSurfaceArea = 0
    uvSurfaceArea = 0
    for p in data.polygons:
        meshSurfaceArea += p.area
        p1 = data.uv_layers[1].data[p.loop_indices[0]].uv
        p2 = data.uv_layers[1].data[p.loop_indices[1]].uv
        p3 = data.uv_layers[1].data[p.loop_indices[2]].uv
        uvSurfaceArea += TriArea(p1, p2, p3)
    
    desiredResolution = (meshSurfaceArea / 5000) * 1024
    multiplier = 1/uvSurfaceArea
    
    res = desiredResolution * multiplier
    res = int(res / 256) * 256
    return min(res, 4096)

def ExportMesh(bm):
    global blends
    newmesh = bpy.data.meshes.new("BlendedMesh")
    bm.to_mesh(newmesh)
    bm.free()
    obj = bpy.data.objects.new("BlendedMesh" + str(blends), newmesh)
    blends += 1
    bpy.context.collection.objects.link(obj)
    for i in bpy.context.selected_objects:
        i.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.modifier_add(type='REMESH')
    
takenIndices = []
    
def MyFilter(x):
    return x[0] not in takenIndices

def SafeName(name):
    return name.replace(".", "_")

def GetSurfaceArea(data):
    area = 0
    for face in data.polygons:
        area += face.area
    return area

def ConvertVert(v, t, uv):
    return "{" + f"{convert(v.co.x)}, {convert(v.co.y)}, {convert(v.co.z)}" + "}, {" + f"{convert(v.normal.x)}, {convert(v.normal.y)}, {convert(v.normal.z)}" + "}, {" + f"{convert(t.x)}, {convert(t.y)}, {convert(t.z)}" + "}, {" + f"{convert(uv[0])}, {convert(-uv[1])}" + "}"

#def ConvertVert(v, t, uv):
    #return "{" + f"{convert(v.co.x)}, {convert(v.co.y)}, {convert(v.co.z)}" + "}, {" + f"{convert(v.normal.x)}, {convert(v.normal.y)}, {convert(v.normal.z)}" + "}, {" + f"{convert(uv[0])}, {convert(uv[1])}" + "}"

import struct

def PackVector(v):
    bits = bytearray(struct.pack("f", v.x))
    bits += bytearray(struct.pack("f", v.y))
    return bits + bytearray(struct.pack("f", v.z))

def PackUV(u, v):
    return bytearray(struct.pack("f", u[0])) + bytearray(struct.pack("f", -u[1])) + bytearray(struct.pack("f", v[0])) + bytearray(struct.pack("f", -v[1]))
    
def PackVertex(vert, nrm, tng, uv1, uv2):
    return PackVector(vert.co) + PackVector(nrm) + PackVector(tng) + PackUV(uv1, uv2)

def UnwrapLightmap(data, smartProject):
    if len(data.uv_layers) < 2:
        data.uv_layers.new()

    bpy.ops.object.editmode_toggle()

    if smartProject:
        bpy.ops.uv.smart_project()
    else:
        bpy.ops.uv.lightmap_pack(PREF_PACK_IN_ONE=False, PREF_MARGIN_DIV=0.1)

    bpy.ops.object.editmode_toggle()
    
def Width(mn, mx):
    return (mx - mn) / 2
    

def GetBoundingBox(data):
    mn = [999999, 999999, 999999]
    mx = [999999, 999999, 999999]
    for i in data.vertices:
        mn[0] = min(mn[0], i.co.x)
        mn[1] = min(mn[1], i.co.y)
        mn[2] = min(mn[2], i.co.z)
        mx[0] = max(mx[0], i.co.x)
        mx[1] = max(mx[1], i.co.y)
        mx[2] = max(mx[2], i.co.z)
    
    return (Width(mn[0], mx[0]), Width(mn[1], mx[1]), Width(mn[2], mx[2]))
    
def ConvertMeshToVulkanFile(data, unwrap):    
    indices = []
    vertices = bytearray([])
    vertbits = []
    numVerts = 0
    numIndices = 0
    
    if unwrap:
        UnwrapLightmap(data)
    
    if len(data.uv_layers) < 2:
        raise RuntimeError(f"Mesh: {data.name} does not have lightmap UVs!")
    
    data.calc_tangents()

    with open(f"C:/Users/zackw/source/repos/glfwtest/glfwtest/models/{data.name.replace('.', '_')}.msh", "wb") as meshFile:
        for i in data.polygons:
            numIndices += len(i.vertices)
            for v, l in zip(i.vertices, i.loop_indices):
                vbits = PackVertex(data.vertices[v], data.vertices[v].normal if i.use_smooth else i.normal, data.loops[l].tangent, data.uv_layers[0].data[l].uv, data.uv_layers[1].data[l].uv)
                if vbits not in vertbits:
                    vertbits.append(vbits)
                    numVerts += 1
                    vertices += vbits
                indices.append(vertbits.index(vbits))
                
        dataType = "H" if len(indices) < 65535 else "I"
        meshFile.write(struct.pack("B", 1 if dataType == "I" else 0))
        meshFile.write(bytearray(struct.pack(dataType, numVerts)))
        meshFile.write(vertices)
        meshFile.write(bytearray(struct.pack(dataType, numIndices)))
        for i in indices:
            meshFile.write(struct.pack(dataType, i))
        aabb = GetBoundingBox(data)
        meshFile.write(struct.pack("f", aabb[0]))
        meshFile.write(struct.pack("f", aabb[1]))
        meshFile.write(struct.pack("f", aabb[2]))
    
    data.free_tangents()
    
class TestOperator(bpy.types.Operator):
    """An operator used to test things"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.testoperator"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Test Operator"         # Display name in the interface.
    
    #unwrap: bpy.props.BoolProperty(name="Unwrap Lightmap")

    def execute(self, context):
        for i in bpy.context.selected_objects:
            if i.type != 'MESH': continue
            i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[28].default_value = 0.0
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class ExportMesh(bpy.types.Operator):
    """Writes out the model in C++ for Vulkan"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "frameblender.blend_frames"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Export Mesh (Vulkan)"         # Display name in the interface.
    
    unwrap: bpy.props.BoolProperty(name="Unwrap Lightmap")

    def execute(self, context):
        selected = bpy.context.selected_objects
        for object in selected:
            if object.type != 'MESH': continue
            ConvertMeshToVulkanFile(object.data, self.unwrap)
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class UnwrapLightmapOperator(bpy.types.Operator):
    """Unwraps a mesh's lightmap automatically"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.unwraplightmap"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Unwrap Lightmap (Vulkan)"         # Display name in the interface.
    
    smartProject: bpy.props.BoolProperty(name="Use Smart Project")

    def execute(self, context):
        selected = bpy.context.selected_objects
        bpy.ops.object.select_all(action='DESELECT')
        wm.progress_begin(0, len(selected))
        for object in selected:
            if object.type != 'MESH': continue
            if len(object.data.uv_layers) < 2:
                object.data.uv_layers.new()
            object.data.uv_layers[1].active = True

        for dex, object in enumerate(selected):
            wm.progress_update(dex)
            if object.type != 'MESH': continue
            object.select_set(True)
            UnwrapLightmap(object.data, self.smartProject)
            object.select_set(False)
        
        for object in selected:
            if object.type != 'MESH': continue
            object.data.uv_layers[0].active = True
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class FixFO2UVOperator(bpy.types.Operator):
    """Fixes the UVs on FO2 Tracks"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.fo2uvs"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Fix FO2 UVs (Vulkan)"         # Display name in the interface.

    def execute(self, context):
        selected = bpy.context.selected_objects
        for object in selected:
            if object.type != 'MESH' or len(object.data.uv_layers) < 2: continue
        
            object.data.materials[0].node_tree.nodes["Principled BSDF"].inputs[28].default_value = 0.0
            #matName = object.data.materials[0].name
            #if matName.startswith("SDM_") or matName.startswith("DM_"):
                #object.data.uv_layers.remove(object.data.uv_layers[0])
                #UnwrapLightmap(object.data)

        return {'FINISHED'}

def BakeLighting(obj, resolution, t3sOnly=False):

    obj.select_set(True)
    oldmat = obj.data.materials[0]
    
    obj.data.uv_layers[1].active = True

    if not t3sOnly:
        
        texScale = 1.0
        if LinkedToNormal(oldmat.node_tree.nodes['Principled BSDF'].inputs[5]):
            tex = GetTextureMap(obj.data, 5)
            nrmMap = tex.image
            if tex.inputs[0].is_linked:
                texScale = tex.inputs[0].links[0].from_node.inputs[3].default_value[0]
        else:
            nrmMap = bpy.data.images['default_white_nrm.png']
            
        #nrmMap = oldmat.node_tree.nodes['Principled BSDF'].inputs[5].links[0].from_node.image if (oldmat.node_tree.nodes['Principled BSDF'].inputs[5].is_linked) else bpy.data.images['default_white_nrm.png']
        
        obj.data.materials[0] = bpy.data.materials['AOBake']
        
        obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[0] = texScale
        obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[1] = texScale
        obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[2] = texScale
        
        obj.data.materials['AOBake'].node_tree.nodes['normal'].image = nrmMap
        
        activeImage = bpy.data.materials['AOBake'].node_tree.nodes['Image Texture'].image
        activeImage.scale(resolution, resolution)
        bpy.ops.object.bake(type='COMBINED', pass_filter={'DIRECT', 'INDIRECT', 'DIFFUSE'})
        
        activeImage.save_render('C:/Users/zackw/source/repos/glfwtest/glfwtest/textures/' + SafeName(obj.name) + "_shadowmap.png", quality=100)

    obj.data.uv_layers[0].active = True
    obj.select_set(False)        
    obj.data.materials[0] = oldmat

def AddPoint(x, y, z):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.08, enter_editmode=False, align='WORLD', location=(x, y, z), scale=(1, 1, 1))
    
    
def GetGIProbeLocation(x, y, z):
    
    probe = bpy.data.objects['IrradianceVolume']

    resX = probe.data.grid_resolution_x
    resY = probe.data.grid_resolution_y
    resZ = probe.data.grid_resolution_z
        
    invX = 1/resX
    invY = 1/resY
    invZ = 1/resZ
        
    sclX = (probe.scale[0] * 2)
    sclY = (probe.scale[1] * 2)
    sclZ = (probe.scale[2] * 2)
        
    cenX = (invX / 2) * sclX
    cenY = (invY / 2) * sclY
    cenZ = (invZ / 2) * sclZ
    
    vX = (((x / resX) - 0.5) * sclX) + probe.location.x
    vY = (((y / resY) - 0.5) * sclY) + probe.location.y
    vZ = (((z / resZ) - 0.5) * sclZ) + probe.location.z
                    
    vX += cenX
    vY += cenY
    vZ += cenZ
    
    return (vX, vY, vZ)

def SaveCubemap(filename):
    
    bpy.ops.object.select_all(action='DESELECT')
    
    for side in ['Back', 'Down', 'Front', 'Left', 'Right', 'Up']:
        renderer = bpy.data.objects['CubemapCreator_' + side]
        
        renderer.hide_render = False
        
        renderer.select_set(True)
        bpy.ops.image.open(filepath='C:/Users/zackw/source/repos/glfwtest/glfwtest/textures/' + filename)
        renderer.data.materials[0].node_tree.nodes['skybox'].image = bpy.data.images[filename]
                        
        bpy.ops.object.bake(type='EMIT')
        renderer.data.materials[0].node_tree.nodes['skyboxbake'].image.save(filepath='C:/Users/zackw/source/repos/glfwtest/glfwtest/textures/' + filename[:-4] + "_" + side + ".png")
        renderer.select_set(False)

def RenderAndSaveCubemap(filename):
    for side in ['Back', 'Down', 'Front', 'Left', 'Right', 'Up']:
        renderer = bpy.data.objects['CubemapCreator_' + side]
        renderer.hide_render = True

    bpy.ops.render.render(use_viewport=True)
    bpy.data.images['Render Result'].save_render('C:/Users/zackw/source/repos/glfwtest/glfwtest/textures/' + filename)
    
    SaveCubemap(filename)

def BakeGI(levelname):
        camera = bpy.data.objects['CubemapCreator']
        
        probe = bpy.data.objects['IrradianceVolume']
        
        RenderAndSaveCubemap(levelname + ".png")
        return

        resX = probe.data.resolution_x
        resY = probe.data.resolution_y
        resZ = probe.data.resolution_z
        
        vX = vY = vZ = 0
        
        out = "static GICapture " + levelname + "[] = {\n"
        
        renderer = bpy.data.objects['cubemaprenderer']
        '''
        for x in range(resX):
            for y in range(resY):
                for z in range(resZ):
                    
                    (camera.location.x, camera.location.y, camera.location.z) = GetGIProbeLocation(x, y, z)
                    
                    RenderAndSaveCubemap(f"GI_{x}_{y}_{z}.png")
        '''
        for obj in bpy.data.objects:
            if obj.type == 'LIGHT_PROBE' and obj.data.type == 'CUBEMAP':
                camera.location = obj.location
                
                filename = obj.name.replace(".", "_") + ".png"
                RenderAndSaveCubemap(filename)
                out = f"#include \"{ obj.name.replace('.', '_') }_t3x.h\"" + out
                out += "\t{ " + f"{obj.location.x}, {obj.location.y}, {obj.location.z}, {obj.data.influence_distance}, &{filename}_tex" + " },\n"
        
        return out + "};\n"
                
    
class BakeLightingOperator(bpy.types.Operator):
    """Bakes the shadow maps for all selected objects"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.bakelighting"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Bake Lighting (Vulkan)"         # Display name in the interface.
    
    shadowmapsize: bpy.props.IntProperty(name="Specific Map Resolution")
    calcMapSize: bpy.props.BoolProperty(name="Calculate Map Resolution")
    
    def execute(self, context):
        objects = bpy.context.selected_objects
        bpy.ops.object.select_all(action='DESELECT')
        # progress from [0 - 1000]
        wm.progress_begin(0, len(objects))
        dex = 0
        for i in objects:
            if i.type == 'MESH':
                if self.calcMapSize: self.shadowmapsize = ShadowMapSizeFromData(i.data)
                i.select_set(True)
                BakeLighting(i, self.shadowmapsize)
                i.select_set(False)
            wm.progress_update(dex)
            dex += 1    

        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class BakeGIOperator(bpy.types.Operator):
    """Bakes the GI cubemaps for the scene"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.bakegi"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Bake GI (Vulkan)"         # Display name in the interface.
    
    cubeName: bpy.props.StringProperty(name="Cubemap Name")
    
    def execute(self, context):
        BakeGI(self.cubeName)
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

def GetTextureMap(data, index):
    normalNode = data.materials[0].node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node
    try:
        normalNode.image
        return normalNode
    except:
        return normalNode.inputs[1].links[0].from_node

def GetNormalMap(data):
    return GetTextureMap(data, 5).image

def LinkedToAnImage(input):
                    if input.is_linked:
                        try:
                            return input.links[0].from_node.image.has_data
                        except:
                            return False
                    return False
                
def LinkedToNormal(input):
                    if input.is_linked:
                        try:
                            return input.links[0].from_node.image.has_data
                        except:
                            return LinkedToAnImage(input.links[0].from_node.inputs[1])
                    return False

def ExportMaterial(material):
    bsdf = material.node_tree.nodes['Principled BSDF']
    zlsl = bsdf.label
    vs = f"shaders/{zlsl}_vert.spv"
    ps = f"shaders/{zlsl}_pixl.spv"

    cullMode = "2" if material.use_backface_culling else "0"
    polygonMode = "0"
    alphaBlend = str(bsdf.inputs[4].is_linked).lower()
    try:
        with open(f"C:/Users/zackw/source/repos/glfwtest/glfwtest/materials/{zlsl}.mat", "x") as matFile:
            zlsl = f"shaders/{zlsl}.zlsl"
            matFile.write(zlsl)
            matFile.write("\n")
            matFile.write(vs + "\n")
            matFile.write(ps + "\n")
            matFile.write(cullMode + "\n")
            matFile.write(polygonMode + "\n")
            matFile.write(alphaBlend + "\n")
            matFile.write("true\ntrue\n")
    except:
        pass
    

class ConvertLevel(bpy.types.Operator):
    """Blends between two meshes by assuming the next vertex has the same index as the current"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.convertlevel"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Convert Level (Vulkan)"         # Display name in the interface.
    
    #my_float: bpy.props.FloatProperty(name="Some Floating Point")
    includeshmaps: bpy.props.BoolProperty(name="Bake Shadow Maps")
    bakegi: bpy.props.BoolProperty(name="Bake GI")
    levelname: bpy.props.StringProperty(name="Level Name")
    vshader: bpy.props.IntProperty(name="Vertex Shader Index")
    shadowmapsize: bpy.props.IntProperty(name="Shadow Map Resolution")
    unwrap: bpy.props.BoolProperty(name="Unwrap Lightmaps")
    exportMeshes: bpy.props.BoolProperty(name="Export Meshes")
    #num_frames: bpy.props.IntProperty(name="Number of Frames")
    '''
    vert_method: EnumProperty(
        name="Blend Method",
        items=(
            ('d', 'Distance-Based', 'Blends to the closest vertex'),
            ('i', 'Index-Based', 'Blends to the vertex at the same index.')
        )
    )
    '''

    def execute(self, context):
        global lvlStrings
        
        writtenMeshes = []
        
        writtenMaterials = []
        
        lvlStrings = bytearray([])
        lvlMats = bytearray([])
        MeshStringLocs = []
        tempMats = []
        tempObjs = []
        customShaders = []
            
        allobjects = bpy.context.selected_objects
        
        bpy.ops.object.select_all(action='DESELECT')
        
        # progress from [0 - 1000]
        wm.progress_begin(0, len(allobjects))
            
        progress = 0
        
        def StringToBytes(string):
            return bytearray(bytes(string, encoding="ansi")) + bytearray([0])
        
        def AddLvlString(string):
            global lvlStrings
            bits = StringToBytes(string)
            if bits in lvlStrings:
                return lvlStrings.index(bits)

            index = len(lvlStrings)
            lvlStrings += bits
            return index
        
        for i in allobjects:
            wm.progress_update(progress)
            progress += 1
            
            if i.type != 'MESH': continue
            
            if i.data.name not in writtenMeshes:
                if self.exportMeshes:
                    i.select_set(True)
                    ConvertMeshToVulkanFile(i.data, self.unwrap)
                    i.select_set(False)
                MeshStringLocs.append(StringToBytes(f"{SafeName(i.data.name)}"))
                writtenMeshes.append(i.data.name)
                
            meshIndex = writtenMeshes.index(i.data.name)
            
            if not i.data.materials:
                raise SyntaxError(i.name + " does not have a material")

            if i.data.materials[0].name not in writtenMaterials:
                
                def GetShaderIndex(node):
                    additional = 4 if node.inputs[4].is_linked else 0
                    if node.inputs[1].default_value > 0.5:
                        return 1 + additional
                    if node.inputs[18].default_value > 0.5:
                        return 2
                    return 0 + additional
                
                numTextures = 0
                tempMats.append([])
                tempMats[-1].append(3)
                bsdfNode = i.data.materials[0].node_tree.nodes['Principled BSDF']
                tempMats[-1].append(AddLvlString((f"textures/{SafeName(i.data.materials[0].name)}_col.png" if bsdfNode.inputs[0].is_linked else "textures/default_white_col.png")))
                tempMats[-1].append(AddLvlString((f"textures/{SafeName(i.data.materials[0].name)}_rgh.png" if bsdfNode.inputs[2].is_linked else "textures/default_white_rgh.png")))
                tempMats[-1].append(AddLvlString((f"textures/{SafeName(i.data.materials[0].name)}_nrm.png" if LinkedToNormal(bsdfNode.inputs[5]) else "textures/default_white_nrm.png")))
                tempMats[-1].append(convert(i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[2].default_value))
                label = bsdfNode.label
                if label:
                    if label not in customShaders:
                        ExportMaterial(i.data.materials[0])
                        customShaders.append(label)

                    tempMats[-1].append(customShaders.index(label) + 6)
                elif i.data.materials[0].name.startswith("skybox_"):
                    tempMats[-1].append(3)
                else:
                    tempMats[-1].append(GetShaderIndex(i.data.materials[0].node_tree.nodes['Principled BSDF']))

                def SaveInputIfLinked(index, extension):
                    if i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[index].is_linked:
                        try:
                            image = i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.image
                        except:
                            if not i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.inputs[1].is_linked:
                                print(f"Normal Map Node not linked: {extension}, {i.data.materials[0].name}")
                                return
                            image = i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.inputs[1].links[0].from_node.image
                        
                        if not image.has_data:
                            print(f"Image has no data: {extension}, {i.data.materials[0].name}")
                            return

                        if max(image.size) > 1024:
                            image.scale(1024, 1024)
                        image.save(filepath='C:/Users/zackw/source/repos/glfwtest/glfwtest/textures/' + SafeName(i.data.materials[0].name) + extension + '.png', quality=80)
                    else: print(f"Input not linked: {extension}, {i.data.materials[0].name}")
                SaveInputIfLinked(0, "_col")
                SaveInputIfLinked(5, "_nrm")
                SaveInputIfLinked(2, "_rgh")

                writtenMaterials.append(i.data.materials[0].name)
            
            if i.data.materials[0].node_tree.nodes["Principled BSDF"].inputs[28].default_value == 0.0:
                BakeLighting(i, self.shadowmapsize, not self.includeshmaps)
            
            node = i.data.materials[0].node_tree.nodes["Principled BSDF"]
            texScale = 1.0
            if node.inputs[0].is_linked and node.inputs[0].links[0].from_node.inputs[0].is_linked:
                texScale = node.inputs[0].links[0].from_node.inputs[0].links[0].from_node.inputs[3].default_value[0]
            elif node.inputs[5].is_linked and node.inputs[5].links[0].from_node.inputs[0].is_linked:
                texScale = node.inputs[5].links[0].from_node.inputs[0].links[0].from_node.inputs[3].default_value[0]

            def PackFloat(y):
                return bytearray(struct.pack("f", y))

            def PackListVec(x):
                return PackFloat(x[0]) + PackFloat(x[1]) + PackFloat(x[2])
            
            def PackRot(x):
                return PackFloat(x[0]) + PackFloat(x[1]) + PackFloat(x[2])

            tempObjs.append([])
            tempObjs[-1].append(meshIndex)
            tempObjs[-1].append(PackListVec(i.location))
            tempObjs[-1].append(PackRot(i.rotation_euler))
            tempObjs[-1].append(PackListVec(i.scale))
            tempObjs[-1].append(texScale)
            tempObjs[-1].append(writtenMaterials.index(i.data.materials[0].name))
            tempObjs[-1].append(not i.hide_render)
            tempObjs[-1].append(i.pass_index)
            tempObjs[-1].append(AddLvlString((f"textures/{SafeName(i.name)}_shadowmap.png" if (i.data.materials[0].node_tree.nodes['Principled BSDF'].inputs[28].default_value == 0.0) else "textures/default_shadowmap.png")))

        wm.progress_end()
            
        fileLength = (len(tempMats) * 18) + 2
        fileLength += sum([len(item) for item in MeshStringLocs]) + 2
        fileLength += (len(tempObjs) * 49) + 2
        fileLength += len(customShaders) * 4 + 2
        
        with open(f"C:/Users/zackw/source/repos/glfwtest/glfwtest/levels/{self.levelname}.lvl", "wb") as file:
            file.write(struct.pack("H", len(customShaders)))
            for i in customShaders:
                file.write(struct.pack("I", AddLvlString(i) + fileLength))
            
            file.write(struct.pack("H", len(tempMats)))
            for i in tempMats:
                file.write(struct.pack("B", i[5])) # Shader Index
                file.write(struct.pack("B", i[0])) # Num Textures (3)
                file.write(struct.pack("I", i[1] + fileLength)) # Albedo Filename
                file.write(struct.pack("I", i[2] + fileLength)) # Roughness Filename
                file.write(struct.pack("I", i[3] + fileLength)) # Normal Filename
                file.write(struct.pack("f", i[4])) # Roughness
            
            file.write(struct.pack("H", len(MeshStringLocs)))
            for i in MeshStringLocs:
                file.write(i)
            
            file.write(struct.pack("H", len(tempObjs)))
            for i in tempObjs:
                file.write(struct.pack("H", i[0])) # Mesh index
                file.write(i[1]) # Position
                file.write(i[2]) # Rotation
                file.write(i[3]) # Scale
                file.write(struct.pack("f", i[4])) # TexScale
                file.write(struct.pack("B", i[5])) # Material Index
                file.write(struct.pack("?", i[6])) # IsStatic
                file.write(struct.pack("B", i[7])) # meshID
                file.write(struct.pack("I", i[8] + fileLength)) # Shadow map filename

            file.write(lvlStrings)
        
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
def BlendButton(self, context):
    layout = self.layout
    layout.operator("frameblender.blend_frames")

def ConvertLevelButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.convertlevel")

def BakeLightingButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.bakelighting")
    
def BakeGIButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.bakegi")

def UnwrapLightmapButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.unwraplightmap")

def TestOperatorButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.testoperator")

def FixFO2UVButton(self, context):
    layout = self.layout
    layout.operator("vulkan_utils.fo2uvs")
  
def register():
    bpy.utils.register_class(ExportMesh)
    bpy.utils.register_class(ConvertLevel)
    bpy.utils.register_class(BakeLightingOperator)
    bpy.utils.register_class(BakeGIOperator)
    bpy.utils.register_class(UnwrapLightmapOperator)
    bpy.utils.register_class(FixFO2UVOperator)
    bpy.utils.register_class(TestOperator)

    bpy.types.VIEW3D_MT_object.append(BlendButton)
    bpy.types.VIEW3D_MT_object.append(ConvertLevelButton)
    bpy.types.VIEW3D_MT_object.append(BakeLightingButton)
    bpy.types.VIEW3D_MT_object.append(BakeGIButton)
    bpy.types.VIEW3D_MT_object.append(UnwrapLightmapButton)
    bpy.types.VIEW3D_MT_object.append(TestOperatorButton)
    bpy.types.VIEW3D_MT_object.append(FixFO2UVButton)
    
def unregister():
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.types.VIEW3D_MT_object.pop()

    bpy.utils.unregister_class(ExportMesh)
    bpy.utils.unregister_class(ConvertLevel)
    bpy.utils.unregister_class(BakeLightingOperator)
    bpy.utils.unregister_class(BakeGIOperator)
    bpy.utils.unregister_class(UnwrapLightmapOperator)
    bpy.utils.unregister_class(FixFO2UVOperator)
    bpy.utils.unregister_class(TestOperator)
    
    
if __name__ == "__main__":
    register()