#
# All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
# its licensors.
#
# For complete copyright and license terms please see the LICENSE at the root of this
# distribution (the "License"). All use of this software is governed by the License,
# or, if provided, by the license below or the license accompanying this file. Do not
# remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#
import argparse
import importlib
import os
import pkgutil
import shutil
import subprocess


class _Configuration:
    def __init__(self, platform, asset_platform, core_compiler):
        self.platform = platform
        self.asset_platform = asset_platform
        self.compiler = '{}-{}'.format(platform, core_compiler)

    def __str__(self):
        return '{} ({})'.format(self.platform, self.asset_platform)

class _ShaderType:
    def __init__(self, name, base_compiler):
        self.name = name
        self.core_compiler = '{}-{}'.format(base_compiler, name)
        self.configurations = []

    def add_configuration(self, platform, asset_platform):
        self.configurations.append(_Configuration(platform, asset_platform, self.core_compiler))

def find_shader_type(shader_type_name, shader_types):
    return next((shader for shader in shader_types if shader.name == shader_type_name), None)

def find_shader_configuration(platform, assets, shader_configurations):
    if platform:
        check_func = lambda config: config.platform == platform and config.asset_platform == assets
    else:
        check_func = lambda config: config.asset_platform == assets

    return next((config for config in shader_configurations if check_func(config)), None)

def error(msg):
    print(msg)
    exit(1)

def is_windows():
    if os.name == 'nt':
        return True
    else:
        return False

def gen_shaders(game_name, shader_type, shader_config, shader_list, bin_folder, game_path, engine_path, verbose):
    """
    Generates the shaders for a specific platform and shader type using a list of shaders using ShaderCacheGen.
    The generated shaders will be output at Cache/<game_name>/<asset_platform>/user/cache/Shaders/Cache/<shader_type>
    """
    platform = shader_config.platform
    asset_platform = shader_config.asset_platform
    compiler = shader_config.compiler

    asset_cache_root = os.path.join(game_path, 'Cache', game_name, asset_platform)

    # Make sure that the Cache/.../user folder exists
    cache_user_folder = os.path.join(asset_cache_root, 'user')
    if not os.path.isdir(cache_user_folder):
        try:
            os.makedirs(cache_user_folder)
        except os.error as err:
            error("Unable to create the required cache folder '{}': {}".format(cache_user_folder, err))

    cache_shader_list = os.path.join(cache_user_folder, 'cache', 'shaders', 'shaderlist.txt')

    if shader_list is None:
        if is_windows():
            shader_compiler_platform = 'x64'
        else:
            shader_compiler_platform = 'osx'

        shader_list_path = os.path.join(engine_path, 'Tools', 'CrySCompileServer', shader_compiler_platform, 'profile', 'Cache', game_name, compiler, 'ShaderList_{}.txt'.format(shader_type))
        if not os.path.isfile(shader_list_path):
            shader_list_path = cache_shader_list

        print("Source Shader List not specified, using {} by default".format(shader_list_path))
    else:
        shader_list_path = os.path.join(game_path, shader_list)

    normalized_shaderlist_path = os.path.normpath(os.path.normcase(os.path.realpath(shader_list_path)))
    normalized_cache_shader_list = os.path.normpath(os.path.normcase(os.path.realpath(cache_shader_list)))

    if normalized_shaderlist_path != normalized_cache_shader_list:
        cache_shader_list_basename = os.path.split(cache_shader_list)[0]
        if not os.path.exists(cache_shader_list_basename):
            os.makedirs(cache_shader_list_basename)
        print("Copying shader_list from {} to {}".format(shader_list_path, cache_shader_list))
        shutil.copy2(shader_list_path, cache_shader_list)

    platform_shader_cache_path = os.path.join(cache_user_folder, 'cache', 'shaders', 'cache', shader_type.lower())
    shutil.rmtree(platform_shader_cache_path, ignore_errors=True)

    shadergen_path = os.path.join(engine_path, bin_folder, 'ShaderCacheGen')
    if is_windows():
        shadergen_path += '.exe'

    if not os.path.isfile(shadergen_path):
        error("ShaderCacheGen could not be found at {}".format(shadergen_path))
    else:
        command_arguments = [
            shadergen_path, 
            '/BuildGlobalCache', 
            '/ShadersPlatform={}'.format(shader_type), 
            '/TargetPlatform={}'.format(asset_platform)
        ]
        if verbose:
            print('Running: {}'.format(' '.join(command_arguments)))
        subprocess.call(command_arguments)

def add_shaders_types():
    """
    Add the shader types for the non restricted platforms.
    The compiler argument is used for locating the shader_list file.
    """
    shaders = []
    d3d11 = _ShaderType('D3D11', 'D3D11_FXC')
    d3d11.add_configuration('PC', 'pc')
    shaders.append(d3d11)

    gl4 = _ShaderType('GL4', 'GLSL_HLSLcc')
    gl4.add_configuration('PC', 'pc')
    shaders.append(gl4)

    gles3 = _ShaderType('GLES3', 'GLSL_HLSLcc')
    gles3.add_configuration('Android', 'es3')
    shaders.append(gles3)

    metal = _ShaderType('METAL', 'METAL_LLVM_DXC')
    metal.add_configuration('Mac', 'osx_gl')
    metal.add_configuration('iOS', 'ios')
    shaders.append(metal)

    for (_, module_name, is_package) in pkgutil.iter_modules([os.path.dirname(__file__)]):
        if not is_package:
            continue

        try:
            imported_module = importlib.import_module('{}.gen_shaders'.format(module_name))
        except:
            continue

        restricted_func = getattr(imported_module, 'get_restricted_platform_shader', lambda: iter(()))

        for shader_type, shader_compiler, platform_name, asset_platform in restricted_func():

            shader = find_shader_type(shader_type, shaders)
            if shader is None:
                shader = _ShaderType(shader_type, shader_compiler)
                shaders.append(shader)

            shader.add_configuration(platform_name, asset_platform)

    return shaders

def check_arguments(args, parser, shader_types):
    """
    Check that the platform and shader type arguments are correct.
    """
    shader_names = [shader.name for shader in shader_types]

    shader_found = find_shader_type(args.shader_type, shader_types)
    if shader_found is None:
        parser.error('Invalid shader type {}. Must be one of [{}]'.format(args.shader_type, ' '.join(shader_names)))

    else:
        config_found = find_shader_configuration(args.shader_platform, args.asset_platform, shader_found.configurations)
        if config_found is None:
            parser.error('Invalid configuration for shader type "{}". It must be one of the following: {}'.format(shader_found.name, ', '.join(str(config) for config in shader_found.configurations)))

    args.game_path = args.game_path or args.engine_path


parser = argparse.ArgumentParser(description='Generates the shaders for a specific platform and shader type.')
parser.add_argument('game_name', type=str, help="Name of the game")
parser.add_argument('asset_platform', type=str, help="The asset cache sub folder to use for shader generation")
parser.add_argument('shader_type', type=str, help="The shader type to use")
parser.add_argument('-p', '--shader_platform', type=str, required=False, default='', help="The target platform to generate shaders for.")
parser.add_argument('-b', '--bin_folder', type=str, help="Folder where the ShaderCacheGen executable lives. This is used along the game_path (game_path/bin_folder/ShaderCacheGen)")
parser.add_argument('-e', '--engine_path', type=str, help="Path to the engine root folder. This the same as game_path for non external projects")
parser.add_argument('-g', '--game_path', type=str, required=False, help="Path to the game root folder. This the same as engine_path for non external projects")
parser.add_argument('-s', '--shader_list', type=str, required=False, help="Optional path to the list of shaders. If not provided will use the list generated by the local shader compiler.")
parser.add_argument('-v', '--verbose', action="store_true", required=False, help="Increase the logging output")

args = parser.parse_args()

shader_types = add_shaders_types()

check_arguments(args, parser, shader_types)
print('Generating shaders for {} (shaders={}, platform={}, assets={})'.format(args.game_name, args.shader_type, args.shader_platform, args.asset_platform))

shader = find_shader_type(args.shader_type, shader_types)
shader_config = find_shader_configuration(args.shader_platform, args.asset_platform, shader.configurations)
gen_shaders(args.game_name, args.shader_type, shader_config, args.shader_list, args.bin_folder, args.game_path, args.engine_path, args.verbose)

print('Finish generating shaders')
