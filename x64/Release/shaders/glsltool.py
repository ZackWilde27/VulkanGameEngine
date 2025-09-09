# HLSL-ish to Vulkan GLSL
# Personal tool to make writing shaders for my vulkan-based engine much easier

import os

def GetParEnd(string, index):
    i = 0
    for dex, char in enumerate(string[index:]):
        if char == "{":
            i += 1

        if char == "}":
            if i:
                i -= 1
            else:
                return dex + index
    return len(string)

def StartsWith(string, substr):
    return string[:len(substr)] == substr

def Compile(zlsl, filename):
    vshaderscript = zlsl
    pshaderscript = zlsl

    includev = True
    includep = True

    for line in zlsl.split("\n"):
        if StartsWith(line, "#VertexShader "):
            with open("shaders/" + line[len("#VertexShader  "):-1]) as includefile:
                vshaderscript = includefile.read()
                includev = False
            pshaderscript = pshaderscript.replace(line + "\n", "")

        if StartsWith(line, "#PixelShader "):
            with open("shaders/" + line[len("#PixelShader  "):-1]) as includefile:
                pshaderscript = includefile.read()
                includep = False
            vshaderscript = vshaderscript.replace(line + "\n", "")

    vindex = vshaderscript.index("VertexShader(")

    vparams = vshaderscript[vshaderscript.index("(", vindex) + 1:vshaderscript.index(")", vindex)]
    vparams = [item.strip() for item in vparams.split(",")]

    vincludes = vshaderscript[:vindex]

    for param in vparams:
        if param:
            vincludes += "in " + param + ";\n"
    vincludes += "\n"

    vindex = vshaderscript.index("{", vindex) + 1
    vend = GetParEnd(vshaderscript, vindex)

    tend = vend if includev else 0

    pindex = pshaderscript.index("PixelShader(")

    pparams = pshaderscript[pshaderscript.index("(", pindex) + 1:pshaderscript.index(")", pindex)]
    pparams = [item.strip() for item in pparams.split(",")]
    
    pincludes = pshaderscript[tend + 1:pindex]

    for param in pparams:
        if param:
            vincludes += "out " + param + ";\n"

    binding = 0
    inLocation = 0
    outLocation = 0

    for line in vincludes.split("\n"):
        if line[:len("layout(location")] == "layout(location" or line[:len("layout (location")] == "layout (location":
            if " out " in line:
                pincludes = line.replace(" out ", " in ") + "\n" + pincludes

        if StartsWith(line, "push "):
            vincludes = vincludes.replace(line + "\n", f"layout(push_constant) {line[5:]}\n")

        if StartsWith(line, "out "):
            vincludes = vincludes.replace(line + "\n", f"layout(location = {outLocation}) {line}\n")
            pincludes = f"layout(location = {outLocation}) in {line[4:]}\n" + pincludes
            outLocation += 1

        if StartsWith(line, "layout(binding"):
            binding += 1

        if StartsWith(line, "uniform "):
            vincludes = vincludes.replace(line + "\n", f"layout(binding = {binding}) {line}\n")
            binding += 1

        if StartsWith(line, "in "):
            vincludes = vincludes.replace(line + "\n", f"layout(location = {inLocation}) {line}\n")
            inLocation += 1

    for line in pincludes.split("\n"):
        if StartsWith(line, "layout(binding"):
            binding += 1
        for sampler in ["sampler2D", "samplerCUBE"]:
            if line[:len(sampler + " ")] == sampler + " ":
                pincludes = pincludes.replace(line + "\n", f"layout(binding = {binding}) uniform {sampler} {line[line.index(' ') + 1:]}\n")
                binding += 1
        if StartsWith(line, "uniform "):
            pincludes = pincludes.replace(line + "\n", f"layout(binding = {binding}) {line}\n")
            binding += 1

        if StartsWith(line, "push "):
            pincludes = pincludes.replace(line + "\n", f"layout(push_constant) {line[5:]}\n")


    
    pindex = zlsl.index("{", pindex) + 1
    pend = GetParEnd(zlsl, pindex)
    
    vshader = vshaderscript[vindex:vend]
    pshader = zlsl[pindex:pend]

    if includev:
        with open("shaders/" + filename[:filename.rindex(".")] + ".vert", "w") as file:
            file.write("#version 450\n\n#include \"zutils.glsl\"\n")
            file.write(vincludes.strip())
            file.write("\n\nvoid main()\n{\n\t")
            file.write(vshader.strip())
            file.write("\n}")

    if includep:
        with open("shaders/" + filename[:filename.rindex(".")] + ".frag", "w") as file:
            file.write("#version 450\n\n#include \"zutils.glsl\"\n")
            file.write(pincludes.strip())
            file.write("\n\nvoid main()\n{\n\t")
            file.write(pshader.strip())
            file.write("\n}")

for file in os.scandir(path="shaders"):
    if file.name[file.name.rindex(".") + 1:] == "zlsl":
        with open("shaders/" + file.name) as shaderfile:
            try:
                Compile(shaderfile.read(), file.name)
            except:
                raise RuntimeError("Error in " + file.name)
