import os

batchScript = ""

gamePath = "shaders/"

for file in os.scandir(gamePath):
    filename = file.name[file.name.find("/") + 1:]
    shaderType = filename[filename.rindex(".") + 1:]
    if shaderType in ["frag", "vert", "comp"]:
        batchScript += "D:/VulkanSDK/Bin/glslc.exe " + gamePath + filename + " -o " + gamePath + filename[:filename.rindex(".")] + "_" + ("pixl" if shaderType == "frag" else shaderType) + ".spv\n"

with open("shaders/compileshaders.bat", "w") as file:
    file.write(batchScript)
    file.write("pause\n")