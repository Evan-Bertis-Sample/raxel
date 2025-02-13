# raxel_build.py

import os
import sys
from tools.build_util import RaxelBuildOptions, RaxelBuildType, RaxelBuildUtil
from tools.tool_util import RaxelToolUtil

TEST_APPLICATION = "raxel-test"

def main(args):
    build_options = RaxelBuildOptions()
    build_options.set_build_type(RaxelBuildType.Debug)
    # if the project directory is not specified (default is "."), then use the current directory
    project_dir = RaxelToolUtil.get_root_dir() + os.path.sep + TEST_APPLICATION

    if not os.path.exists(project_dir):
        print(f"Error: The project directory {project_dir} does not exist")
        sys.exit(1)

    build_options.set_project_dir(project_dir)
    RaxelBuildUtil.build_project(build_options)
    RaxelBuildUtil.run_project(build_options)