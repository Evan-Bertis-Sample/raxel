#!/usr/bin/env bash

# 
# raxel_sc.sh -- Raxel Shader Compiler 
#

# finds any file with the extensions:
#   - .vert
#   - .frag
#   - .comp
#   - .geom
#   - .tesc
#   - .tese
#   - .glsl
#   - .hlsl
# and it compiles them into SPIR-V using glslangValidator
# it will put them in the same directory as the source file, and collects them into the .raxel/shaders directory
# the path from the root of the project to the shader file will be preserved in the .raxel/shaders directory

# expects a single argument:
#   1. the path to the root of the raxel project
#   2. the path to the raxel core directory -- these will be compiled into the .raxel/internal/shaders/ directory

# example usage:
#   rsc.sh /path/to/raxel

PATH_TO_PROJECT=$1
COMPILED_DIR=$PATH_TO_PROJECT/.raxel/build/shaders

# create the compiled directory if it doesn't exist
if [ ! -d "$COMPILED_DIR" ]; then
  mkdir -p $COMPILED_DIR
fi

# find all the shader files, and collect their relative paths
SHADER_FILES=$(find $PATH_TO_PROJECT -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" -o -name "*.geom" -o -name "*.tesc" -o -name "*.tese" -o -name "*.glsl" -o -name "*.hlsl" \))

# if there are no shader files, let the user know and exit
if [ -z "$SHADER_FILES" ]; then
  echo "No shader files found"
fi

# compile each shader file
for SHADER_FILE in $SHADER_FILES; do
  # get the relative path to the shader file
  RELATIVE_PATH=$(realpath --relative-to=$PATH_TO_PROJECT $SHADER_FILE)
  # get the file name without the extension
  echo "Compiling $RELATIVE_PATH"
  # get the file extension
  EXTENSION="${SHADER_FILE##*.}"
  # compile the shader file
  # make sure the directory exists
  mkdir -p $COMPILED_DIR/$(dirname $RELATIVE_PATH)
  glslangValidator -V $SHADER_FILE -o $COMPILED_DIR/$RELATIVE_PATH.spv
done

# compile the core shaders
CORE_DIR=$2/assets/shaders
CORE_COMPILED_DIR=$PATH_TO_PROJECT/.raxel/build/internal/shaders

# create the compiled directory if it doesn't exist
if [ ! -d "$CORE_COMPILED_DIR" ]; then
  mkdir -p $CORE_COMPILED_DIR
fi

# find all the shader files, and collect their relative paths
CORE_SHADER_FILES=$(find $CORE_DIR -type f \( -name "*.vert" -o -name "*.frag" -o -name "*.comp" -o -name "*.geom" -o -name "*.tesc" -o -name "*.tese" -o -name "*.glsl" -o -name "*.hlsl" \))

# compile each shader file
for SHADER_FILE in $CORE_SHADER_FILES; do
  # get the relative path to the shader file
  RELATIVE_PATH=$(realpath --relative-to=$CORE_DIR $SHADER_FILE)
  # get the file name without the extension
  echo $"Compiling internal shader $RELATIVE_PATH"
  # get the file extension
  EXTENSION="${SHADER_FILE##*.}"
  # make sure the directory exists
  mkdir -p $CORE_COMPILED_DIR/$(dirname $RELATIVE_PATH)
  # compile the shader file
  glslangValidator -V $SHADER_FILE -o $CORE_COMPILED_DIR/$RELATIVE_PATH.spv
done