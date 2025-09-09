@echo off

:: If your PATH isn't set up to make this work you can change it to the path to python.exe
SET pythonDir=python

cd ../

echo Cleaning up shaders...
%pythonDir% shaders/cleanupShaders.py

echo Compiling ZLSLs...
%pythonDir% shaders/glsltool.py

echo Creating generated.bat...
%pythonDir% shaders/create_shader_compiler.py

echo Running generated.bat...
shaders/generated.bat

PAUSE