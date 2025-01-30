# raxel_build.py

import os
import sys
from tools.build_util import RaxelBuildOptions, RaxelBuildType, RaxelBuildUtil
from tools.tool_util import RaxelToolUtil

def register_subparser(subparser):
    RaxelBuildOptions.add_subparser_args(subparser)


def main(args):
    print("raxel_build main() called")