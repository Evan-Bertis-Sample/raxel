# raxel_run.py

import os
import sys
from tools.build_util import RaxelBuildOptions, RaxelBuildType, RaxelBuildUtil
from tools.tool_util import RaxelToolUtil

def register_subparser(subparser):
    RaxelBuildOptions.add_subparser_args(subparser)

def main(args):
    build_options = RaxelBuildOptions()
    build_options.set_build_type(RaxelBuildType.from_string(args.build_type))
    # if the project directory is not specified (default is "."), then use the current directory
    project_dir = args.project_dir
    if project_dir == ".":
        project_dir = os.getcwd()
    build_options.set_project_dir(project_dir)

    RaxelBuildUtil.run_project(build_options)