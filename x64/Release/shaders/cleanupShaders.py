import os

gamePath = "shaders/"

for file in os.scandir(gamePath):
    if not file.is_file() or "." not in file.name:
        continue

    if file.name[file.name.rindex(".") + 1:] in ["spv", "frag", "vert"]:
        os.remove(file.path)