bl_info = {
    "name": "Last-Gen Engine Level Editor",
    "blender": (4, 50, 3),
    "category": "Editor",
    "author": "Zack"
}



import bpy
import os
import bmesh
from math import floor
import struct

wm = bpy.context.window_manager

folderDir = bpy.path.abspath("//x64/Release/")



# ====== Utils ======

def SafeName(name):
    return name.replace(".", "_").replace(" ", "_")

def convert(x):
    return round(x, 4)

def GetLevelNameFromObject(object):
    # There's no built in way to get the root collection from an object, so here we go
    c = object.users_collection[0]

    while True:
        for col in bpy.data.collections:
            if c.name in col.children:
                c = col
                continue
        break

    return SafeName(c.name)

def MeshFileExists(object):
    for dex, mat in enumerate(object.data.materials):
        if not os.path.isfile(f"{folderDir}models/{SafeName(object.data.name)}_{dex}.msh"):
            return False

    return True

def PackVector(v):
    bits = bytearray(struct.pack("f", v.x))
    bits += bytearray(struct.pack("f", v.y))
    return bits + bytearray(struct.pack("f", v.z))

def PackUV(u, v):
    return bytearray(struct.pack("f", u[0])) + bytearray(struct.pack("f", 1-u[1])) + bytearray(struct.pack("f", v[0])) + bytearray(struct.pack("f", 1-v[1]))
    
def PackVertex(vert, nrm, tng, uv1, uv2):
    return PackVector(vert.co) + PackVector(nrm) + PackVector(tng) + PackUV(uv1, uv2)

def PackVectorList(v):
    return struct.pack('f', v[0]) + struct.pack('f', v[1]) + struct.pack('f', v[2])

def PackArray(type, data):
    return bytearray(struct.pack(type, data))

def PackQuat(quat):
    bits = bytearray([])
    for i in range(4):
        bits += PackArray("f", quat[i])
    return bits

def Width(mn, mx):
    return (mx - mn) / 2

def AddPoint(x, y, z):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=8, radius=0.08, enter_editmode=False, align='WORLD', location=(x, y, z), scale=(1, 1, 1))




# ====== Custom Property Stuff ======

class LGESettingsNames():
    def __init__(self):
        self.castsShadows = "Cast Dynamic Shadows (LGE)"
        self.id = "Object ID (LGE)"
        self.script = "Lua Script (LGE)"
        self.collision = "Enable Collision (LGE)"
        self.complexCollision = "Per-Triangle Collision (LGE)"
        self.shadowSize = "Shadow Map Size (LGE)"
        self.forceStatic = "Force isStatic (LGE)"
        self.globalName = "Name In Lua (LGE)"
        self.ignore = "Ignore (LGE)"

LGE = LGESettingsNames()


def AddCustomProperty(object, property, desc, value):
    if property not in object:
        object[property] = value
        ui = object.id_properties_ui(property)
        ui.update(description=desc, default=value)
        
def RemoveCustomProperty(object, property):
    if property in object:
        del object[property]

def SetUpCustomProperties():
    if True:
        for i in bpy.data.objects:
            match i.type:
                case 'MESH':
                    AddCustomProperty(i, LGE.castsShadows, "Whether or not to include this object when drawing shadow maps from dynamic lights", True)
                    AddCustomProperty(i, LGE.id, "The ID of this Thing, determines which Things are returned when calling GetThingsById()", 0)
                    AddCustomProperty(i, LGE.script, "If blank, does not run its own lua code. The '.lua' extension is automatically added, and it automatically searches in the scripts folder, so just put the name of the lua file", "")
                    AddCustomProperty(i, LGE.collision, "", False)
                    AddCustomProperty(i, LGE.complexCollision, "If false, uses the bounding box for collision", False)
                    AddCustomProperty(i, LGE.shadowSize, "The resolution of the shadow map when using the BakeLighting operator. 0 means to use the size given when running the operator", 0)
                    AddCustomProperty(i, LGE.forceStatic, "The options are 'default', 'static', or 'dynamic'. With this you can force an object to either be static or dynamic, regardless of the hide_render flag", "default")
                    AddCustomProperty(i, LGE.globalName, "If not blank, this object will be accesible in lua by this name, so you don't have to find it with GetThingsById()", "")
                    AddCustomProperty(i, LGE.ignore, "If true, don't include this object in the level. This can also be done by turning off both visibility (eye icon), and the render flag (camera icon)", False)
                case 'LIGHT':
                    AddCustomProperty(i, LGE.forceStatic, "The options are 'default', 'static', or 'dynamic'. With this you can force a light to either be static or dynamic, regardless of the hide_render flag (camera icon)", "default")
                    AddCustomProperty(i, LGE.globalName, "If not blank, this light will be accesible in lua by this name", "")
                    AddCustomProperty(i, LGE.ignore, "If true, don't include this light in the level. This can also be done by setting it to static or enabling the render flag (camera icon)", False)
        
    else:
        # For testing, I have the code for removing the custom properties
        for i in bpy.data.objects:
                RemoveCustomProperty(i, LGE.castsShadows)
                RemoveCustomProperty(i, LGE.id)
                RemoveCustomProperty(i, LGE.script)
                RemoveCustomProperty(i, LGE.collision)
                RemoveCustomProperty(i, LGE.complexCollision)
                RemoveCustomProperty(i, LGE.shadowSize)
                RemoveCustomProperty(i, LGE.forceStatic)
                RemoveCustomProperty(i, LGE.globalName)
                RemoveCustomProperty(i, LGE.ignore)


SetUpCustomProperties()

class SetUpCustomPropertiesOperator(bpy.types.Operator):
    """Sets up the needed custom properties on each object if it doesn't already have them"""
    bl_idname = "vulkan_utils.setupproperties"
    bl_label = "Set Up Custom Properties"
    
    def execute(self, context):
        SetUpCustomProperties()
        return {'FINISHED'}





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



# ====== Texture Baking ======

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

class Mexel():
    def __init__(self, material):
        self.material = material
        self.vertices = bytearray([])
        self.vertbits = []
        self.numVerts = 0
        self.numIndices = 0
        self.indices = []

def ConvertMeshToVulkanFile(object, unwrap, skipExisting, pathoverride=""):
    data = object.data
    
    if unwrap:
        UnwrapLightmap(object, True, False)
    
    if len(data.uv_layers) < 2:
        raise RuntimeError(f"Mesh: {data.name} does not have lightmap UVs!")
    
    if MeshFileExists(object) and skipExisting and not unwrap: return

    meshName = data.name
    
    oldActiveObject = bpy.context.active_object
    
    with bpy.context.temp_override(active_object=object):
        depsgraph = bpy.context.evaluated_depsgraph_get()
        appliedmodifiers = object.evaluated_get(depsgraph)
    
    data = bpy.data.meshes.new_from_object(appliedmodifiers)

    bm = bmesh.new()
    bm.from_mesh(data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    
    bm.to_mesh(data)
    
    bm.free()
    
    data.calc_tangents()
    
    mexels = []
    
    wm.progress_begin(0, len(data.polygons))
    
    progress = 0


    for i in data.polygons:
        wm.progress_update(progress)
        progress += 1

        while len(mexels) <= i.material_index:
            mat = data.materials[len(mexels)]
            if not mat:
                mat = object.material_slots[len(mexels)].material
            mexels.append(Mexel(mat))
        
        mexel = mexels[i.material_index]
        mexel.numIndices += len(i.vertices)

        for v, l in zip(i.vertices, i.loop_indices):
            vbits = PackVertex(data.vertices[v], data.vertices[v].normal if i.use_smooth else i.normal, data.loops[l].tangent, data.uv_layers[0].data[l].uv, data.uv_layers[1].data[l].uv)
            if True:
            #try:
                #mexel.indices.append(mexel.vertbits.index(vbits))
            #except:
                mexel.vertbits.append(vbits)
                mexel.numVerts += 1
                mexel.vertices += vbits
                mexel.indices.append(mexel.numVerts - 1)
    
    for dex, m in enumerate(mexels):
        with open(pathoverride if pathoverride else f"{folderDir}models/{SafeName(meshName)}_{dex}.msh", "wb") as meshFile:

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
    bpy.data.meshes.remove(data)

def ExportBone(bone, file):
    file.write(PackVector())
    file.write(struct.pack("I", len(bone.children)))
    for c in bone.children:
        ExportBone(c, file)

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
    
    #toFind: bpy.props.StringProperty(name="Filename to look for")

    def execute(self, context):
        dTextureName = "LGE_LightingBakeTexture"
        eTextureName = "LGE_EmitBakeTexture"
        
        object = context.selected_objects[0]
        object.data.uv_layers[1].active = True
        
        if dTextureName in bpy.data.images:
            bpy.data.images.remove(bpy.data.images[dTextureName])
        
        if eTextureName in bpy.data.images:
            bpy.data.images.remove(bpy.data.images[eTextureName])
        
        bpy.ops.image.new(name=dTextureName, width=256, height=256, float=True)
        bpy.ops.image.new(name=eTextureName, width=256, height=256, float=True)
        
        imageNodes = []
        
        for dex, mat in enumerate(object.data.materials):
            if not mat:
                mat = object.material_slots[dex].material
            
            bakeNode = mat.node_tree.nodes.new('ShaderNodeTexImage')
            bakeNode.image = bpy.data.images[dTextureName]
            mat.node_tree.nodes.active = bakeNode
            imageNodes.append(bakeNode)
        
        if object.hide_render:
            object.hide_render = False
            bpy.ops.object.bake(type='AO')
            object.hide_render = True
        else:
            bpy.ops.object.bake(type='DIFFUSE', pass_filter={'DIRECT', 'INDIRECT'})
        
        for i in imageNodes:
            i.image = bpy.data.images[eTextureName]
        
        bpy.ops.object.bake(type='EMIT')
        
        for dex, pixel in enumerate(bpy.data.images[eTextureName].pixels):
            bpy.data.images[dTextureName].pixels[dex] += pixel
        
        for dex, mat in enumerate(object.data.materials):
            if not mat:
                mat = object.material_slots[dex].material
            
            mat.node_tree.nodes.remove(imageNodes[dex])
        
        object.data.uv_layers[0].active = True

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
        SetUpCustomProperties()

        for i in bpy.context.selected_objects:
            if i.type != 'MESH': continue
            i[LGE.id] = self.id

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

def BakeLighting(object, resolution, levelName, bakeEmit=False):
    dTextureName = "LGE_LightingBakeTexture"
    eTextureName = "LGE_EmitBakeTexture"
    
    object.data.uv_layers[1].active = True
    
    if dTextureName in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[dTextureName])
    
    if eTextureName in bpy.data.images:
        bpy.data.images.remove(bpy.data.images[eTextureName])
    
    bpy.ops.image.new(name=dTextureName, width=resolution, height=resolution, float=True)
    bpy.ops.image.new(name=eTextureName, width=resolution, height=resolution, float=True)
    
    imageNodes = []
    
    for dex, mat in enumerate(object.data.materials):
        if not mat:
            mat = object.material_slots[dex].material
        
        bakeNode = mat.node_tree.nodes.new('ShaderNodeTexImage')
        bakeNode.image = bpy.data.images[dTextureName]
        mat.node_tree.nodes.active = bakeNode
        imageNodes.append(bakeNode)
    
    if object.hide_render:
        object.hide_render = False
        bpy.ops.object.bake(type='AO')
        object.hide_render = True
    else:
        bpy.ops.object.bake(type='DIFFUSE', pass_filter={'DIRECT', 'INDIRECT'})

    
    if bakeEmit:
        for i in imageNodes:
            i.image = bpy.data.images[eTextureName]
        
        bpy.ops.object.bake(type='EMIT')
        
        for dex, pixel in enumerate(bpy.data.images[eTextureName].pixels):
            bpy.data.images[dTextureName].pixels[dex] += pixel

        
    bpy.data.images[dTextureName].save(filepath=f"{folderDir}/levels/{levelName}/textures/{SafeName(object.name)}_shadowmap.png")
    
    for dex, mat in enumerate(object.data.materials):
        if not mat:
            mat = object.material_slots[dex].material
        
        mat.node_tree.nodes.remove(imageNodes[dex])
    
    bpy.data.images.remove(bpy.data.images[dTextureName])
    bpy.data.images.remove(bpy.data.images[eTextureName])
    
    
    object.data.uv_layers[0].active = True

    
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
        renderer.data.materials[0].node_tree.nodes['skybox'].image = bpy.data.images.load(folderDir + filename)
        
        tempImage = bpy.data.images.new('cubemapbaking', bpy.data.scenes["Scene"].render.resolution_y, bpy.data.scenes["Scene"].render.resolution_y)
        
        renderer.data.materials[0].node_tree.nodes['skyboxbake'].image = tempImage
        
        bpy.ops.object.bake(type='EMIT')
        renderer.data.materials[0].node_tree.nodes['skyboxbake'].image.save(filepath=folderDir + filename[:-4] + "_" + side + ".png")
        renderer.select_set(False)
        bpy.data.images.remove(tempImage)
        
        renderer.hide_render = True

def RenderAndSaveCubemap(filename):
    for side in ['Back', 'Down', 'Front', 'Left', 'Right', 'Up']:
        renderer = bpy.data.objects['CubemapCreator_' + side]
        renderer.hide_render = True

    bpy.ops.render.render(use_viewport=True)
    bpy.data.images['Render Result'].save_render(folderDir + filename)
    
    SaveCubemap(filename)

def BakeCubemap(levelname):
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
    overrideShadowMapSize: bpy.props.BoolProperty(name="Override Shadow Map Resolution", description="If checked, it will use the shadow map resolution above for all objects, regardless of their Shadow Map Size property")
    calcMapSize: bpy.props.BoolProperty(name="Calculate Map Resolution")
    
    def execute(self, context):
        SetUpCustomProperties()
        
        objects = bpy.context.selected_objects
        
        levelName = GetLevelNameFromObject(objects[0])

        bpy.ops.object.select_all(action='DESELECT')

        wm.progress_begin(0, len(objects))
        dex = 0
        for i in objects:
            if i.type == 'MESH':

                if self.overrideShadowMapSize or i[LGE.shadowSize] == 0:
                    if self.calcMapSize:
                        size = ShadowMapSizeFromData(i.data)
                    else:
                        size = self.shadowmapsize
                else:
                    size = i[LGE.shadowSize]
                
                bakeEmit = False
                for dex, mat in enumerate(i.data.materials):
                    if not mat:
                        mat = i.material_slots[dex].material
                    
                    if mat.node_tree.nodes["Principled BSDF"].inputs[28].default_value > 0.0:
                        bakeEmit = True

                i.select_set(True)
                BakeLighting(i, size, levelName, bakeEmit)
                i.select_set(False)

            wm.progress_update(dex)
            dex += 1    

        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)
    
class BakeCubemapOperator(bpy.types.Operator):
    """Renders a cubemap for either objects to reflect or the skybox"""      # Use this as a tooltip for menu items and buttons.
    bl_idname = "vulkan_utils.bakecubemap"        # Unique identifier for buttons and menu items to reference.
    bl_label = "Render Cubemap (Vulkan)"         # Display name in the interface.
    
    isSky: bpy.props.BoolProperty(name="Is Skybox")
    levelName: bpy.props.StringProperty(name="Level Name")
    height: bpy.props.IntProperty(name="Resolution")
    
    def execute(self, context):
        bpy.data.scenes["Scene"].render.resolution_x = self.height * 2
        bpy.data.scenes["Scene"].render.resolution_y = self.height
        BakeCubemap(f"levels/{self.levelName}/textures/{'skycube' if self.isSky else 'cubemap'}")
        return {'FINISHED'}
    
    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)

class MakeFontOperator(bpy.types.Operator):
    """Creates 3D models for each letter and number of a font for displaying text"""
    bl_idname = "vulkan_utils.makefont"
    bl_label = "Make Font (Vulkan)"
    
    resolution: bpy.props.IntProperty(name="Letter Resolution")
    fontName: bpy.props.StringProperty(name="Font Name")
    characters: bpy.props.StringProperty(name="Characters to Bake")
    
    def execute(self, context):
        name = ""
        
        def ConvertIndexToName(index):
            letters = "abcdefghijklmnopqrstuvwxyz"
            
            out = ""
            while index >= 26:
                out += letters[index % 26]
                index //= 26
            
            out += letters[index]
            return out

        filenames = []
        
        if not os.path.isdir(f"{folderDir}/text/tmp"):
            os.makedirs(f"{folderDir}/text/tmp")
        
        for dex, char in enumerate(self.characters):
            bpy.ops.object.text_add()
            bpy.context.active_object.data.body = char
            bpy.ops.object.convert(target='MESH', keep_original=False)
            bpy.context.active_object.data.materials.append(bpy.data.materials["AOBake"])
            name = ConvertIndexToName(dex)
            filenames.append(name)
            ConvertMeshToVulkanFile(bpy.context.active_object, True, False, f"{folderDir}/text/tmp/{name}.msh")
            bpy.data.objects.remove(bpy.context.active_object)
        
        with open(f"{folderDir}/text/{self.fontName}.fnt", "wb") as file:
            file.write(struct.pack("I", len(self.characters)))
            file.write(self.characters.encode('utf-8'))
            for name in filenames:
                with open(f"{folderDir}/text/tmp/{name}.msh", "rb") as meshFile:
                    file.write(meshFile.read())
            
            
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
    
CT_NONE = 0
CT_BBOX = 1
CT_PERTRI = 2

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
        self.collisionType = CT_NONE
        self.id = 0
        self.shadowMap = 0
        self.luaScript = -1
        self.globalName = -1

class TempMat:
    def __init__(self):
        self.shaderIndex = 0
        self.textures = []
        self.roughness = 0.0

class TempSpotLight:
    def __init__(self, object):
        self.position = object.location
        self.rotation = object.rotation_euler
        self.colour = object.data.color * (object.data.energy / 50)
        self.name = object[LGE.globalName]
        self.fov = object.data.spot_size * 1.415


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
    
    includeshmaps: bpy.props.BoolProperty(name="Bake Shadow Maps")
    shadowmapsize: bpy.props.IntProperty(name="Shadow Map Resolution")
    unwrap: bpy.props.BoolProperty(name="Unwrap Lightmaps")
    exportMeshes: bpy.props.BoolProperty(name="Export Meshes")

    def execute(self, context):
        global lvlStrings
        
        SetUpCustomProperties()
        
        levelName = GetLevelNameFromObject(context.selected_objects[0])
        
        writtenMeshes = []
        
        writtenMaterials = []
        
        lvlStrings = bytearray([])
        lvlMats = bytearray([])
        MeshStringLocs = []
        tempMats = []
        tempObjs = []
        customShaders = []
        
        sunInfo = []
        spotLights = []
            
        allobjects = bpy.context.selected_objects
        
        if not os.path.isdir(f"{folderDir}levels/{levelName}/textures"):
            os.makedirs(f"{folderDir}levels/{levelName}/textures")
        
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

        def SaveImage(image, tempMat, levelname):
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
                
                SaveImage(image, tempMat, levelname)
            else:
                tempMat.textures.append([AddLvlString("textures/default_white" + extension + ".png"), isCol])
        
        
        for i in allobjects:
            wm.progress_update(progress)
            progress += 1
            
            if i.type == 'LIGHT' and i.hide_render:
                if i.data.type == 'SUN':
                    sunInfo = PackVector(i.rotation_euler)
                elif i.data.type == 'SPOT':
                    spotLights.append(TempSpotLight(i))
                    #brightness = i.data.energy / 50
                    #spotLights.append(PackVector(i.location) + PackVector(i.rotation_euler) + PackVectorList(i.data.color * brightness) + pack('f', i.data.spot_size * 1.415))
            
            if i.type != 'MESH': continue
        
            if (i.hide_viewport and i.hide_render) or i[LGE.ignore]: continue
            
            i.select_set(True)
        
            if not i.data.materials:
                raise SyntaxError(i.name + " does not have any materials")
            
            if i.data.name not in writtenMeshes:
                if self.exportMeshes:
                    
                    ConvertMeshToVulkanFile(i, self.unwrap)

                for dex, mat in enumerate(i.data.materials):
                    if not mat:
                        mat = i.material_slots[dex].material
                    if not mat:
                        raise RuntimeError(i.name + " has no material at index " + str(dex))

                MeshStringLocs.append(StringToBytes(f"{SafeName(i.data.name)}"))
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
                        
                        textureNodes.sort(reverse=True, key=TextureSort)
                        
                        for node in textureNodes:
                            SaveImage(node.image, tempMat, levelName)
                        
                    else:
                        if mat.name.startswith("skybox_"):
                            tempMat.shaderIndex = 3
                        else:
                            tempMat.shaderIndex = GetShaderIndex(mat.node_tree.nodes['Principled BSDF'])

                        SaveInputIfLinked(tempMat, mat, levelName, 0, "_col", True)
                        SaveInputIfLinked(tempMat, mat, levelName, 2, "_rgh", False)
                        SaveInputIfLinked(tempMat, mat, levelName, 5, "_nrm", False)

                    tempMats.append(tempMat)
                    writtenMaterials.append(mat.name)
            
            shadowMapFilename = "textures/default_shadowmap.png"
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material
                
                if mat.node_tree.nodes["Principled BSDF"].inputs[28].default_value == 0.0:
                    if self.includeshmaps:
                        BakeLighting(i, self.shadowmapsize, levelName)
                    shadowMapFilename = f"levels/{levelName}/textures/{SafeName(i.name)}_shadowmap.png"
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

            if i[LGE.collision]:
                tempObj.collisionType = CT_PERTRI if i[LGE.complexCollision] else CT_BBOX
            
            for dex, mat in enumerate(i.data.materials):
                if not mat:
                    mat = i.material_slots[dex].material
                tempObj.materials.append(writtenMaterials.index(mat.name))
            
            if i[LGE.forceStatic] == "default":
                tempObj.isStatic = not i.hide_render
            else:
                tempObj.isStatic = i[LGE.forceStatic] == "static"

            tempObj.castShadows = i[LGE.castsShadows]
            tempObj.id = i[LGE.id]
            tempObj.shadowMap = AddLvlString(shadowMapFilename)
            
            
            if i[LGE.script]:
                scriptPath = f"scripts/{i[LGE.script]}.lua"

                # Creating a template script (if it doesn't already exist)
                try:
                    with open(scriptPath, "x") as scriptFile:
                        scriptFile.write("-- 'this' is the thing belonging to this script\n-- Both functions are optional, you can remove either one if it's not needed\n\nfunction Spawn(this)\nend\n\nfunction Tick(this)\nend")
                except:
                    pass

                tempObj.luaScript = AddLvlString(scriptPath)
            
            if i[LGE.globalName]:
                tempObj.globalName = AddLvlString(i[LGE.globalName])

            tempObjs.append(tempObj)

            i.select_set(False)

        wm.progress_end()
        
            
        fileLength = (len(tempMats) * 6) + 2
        for i in tempMats:
            fileLength += len(i.textures) * 5
        
        fileLength += sum([len(string) for string in MeshStringLocs]) + 1

        fileLength += (len(tempObjs) * 67) + 2
        for i in tempObjs:
            fileLength += len(i.materials) * 2

        fileLength += len(customShaders) * 4 + 1

        fileLength += len(sunInfo)
        fileLength += len(spotLights) * ((12 * 3) + 8) + 2

        fileLength += 2 # Length of Point Lights (always 0 till I get that implemented)
        fileLength += 4 # Level Header
        
        print("Writing", len(tempObjs), "objects")
        
        def MakeLevelByte(hasSun, isPacked):
            return ((1 << 7) if hasSun else 0) | ((1 << 6) if isPacked else 0)
        
        with open(folderDir + f"levels/{levelName}/{levelName}.lvl", "wb") as file:
            
            file.write(StringToBytes("LVL")[:-1])
            
            file.write(pack('B', MakeLevelByte(sunInfo, False)))
            
            if sunInfo:
                file.write(sunInfo)
            
            file.write(pack('H', len(spotLights)))
            for i in spotLights:
                file.write(PackVector(i.position))
                file.write(PackVector(i.rotation))
                file.write(PackVectorList(i.colour))
                file.write(pack("f", i.fov))
                file.write(pack("I", (AddLvlString(i.name) + fileLength) if i.name else 0))
                
            file.write(pack('H', 0))
            
            file.write(struct.pack("B", len(customShaders)))
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
            
            print(len(MeshStringLocs))
            for i in MeshStringLocs:
                file.write(i)
            
            file.write(pack('B', 0))
            
            start = 0
            end = len(tempObjs)
            
            while start < len(tempObjs):
                if len(tempObjs) - start > 0xFFFF:
                    end = start + 0xFFFF
                else:
                    end = len(tempObjs)
                
                file.write(struct.pack("H", end - start))

                for i in tempObjs[start:end]:
                    file.write(struct.pack("H", i.meshIndex))
                    file.write(i.position)
                    file.write(i.rotation)
                    file.write(i.scale)

                    file.write(struct.pack("B", len(i.materials)))
                    for mat in i.materials:
                        file.write(struct.pack("H", mat))

                    file.write(struct.pack("?", i.isStatic))
                    file.write(struct.pack("?", i.castShadows))
                    file.write(struct.pack("B", i.collisionType))
                    file.write(struct.pack("B", i.id))
                    file.write(struct.pack("I", i.shadowMap + fileLength))
                    file.write(struct.pack("I", (i.luaScript + fileLength) if i.luaScript >= 0 else 0))
                    file.write(struct.pack("I", (i.globalName + fileLength) if i.globalName >= 0 else 0))
                    file.write(struct.pack("f", 0)) # Shadow Map Offset X
                    file.write(struct.pack("f", 0)) # Shadow Map Offset Y
                    file.write(struct.pack("f", 0)) # Shadow Map Scale
                
                start = end
            
            print(file.tell(), fileLength)

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
    (BakeCubemapOperator, "bakecubemap"),
    (UnwrapLightmapOperator, "unwraplightmap"),
    (SetPassIndexOperator, "set_object_id"),
    (FixFO2UVOperator, "fo2uvs"),
    (SetUVActiveOperator, "set_active_uv"),
    (MaterialReplaceOperator, "replace_material"),
    (MaterialFindOperator, "find_material"),
    (MakeFontOperator, "makefont"),
    (SetUpCustomPropertiesOperator, "setupproperties"),
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