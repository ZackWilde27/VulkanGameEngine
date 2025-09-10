#! /bin/bash

pythonDir="python3"

cd ../

echo "Cleaning up shaders..."
$pythonDir shaders/cleanupShaders.py

echo "Compiling ZLSLs..."
$pythonDir shaders/glsltool.py

echo "Compiling GLSLs..."

# I needed a refresher on writing bash script and realized I could do all of this without Python, so here we go
cd shaders

for file in *; do
    if [ -f "$file" ]; then
        # Well ok look I didn't say I was great at bash script
        vertTest=$(grep "\.vert$" <<< $file)
        fragTest=$(grep "\.frag$" <<< $file)

        if [ "$vertTest" ] || [ "$fragTest" ]; then
            fileType=$(cut -d '.' -f2 <<< "$file")
            fileName=$(cut -d '.' -f1 <<< "$file")

            if [ "$fileType" = "frag" ]; then
                fileType='pixl'
            fi

            destFilename="${fileName}_${fileType}.spv"

            ./glslc $file -o $destFilename
        fi
    fi
done

echo "Done!"
