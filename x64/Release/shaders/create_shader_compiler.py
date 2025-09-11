import os

batchScript = ""

gamePath = "shaders/"

for file in os.scandir(gamePath):
    if "." not in file.name: continue
    filename = file.name[file.name.find("/") + 1:]
    shaderType = filename[filename.rindex(".") + 1:]
    if shaderType in ["frag", "vert", "comp"]:
        batchScript += "shaders\\glslc.exe " + gamePath + filename + " -o " + gamePath + filename[:filename.rindex(".")] + "_" + ("pixl" if shaderType == "frag" else shaderType) + ".spv\n"

with open("shaders/generated.bat", "w") as file:
    file.write(batchScript)
    file.write("pause\n")