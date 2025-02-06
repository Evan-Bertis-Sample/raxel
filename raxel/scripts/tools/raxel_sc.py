# raxel_sc.py
# raxel_shader_compiler

import os
import sys
from tools.build_util import RaxelBuildOptions, RaxelBuildType, RaxelBuildUtil
from tools.tool_util import RaxelToolUtil

RAXEL_SHADER_COMPILER_BASH_SCRIPT = "raxel_sc.sh"

def register_subparser(subparser):
    # we need the path to the root directory of the project
    subparser.add_argument(
        "--project_dir",
        type=str,
        default=".",
        help="The path to the root directory of the project",
    )

    # also add a description for the subparser
    subparser.description = "Compile the shaders for the project"

def main(args):
    # if the project directory is not specified (default is "."), then use the current directory
    project_dir = args.project_dir
    if project_dir == ".":
        project_dir = os.getcwd()

    raxel_sc_script = os.path.join(RaxelToolUtil.get_raxel_tool_dir(), RAXEL_SHADER_COMPILER_BASH_SCRIPT)
    if not os.path.exists(raxel_sc_script):
        print(f"Error: The raxel shader compiler script {raxel_sc_script} does not exist")
        return

    # raxel_sc.sh <project_root> <raxel_root>
    os.system(f"bash {raxel_sc_script} {project_dir} {RaxelToolUtil.get_raxel_dir()}")