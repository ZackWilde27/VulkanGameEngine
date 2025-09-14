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
import struct

wm = bpy.context.window_manager

folderDir = bpy.path.abspath("//")


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

def SetColourManagement(viewTransform, look, exposure, gamma):
    viewSettings = bpy.data.scenes["Scene"].view_settings
    viewSettings.view_transform = viewTransform
    viewSettings.exposure = exposure
    viewSettings.gamma = gamma
    viewSettings.look = look

def GetColourManagement():
    viewSettings = bpy.data.scenes["Scene"].view_settings
    return (viewSettings.view_transform, viewSettings.look, viewSettings.exposure, viewSettings.gamma)

def BakeTexture_Object(material, size=1024, bakeNormal=True):
    if 'TextureBake' in bpy.data.images:
        bpy.data.images.remove(bpy.data.images['TextureBake'])

    bpy.ops.image.new(name='TextureBake', width=size, height=size, float=True)
    
    try:
        bakeNode = material.node_tree.nodes['TextureBake']
    except:
        bakeNode = material.node_tree.nodes.new('ShaderNodeTexImage')
    
    bakeNode.image = bpy.data.images['TextureBake']
    bakeNode.image.scale(size, size)
    bakeNode.image.colorspace_settings.name = 'Non-Color'
    
    (oldVT, oldLook, oldExposure, oldGamma) = GetColourManagement()
    SetColourManagement('Raw', 'None', 0.0, 1.0)
    
    SetColourManagement(oldVT, oldLook, oldExposure, oldGamma)
    
    oldActive = material.node_tree.nodes.active
    material.node_tree.nodes.active = bakeNode
    bpy.ops.object.bake(type = ('NORMAL' if bakeNormal else 'ROUGHNESS'))
    material.node_tree.nodes.active = oldActive
    
    matName = SafeName(material.name)
    bakeNode.image.save(filepath=folderDir + "textures/" + matName + ("_nrm" if bakeNormal else "_rgh") + ".png")
    
    bpy.data.images.remove(bpy.data.images['TextureBake'])
    

# Options: NORMAL, ROUGHNESS
def BakeTexture_Plane(material, size=1024, bakeNormal=True):
    renderPlane = bpy.data.objects['texturerenderingplane']
    renderPlane.select_set(True)
    
    renderPlane.data.materials[0] = material
    
    if 'TextureBake' in bpy.data.images:
        bpy.data.images.remove(bpy.data.images['TextureBake'])
    

    bpy.ops.image.new(name='TextureBake', width=size, height=size, float=True)
    
    try:
        bakeNode = material.node_tree.nodes['TextureBake']
    except:
        bakeNode = material.node_tree.nodes.new('ShaderNodeTexImage')
    
    bakeNode.image = bpy.data.images['TextureBake']
    bakeNode.image.scale(size, size)
    bakeNode.image.colorspace_settings.name = 'Non-Color'
    
    context_override = bpy.context.copy()
    context_override['selected_objects'] = [renderPlane]
    context_override['active_object'] = renderPlane
    
    (oldVT, oldLook, oldExposure, oldGamma) = GetColourManagement()
    SetColourManagement('Raw', 'None', 0.0, 1.0)

    with bpy.context.temp_override(**context_override):
        oldActive = material.node_tree.nodes.active
        material.node_tree.nodes.active = bakeNode
        bpy.ops.object.bake(type = ('NORMAL' if bakeNormal else 'ROUGHNESS'))
        material.node_tree.nodes.active = oldActive
    
    SetColourManagement(oldVT, oldLook, oldExposure, oldGamma)
    
    matName = SafeName(material.name)
    bakeNode.image.save(filepath=folderDir + "textures/" + matName + ("_nrm" if bakeNormal else "_rgh") + ".png")
    
    bpy.data.images.remove(bpy.data.images['TextureBake'])
    material.node_tree.nodes.remove(bakeNode)
    
    renderPlane.select_set(False)
    
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



def PackVector(v):
    bits = bytearray(struct.pack("f", v.x))
    bits += bytearray(struct.pack("f", v.y))
    return bits + bytearray(struct.pack("f", v.z))

def PackUV(u, v):
    return bytearray(struct.pack("f", u[0])) + bytearray(struct.pack("f", 1-u[1])) + bytearray(struct.pack("f", v[0])) + bytearray(struct.pack("f", 1-v[1]))
    
def PackVertex(vert, nrm, tng, uv1, uv2):
    return PackVector(vert.co) + PackVector(nrm) + PackVector(tng) + PackUV(uv1, uv2)

def UnwrapLightmap(object, smartProject, allInOne):
    data = object.data
    if len(data.uv_layers) < 2:
        data.uv_layers.new()

    oldActiveObjects = bpy.context.view_layer.objects.active
    if not allInOne:
        bpy.context.view_layer.objects.active = object

    bpy.ops.object.editmode_toggle()
        
    bpy.ops.mesh.select_all(action='SELECT')

    if smartProject:
        bpy.ops.uv.smart_project(island_margin=0.001, margin_method='FRACTION', scale_to_bounds=True)
    else:
        bpy.ops.uv.lightmap_pack(PREF_PACK_IN_ONE=allInOne, PREF_MARGIN_DIV=0.1)

    bpy.ops.object.editmode_toggle()
    
    bpy.context.view_layer.objects.active = oldActiveObjects
    
def Width(mn, mx):
    return (mx - mn) / 2
    

def GetBoundingBox(data):
    mn = [999999, 999999, 999999]
    mx = [-999999, -999999, -999999]
    for i in data.vertices:
        mn[0] = min(mn[0], i.co.x)
        mn[1] = min(mn[1], i.co.y)
        mn[2] = min(mn[2], i.co.z)
        mx[0] = max(mx[0], i.co.x)
        mx[1] = max(mx[1], i.co.y)
        mx[2] = max(mx[2], i.co.z)
    
    return (Width(mn[0], mx[0]), Width(mn[1], mx[1]), Width(mn[2], mx[2]))

class Meshel():
    def __init__(self, material):
        self.material = material
        self.vertices = bytearray([])
        self.vertbits = []
        self.numVerts = 0
        self.numIndices = 0
        self.indices = []

def SafeName(name):
    return name.replace(".", "_")

def MeshFileExists(object):
    for dex, mat in enumerate(object.data.materials):
        if not mat:
            mat = object.material_slots[dex].material
        if not os.path.isfile(f"{folderDir}models/{SafeName(object.data.name)}_{dex}.msh"):
            return False
    return True

def ConvertMeshToVulkanFile(object, unwrap, skipExisting):
    data = object.data
    
    if len(data.uv_layers) < 2:
        raise RuntimeError(f"Mesh: {data.name} does not have lightmap UVs!")
    
    if unwrap:
        UnwrapLightmap(object, True)
    
    if MeshFileExists(object) and skipExisting and not unwrap: return
    
    data.calc_tangents()
    
    meshels = []
    
    wm.progress_begin(0, len(data.polygons))
    
    progress = 0


    for i in data.polygons:
        wm.progress_update(progress)
        progress += 1

        while len(meshels) <= i.material_index:
            mat = data.materials[len(meshels)]
            if not mat:
                mat = object.material_slots[len(meshels)].material
            meshels.append(Meshel(mat))
        
        meshel = meshels[i.material_index]
        meshel.numIndices += len(i.vertices)

        for v, l in zip(i.vertices, i.loop_indices):
            vbits = PackVertex(data.vertices[v], data.vertices[v].normal if i.use_smooth else i.normal, data.loops[l].tangent, data.uv_layers[0].data[l].uv, data.uv_layers[1].data[l].uv)
            if True:
            #try:
                #meshel.indices.append(meshel.vertbits.index(vbits))
            #except:
                meshel.vertbits.append(vbits)
                meshel.numVerts += 1
                meshel.vertices += vbits
                meshel.indices.append(meshel.numVerts - 1)
    
    for dex, m in enumerate(meshels):
        with open(f"{folderDir}models/{SafeName(data.name)}_{dex}.msh", "wb") as meshFile:

            dataType = "H" if len(m.indices) < 65535 else "I"
            meshFile.write(struct.pack("B", 1 if dataType == "I" else 0))
            meshFile.write(bytearray(struct.pack(dataType, m.numVerts)))
            meshFile.write(m.vertices)
            meshFile.write(bytearray(struct.pack(dataType, m.numIndices)))
            for i in m.indices:
                meshFile.write(struct.pack(dataType, i))
            aabb = GetBoundingBox(data)
            meshFile.write(struct.pack("f", aabb[0]))
            meshFile.write(struct.pack("f", aabb[1]))
            meshFile.write(struct.pack("f", aabb[2]))
    
    wm.progress_end()
    
    data.free_tangents()

def ExportBone(bone, file):
    file.write(PackVector())
    file.write(struct.pack("I", len(bone.children)))
    for c in bone.children:
        ExportBone(c, file)

def PackArray(type, data):
    return bytearray(struct.pack(type, data))

def PackQuat(quat):
    bits = bytearray([])
    for i in range(4):
        bits += PackArray("f", quat[i])
    return bits

def ExportSkeleton(object, file):
    root = object.pose.bones[0]
    file.write(PackVector(root.location))
    file.write(PackQuat(root.rotation_quaternion))
    file.write(PackVector(root.scale))

    file.write(struct.pack("I", len(object.pose.bones) - 1))
    for b in object.pose.bones[1:]:
        file.write(PackQuat(b.rotation_quaternion))
        file.write(PackVector(b.scale))

def ExportSkeletalMesh(object):
    data = object.data
    ConvertMeshToVulkanFile(object, False)

    with open(folderDir + "skeletalmeshes/skeleton.skl", "wb") as file:
        ExportSkeleton(object.parent, file)

    with open(folderDir + "skeletalmeshes/skeletalmesh.sklmsh", "wb") as file:
        file.write(struct.pack("H", len(data.vertices[0].groups)))
        weights = bytearray([])
        vertbits = []
        numVerts = 0
        for i in data.polygons:
            for v, l in zip(i.vertices, i.loop_indices):
                vbits = PackVertex(data.vertices[v], data.vertices[v].normal if i.use_smooth else i.normal, data.loops[l].tangent, data.uv_layers[0].data[l].uv, data.uv_layers[1].data[l].uv)
                if vbits not in vertbits:
                    vertbits.append(vbits)
                    numVerts += 1
                    for g in data.vertices[v].groups:
                        weights += struct.pack("f", g.weight)
        file.write(struct.pack("H", numVerts))
        file.write(weights)


def ExportSkeletalAnimation(armature):
    bones = armature.data.bones
    action = armature.animation_data.action
    scene = bpy.context.scene
    
    ranges = [marker.frame for marker in scene.timeline_markers]
    
    ranges.append(scene.frame_end)

    with open(folderDir + "skeletalmeshes/skeleton.sklanm", "wb") as file:
        for anim in range(len(ranges) - 1):
            for frame in range(ranges[anim], ranges[anim + 1]):
                scene.frame_current = frame
                ExportSkeleton(armature, file)
        scene.frame_current = scene.frame_end
        ExportSkeleton(armature, file)

    
class TestOperator(bpy.types.Operator):
    """An operator used to test things"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.testoperator"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Test Operator"         # Display name in the interface.
    
    toFind: bpy.props.StringProperty(name="Filename to look for")

    def execute(self, context):
        print("== Images ==")
        for i in bpy.data.images:
            if i.filepath == self.toFind:
                print(i.name)
        print("== Done ==")

        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class MaterialReplaceOperator(bpy.types.Operator):
    """Replaces all given materials on selected objects with another material"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.replace_material"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Mass Replace Materials"         # Display name in the interface.
    
    toFind: bpy.props.StringProperty(name="Material(s) to Find (split by commas)")
    replaceWith: bpy.props.StringProperty(name="Material to Replace With")

    def execute(self, context):
        searchMat = []
        for i in self.toFind.split(","):
            searchMat.append(bpy.data.materials[i])
        replaceMat = bpy.data.materials[self.replaceWith]
        for i in context.selected_objects:
            if i.type != 'MESH': continue
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    if i.material_slots[dex].material in searchMat:
                        i.material_slots[dex].material = replaceMat
                else:
                    if mat in searchMat:
                        i.data.materials[dex] = replaceMat

        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class MaterialFindOperator(bpy.types.Operator):
    """Selects all objects that use a specific material"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.find_material"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Find Objects of Material"         # Display name in the interface.
    
    toFind: bpy.props.StringProperty(name="Name of Material to Find")
    exact: bpy.props.BoolProperty(name="Exact match")

    def execute(self, context):
        if self.exact:
            searchMat = bpy.data.materials[self.toFind]

        selected = context.selected_objects
        bpy.ops.object.select_all(action='DESELECT')

        for i in selected:
            if i.type != 'MESH': continue
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material

                if self.exact:
                    if mat == searchMat:
                        i.select_set(True)
                else:
                    if self.toFind in mat.name:
                        i.select_set(True)

        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class ExportSkeletalMeshOperator(bpy.types.Operator):
    """Exports the selected mesh and its armature parent"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.exportskelmsh"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Export Skeletal Mesh"         # Display name in the interface.
    
    #unwrap: bpy.props.BoolProperty(name="Unwrap Lightmap")

    def execute(self, context):
        ExportSkeletalMesh(context.selected_objects[0])
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class ExportSkeletalMeshAnimationOperator(bpy.types.Operator):
    """Exports all animations for the selected armature"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.exportskelanim"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Export Skeletal Mesh Animation"         # Display name in the interface.
    
    #unwrap: bpy.props.BoolProperty(name="Unwrap Lightmap")

    def execute(self, context):
        ExportSkeletalAnimation(context.selected_objects[0])
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class SetPassIndexOperator(bpy.types.Operator):
    """Automatically sets the pass index (which decides the object's id in the engine) for all selected objects"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.set_object_id"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Set Object Id"         # Display name in the interface.
    
    id: bpy.props.IntProperty(name="Object ID")

    def execute(self, context):
        for i in bpy.context.selected_objects:
            if i.type != 'MESH': continue
            i.pass_index = self.id
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class ExportMesh(bpy.types.Operator):
    """Writes out the model in C++ for Vulkan"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "frameblender.blend_frames"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Export Mesh (Vulkan)"         # Display name in the interface.
    
    unwrap: bpy.props.BoolProperty(name="Unwrap Lightmap")
    skipExisting: bpy.props.BoolProperty(name="Skip Existing Meshes")

    def execute(self, context):
        selected = bpy.context.selected_objects
        for object in selected:
            if object.type != 'MESH': continue
            ConvertMeshToVulkanFile(object, self.unwrap, self.skipExisting)
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class SetUVActiveOperator(bpy.types.Operator):
    """Sets the active UV map for all selected objects, to perform unwrapping on"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.set_active_uv"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Set Active UV (Vulkan)"         # Display name in the interface.
    
    UVIndex: bpy.props.IntProperty(name="UVIndex")

    def execute(self, context):
        selected = bpy.context.selected_objects
        for object in selected:
            if object.type != 'MESH': continue
            object.data.uv_layers[self.UVIndex].active = True
            object.data.uv_layers.active_index = self.UVIndex
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class UnwrapLightmapOperator(bpy.types.Operator):
    """Unwraps a mesh's lightmap automatically"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.unwraplightmap"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Unwrap Lightmap (Vulkan)"         # Display name in the interface.
    
    smartProject: bpy.props.BoolProperty(name="Use Smart Project")
    allInOne: bpy.props.BoolProperty(name="All in One")

    def execute(self, context):
        selected = bpy.context.selected_objects
        

        for object in selected:
            if object.type != 'MESH': continue
        
            if len(object.data.uv_layers) < 2:
                object.data.uv_layers.new()

            object.data.uv_layers[1].active = True
            object.data.uv_layers.active_index = 1
            

            
        if self.allInOne:
            UnwrapLightmap(selected[0], self.smartProject, self.allInOne)
        else:
            bpy.ops.object.select_all(action='DESELECT')

            wm.progress_begin(0, len(selected))

            for dex, object in enumerate(selected):
                wm.progress_update(dex)
                if object.type != 'MESH': continue

                object.select_set(True)
                UnwrapLightmap(object, self.smartProject, self.allInOne)
                object.select_set(False)
            wm.progress_end()

        for object in selected:
            if object.type != 'MESH': continue

            object.data.uv_layers[0].active = True
            object.data.uv_layers.active_index = 0

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
            matName = object.data.materials[0].name
            if matName.startswith("SDM_") or matName.startswith("DM_"):
                object.data.uv_layers.remove(object.data.uv_layers[0])
                UnwrapLightmap(object.data)

        return {'FINISHED'}

def BakeLighting(obj, resolution, levelName):

    obj.select_set(True)
    oldmats = []
    for dex, mat in enumerate(obj.data.materials):
        isData = True
        if not mat:
            isData = False
            mat = obj.material_slots[dex].material
        oldmats.append([mat, isData])
        if isData:
            obj.data.materials[dex] = bpy.data.materials['AOBake']
        else:
            obj.material_slots[dex].material = bpy.data.materials['AOBake']
    
    obj.data.uv_layers[1].active = True
    
    texScale = 1.0
    '''
    if LinkedToNormal(oldmats[i].node_tree.nodes['Principled BSDF'].inputs[5]):
        tex = GetTextureMap(obj.data, 5)
        nrmMap = tex.image
        if tex.inputs[0].is_linked:
            texScale = tex.inputs[0].links[0].from_node.inputs[3].default_value[0]
    else:
        nrmMap = bpy.data.images['default_white_nrm.png']
    '''
    #nrmMap = oldmat.node_tree.nodes['Principled BSDF'].inputs[5].links[0].from_node.image if (oldmat.node_tree.nodes['Principled BSDF'].inputs[5].is_linked) else bpy.data.images['default_white_nrm.png']
    
    #obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[0] = texScale
    #obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[1] = texScale
    #obj.data.materials[0].node_tree.nodes['Mapping'].inputs[3].default_value[2] = texScale
    
    #obj.data.materials['AOBake'].node_tree.nodes['normal'].image = nrmMap
    
    activeImage = bpy.data.materials['AOBake'].node_tree.nodes['Image Texture'].image
    activeImage.scale(resolution, resolution)
    
    if obj.hide_render:
        obj.hide_render = False
        bpy.ops.object.bake(type='AO')
        obj.hide_render = True
    else:
        bpy.ops.object.bake(type='COMBINED', pass_filter={'DIRECT', 'INDIRECT', 'DIFFUSE'})
    
    activeImage.save_render(folderDir + 'levels/' + levelName + '/textures/' + SafeName(obj.name) + "_shadowmap.png", quality=100)

    obj.data.uv_layers[0].active = True
    obj.select_set(False)
    for i in range(len(oldmats)):
        if oldmats[i][1]:
            obj.data.materials[i] = oldmats[i][0]
        else:
            obj.material_slots[i].material = oldmats[i][0]

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
        bpy.ops.image.open(filepath=folderDir + 'textures/' + filename)
        renderer.data.materials[0].node_tree.nodes['skybox'].image = bpy.data.images[filename]
                        
        bpy.ops.object.bake(type='EMIT')
        renderer.data.materials[0].node_tree.nodes['skyboxbake'].image.save(filepath=folderDir + 'textures/' + filename[:-4] + "_" + side + ".png")
        renderer.select_set(False)

def RenderAndSaveCubemap(filename):
    for side in ['Back', 'Down', 'Front', 'Left', 'Right', 'Up']:
        renderer = bpy.data.objects['CubemapCreator_' + side]
        renderer.hide_render = True

    bpy.ops.render.render(use_viewport=True)
    bpy.data.images['Render Result'].save_render(folderDir + 'textures/' + filename)
    
    SaveCubemap(filename)

def BakeGI(levelname):
    RenderAndSaveCubemap(levelname + ".png")

    '''
    camera = bpy.data.objects['CubemapCreator']
    probe = bpy.data.objects['IrradianceVolume']

    resX = probe.data.resolution_x
    resY = probe.data.resolution_y
    resZ = probe.data.resolution_z
    
    vX = vY = vZ = 0
    
    out = "static GICapture " + levelname + "[] = {\n"
    
    renderer = bpy.data.objects['cubemaprenderer']

    for x in range(resX):
        for y in range(resY):
            for z in range(resZ):
                
                (camera.location.x, camera.location.y, camera.location.z) = GetGIProbeLocation(x, y, z)
                
                RenderAndSaveCubemap(f"GI_{x}_{y}_{z}.png")
    
    for obj in bpy.data.objects:
        if obj.type == 'LIGHT_PROBE' and obj.data.type == 'CUBEMAP':
            camera.location = obj.location
            
            filename = obj.name.replace(".", "_") + ".png"
            RenderAndSaveCubemap(filename)
    '''
                
    
class BakeLightingOperator(bpy.types.Operator):
    """Bakes the shadow maps for all selected objects"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.bakelighting"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Bake Lighting (Vulkan)"         # Display name in the interface.
    
    shadowmapsize: bpy.props.IntProperty(name="Specific Map Resolution")
    calcMapSize: bpy.props.BoolProperty(name="Calculate Map Resolution")
    levelname: bpy.props.StringProperty(name="Level Name")
    
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
                BakeLighting(i, self.shadowmapsize, self.levelname)
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
        with open(folderDir + f"materials/{zlsl}.mat", "x") as matFile:
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

class TempObj:
    def __init__(self):
        self.meshIndex = -1
        self.position = []
        self.rotation = []
        self.scale = []
        self.texScale = 0.0
        self.materials = []
        self.isStatic = False
        self.castShadows = True
        self.id = 0
        self.shadowMap = 0
        self.luaScript = -1

class TempMat:
    def __init__(self):
        self.shaderIndex = 0
        self.textures = []
        self.roughness = 0.0


def GetShaderIndex(node):
    additional = 4 if node.inputs[4].is_linked else 0
    if node.inputs[1].default_value > 0.5:
        return 1 + additional
    if node.inputs[18].default_value > 0.5:
        return 2
    return 0 + additional
    

class ConvertLevel(bpy.types.Operator):
    """Exports a level to the vulkan engine, also copies over all textures used in the level to the level folder"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.convertlevel"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Convert Level (Vulkan)"         # Display name in the interface.
    
    #my_float: bpy.props.FloatProperty(name="Some Floating Point")
    includeshmaps: bpy.props.BoolProperty(name="Bake Shadow Maps")
    shadowmapsize: bpy.props.IntProperty(name="Shadow Map Resolution")

    bakegi: bpy.props.BoolProperty(name="Bake GI")

    levelname: bpy.props.StringProperty(name="Level Name")
    
    unwrap: bpy.props.BoolProperty(name="Unwrap Lightmaps")
    
    exportMeshes: bpy.props.BoolProperty(name="Export Meshes")

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
        
        if not self.levelname:
            raise RuntimeError("Level name was not given!")
        
        writtenMeshes = []
        
        writtenMaterials = []
        
        lvlStrings = bytearray([])
        lvlMats = bytearray([])
        MeshStringLocs = []
        tempMats = []
        tempObjs = []
        customShaders = []
        
        sunInfo = False
        spotLights = []
            
        allobjects = bpy.context.selected_objects
        
        if not os.path.isdir(f"{folderDir}levels/{self.levelname}/textures"):
            os.makedirs(f"{folderDir}levels/{self.levelname}/textures")
        
        bpy.ops.object.select_all(action='DESELECT')
        
        wm.progress_begin(0, len(allobjects))
        
        def pack(type, data):
            return struct.pack(type, data)
            
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

        def SaveImage(image, tempMat):
            imageFilename = image.filepath[image.filepath.rindex("\\" if "\\" in image.filepath else "/") + 1:]
            fullImageFilename = "levels/" + levelname + "/textures/" + imageFilename
            image.save(filepath=folderDir + fullImageFilename, quality=80)
            tempMat.textures.append([AddLvlString(fullImageFilename), image.colorspace_settings.name == 'sRGB'])
        
        def SaveInputIfLinked(tempMat, mat, levelname, index, extension, isCol):
            if mat.node_tree.nodes['Principled BSDF'].inputs[index].is_linked:
                try:
                    image = mat.node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.image
                except:
                    if not mat.node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.inputs[1].is_linked:
                        print("Not linked")
                        tempMat.textures.append([AddLvlString("textures/default_white" + extension + ".png"), isCol])
                        return
                    image = mat.node_tree.nodes['Principled BSDF'].inputs[index].links[0].from_node.inputs[1].links[0].from_node.image
                
                if not image:
                    print("Texture has no data")
                    tempMat.textures.append([AddLvlString("textures/default_white" + extension + ".png"), isCol])
                    return

                if max(image.size) > 1024:
                    image.scale(1024, 1024)
                
                SaveImage(image, tempMat)
            else:
                tempMat.textures.append([AddLvlString("textures/default_white" + extension + ".png"), isCol])
        
        
        for i in allobjects:
            wm.progress_update(progress)
            progress += 1
            
            i.select_set(True)

            if i.type == 'LIGHT' and i.hide_render:
                if i.data.type == 'SUN':
                    sunInfo = PackVector(i.rotation_euler)
                elif i.data.type == 'SPOT':
                    spotLights.append(PackVector(i.location) + PackVector(i.rotation_euler) + pack('f', i.data.spot_size * 1.415))
            
            if i.type != 'MESH':
                i.select_set(False)
                continue
        
            if not i.data.materials:
                raise SyntaxError(i.name + " does not have any materials")
            
            if i.data.name not in writtenMeshes:
                if self.exportMeshes:
                    
                    ConvertMeshToVulkanFile(i, self.unwrap)
                    
                MeshStringLocs.append([])
                for dex, mat in enumerate(i.data.materials):
                    if not mat:
                        mat = i.material_slots[dex].material
                    if not mat:
                        raise RuntimeError(i.name + " has no material at index " + str(dex))

                MeshStringLocs[-1].append(StringToBytes(f"{SafeName(i.data.name)}"))
                writtenMeshes.append(i.data.name)
                
            meshIndex = writtenMeshes.index(i.data.name)
            
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material

                if mat.name not in writtenMaterials:
                    numTextures = 0
                    tempMat = TempMat()
                    tempMat.roughness = convert(mat.node_tree.nodes['Principled BSDF'].inputs[2].default_value)
                    
                    bsdfNode = mat.node_tree.nodes['Principled BSDF']
 
                    label = bsdfNode.label
                    if label:
                        if label not in customShaders:
                            ExportMaterial(mat)
                            customShaders.append(label)

                        tempMat.shaderIndex = customShaders.index(label) + 6
                        
                        textureNodes = []
                        for node in mat.node_tree.nodes:
                            if node.type == 'TEX_IMAGE':
                                textureNodes.append(node)
                        
                        def TextureSort(x):
                            return x.location.y
                        
                        textureNodes.sort(key=TextureSort)
                        
                        for node in textureNodes:
                            SaveImage(node.image, tempMat)
                        
                    else:
                        if mat.name.startswith("skybox_"):
                            tempMat.shaderIndex = 3
                        else:
                            tempMat.shaderIndex = GetShaderIndex(mat.node_tree.nodes['Principled BSDF'])

                        SaveInputIfLinked(tempMat, mat, self.levelname, 0, "_col", True)
                        SaveInputIfLinked(tempMat, mat, self.levelname, 2, "_rgh", False)
                        SaveInputIfLinked(tempMat, mat, self.levelname, 5, "_nrm", False)
                        tempMats.append(tempMat)

                    writtenMaterials.append(mat.name)
            
            shadowMapFilename = "textures/default_shadowmap.png"
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material
                
                if mat.node_tree.nodes["Principled BSDF"].inputs[28].default_value == 0.0:
                    if self.includeshmaps:
                        BakeLighting(i, self.shadowmapsize, self.levelname)
                    shadowMapFilename = f"levels/{self.levelname}/textures/{SafeName(i.name)}_shadowmap.png"
                    break
            
            texScale = 1.0

            def PackFloat(y):
                return bytearray(struct.pack("f", y))

            def PackListVec(x):
                return PackFloat(x[0]) + PackFloat(x[1]) + PackFloat(x[2])
            
            def PackRot(x):
                return PackFloat(x[0]) + PackFloat(x[1]) + PackFloat(x[2])

            tempObj = TempObj()
            tempObj.meshIndex = meshIndex
            tempObj.position = PackListVec(i.location)
            tempObj.rotation = PackRot(i.rotation_euler)
            tempObj.scale = PackListVec(i.scale)
            tempObj.texScale = texScale
            
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material
                tempObj.materials.append(writtenMaterials.index(mat.name))
            
            tempObj.isStatic = not i.hide_render
            tempObj.castShadows = i.display.show_shadows
            tempObj.id = i.pass_index
            tempObj.shadowMap = AddLvlString(shadowMapFilename)
            
            
            if "lua" in i:
                scriptPath = f"scripts/{i['lua']}.lua"

                # Creating a template script (if it doesn't already exist)
                try:
                    with open(scriptPath, "x") as scriptFile:
                        scriptFile.write("-- 'this' is the thing belonging to this script\n-- Both functions are optional, you can remove either one if it's not needed\n\nfunction Spawn(this)\nend\n\nfunction Tick(this)\nend")
                except:
                    pass

                tempObj.luaScript = AddLvlString(scriptPath)

            tempObjs.append(tempObj)

            i.select_set(False)

        wm.progress_end()
            
        fileLength = (len(tempMats) * 6) + 2
        fileLength += sum([len(string) for item in MeshStringLocs for string in item]) + 2
        fileLength += (len(tempObjs) * 66) + 2
        for i in tempObjs:
            fileLength += len(i.materials) * 2

        for i in tempMats:
            fileLength += len(i.textures) * 5

        fileLength += len(customShaders) * 4 + 2
        fileLength += 1
        if sunInfo: fileLength += 4 * 3
        fileLength += len(spotLights) * (4 * 7) + 2
        
        print("Writing", len(tempObjs), "objects")
        
        with open(folderDir + f"levels/{self.levelname}/{self.levelname}.lvl", "wb") as file:
            
            file.write(pack('?', sunInfo))
            if sunInfo:
                file.write(sunInfo)
            
            file.write(pack('H', len(spotLights)))
            for i in spotLights:
                file.write(i)
            
            file.write(struct.pack("H", len(customShaders)))
            for i in customShaders:
                file.write(struct.pack("I", AddLvlString(i) + fileLength))
            
            file.write(struct.pack("H", len(tempMats)))
            for i in tempMats:
                file.write(struct.pack("B", i.shaderIndex))
                file.write(struct.pack("B", len(i.textures)))
                for tex in i.textures:
                    file.write(struct.pack("I", tex[0] + fileLength))
                    file.write(struct.pack("?", tex[1]))
                file.write(struct.pack("f", i.roughness))
            
            file.write(struct.pack("H", len(MeshStringLocs)))
            for i in MeshStringLocs:
                for mesh in i:
                    file.write(mesh)
            
            file.write(struct.pack("H", len(tempObjs)))
            for i in tempObjs:
                file.write(struct.pack("H", i.meshIndex))
                file.write(i.position)
                file.write(i.rotation)
                file.write(i.scale)
                file.write(struct.pack("f", i.texScale))
                file.write(struct.pack("B", len(i.materials)))
                for mat in i.materials:
                    file.write(struct.pack("H", mat))
                file.write(struct.pack("?", i.isStatic))
                file.write(struct.pack("?", i.castShadows))
                file.write(struct.pack("B", i.id))
                file.write(struct.pack("I", i.shadowMap + fileLength))
                file.write(struct.pack("I", (i.luaScript + fileLength) if i.luaScript >= 0 else 0))
                file.write(struct.pack("f", 0)) # Shadow Map Offset X
                file.write(struct.pack("f", 0)) # Shadow Map Offset Y
                file.write(struct.pack("f", 0)) # Shadow Map Scale

            file.write(lvlStrings)
        
        return {'FINISHED'}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
def BlendButton(self, context):
    layout = self.layout
    layout.operator("frameblender.blend_frames")
    
buttons = [
    (ConvertLevel, "convertlevel"),
    (BakeLightingOperator, "bakelighting"),
    (BakeGIOperator, "bakegi"),
    (UnwrapLightmapOperator, "unwraplightmap"),
    (SetPassIndexOperator, "set_object_id"),
    (FixFO2UVOperator, "fo2uvs"),
    (SetUVActiveOperator, "set_active_uv"),
    (MaterialReplaceOperator, "replace_material"),
    (MaterialFindOperator, "find_material"),
    (TestOperator, "testoperator")
]

buttonFuncs = []

def MakeButtonFunc(name):
    return lambda self, context : self.layout.operator("vulkan_utils." + name)
    
def RegisterStuff():
    for b in buttons:
        bpy.utils.register_class(b[0])
        buttonFuncs.append(MakeButtonFunc(b[1]))
        bpy.types.VIEW3D_MT_object.append(buttonFuncs[-1])

def UnRegisterStuff():
    for b in buttons:
        bpy.utils.unregister_class(b[0])
        bpy.types.VIEW3D_MT_object.pop()
  
def register():
    bpy.utils.register_class(ExportMesh)
    bpy.types.VIEW3D_MT_object.append(BlendButton)
    RegisterStuff()
    
def unregister():
    UnRegisterStuff()
    bpy.types.VIEW3D_MT_object.pop()
    bpy.utils.unregister_class(ExportMesh)
    
    
if __name__ == "__main__":
    register()