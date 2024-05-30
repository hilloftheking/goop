# This doesn't really compile the shaders, it applies optimizations using
# glslc, puts them through spirv-cross to get them back to GLSL, minifies that,
# then shoves them into headers

# TODO: Automatically put uniform locations into C header/source

import os
import subprocess

SRC_OUTPUT = "../src/shader_sources.c"
HEADER_OUTPUT = "../src/shader_sources.h"

TEMP_FILE_NAME = ".temp"

GLSLC_EXE = "glslc"
GLSLC_ARGS = ["--target-env=opengl", "-O"]

SPIRV_CROSS_EXE = "spirv-cross"
SPIRV_CROSS_ARGS = ["--version", "430", "--no-es"]

GLSL_EXTENSIONS = [".vert", ".frag", ".comp"]
GLSL_SYMBOLS = "!+-/*%^&|.,;=<>()[]{} "

def is_glsl_filename(name):
    for ext in GLSL_EXTENSIONS:
        if name.endswith(ext):
            return True
    return False

def run_exe(name, args):
    p = subprocess.run([name] + args, shell=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    print(p.stdout, end="")
    p.check_returncode()

def minify_glsl(path):
    with open(path, "r") as f:
        out = ""
        for line in f:
            if line.startswith("#"):
                out += line
                continue
            
            for i in range(len(line)):
                c = line[i]
                if c == '\n' or c == '\t':
                    continue
                if i == 0 or i == len(line) - 1:
                    if c != ' ':
                        out += line[i]
                    continue
                if c == ' ' and (line[i - 1] in GLSL_SYMBOLS or line[i + 1] in GLSL_SYMBOLS):
                    continue
                out += line[i]
        
        return out

def main():
    shader_paths = {}

    parent_dir = os.path.dirname(os.path.realpath(__file__))
    temp_path = os.path.join(parent_dir, TEMP_FILE_NAME)
    
    content = os.scandir(parent_dir)
    for entry in content:
        if entry.is_file() and is_glsl_filename(entry.name):
            shader_paths[entry.name] = entry.path
            
    src_content = ""
    header_content = "#pragma once\n\n"
    
    for shader_name in shader_paths:
        shader_path = shader_paths[shader_name]
    
        run_exe(GLSLC_EXE, GLSLC_ARGS + ["-o" + temp_path, shader_path])
        run_exe(SPIRV_CROSS_EXE, SPIRV_CROSS_ARGS + ["--output", temp_path, temp_path])
        
        minified = minify_glsl(temp_path)
        
        var_name = shader_name.replace('.', '_').upper() + "_SRC"
        
        src_content += "const char *" + var_name + " = \""
        for line in minified.splitlines():
            src_content += line + "\\n"
        src_content += "\";\n";
        
        header_content += "extern const char *" + var_name + ";\n"
    
    src_path = os.path.join(parent_dir, SRC_OUTPUT)
    header_path = os.path.join(parent_dir, HEADER_OUTPUT)
    
    with open(src_path, "w") as f:
        f.write(src_content)
    with open(header_path, "w") as f:
        f.write(header_content)
    
if __name__ == "__main__":
    main()