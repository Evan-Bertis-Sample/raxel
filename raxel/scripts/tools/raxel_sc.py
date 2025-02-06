# raxel_sc.py
# raxel_shader_compiler

import os
import sys
from tools.build_util import RaxelBuildOptions, RaxelBuildType, RaxelBuildUtil
from tools.tool_util import RaxelToolUtil


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

    RaxelBuildUtil.compile_shaders(project_dir)