# build_util.py

import os
from tools.tool_util import RaxelToolUtil
import enum

class RaxelBuildType(enum.Enum):
    Debug = "Debug"
    Release = "Release"

    @staticmethod
    def options() -> list:
        return [RaxelBuildType.Debug, RaxelBuildType.Release]
    
    @staticmethod
    def options_as_strings() -> list:
        return [str(option) for option in RaxelBuildType.options()]
    
    @staticmethod
    def from_string(build_type_str : str) -> "RaxelBuildType":
        for option in RaxelBuildType.options():
            if str(option) == build_type_str:
                return option
        return None

class RaxelBuildOptions:
    def __init__(self):
        self.build_type = RaxelBuildType.Release
        self.build_dir = ""
        self.project_dir = ""

    def set_build_type(self, build_type : RaxelBuildType) -> "RaxelBuildOptions":
        self.build_type = build_type
        return self

    def set_build_dir(self, build_dir : str) -> "RaxelBuildOptions":
        self.build_dir = build_dir
        return self
    
    def set_project_dir(self, project_dir : str) -> "RaxelBuildOptions":
        self.project_dir = project_dir
        return self
    
    def is_valid(self) -> bool:
        return self.build_dir != "" and self.project_dir != ""
    
    @staticmethod
    def add_subparser_args(subparser):
        subparser.add_argument("--build-type", choices=RaxelBuildType.options_as_strings(), default=RaxelBuildType.Release, help="The build type")
        subparser.add_argument("--build-dir", default="", help="The build directory")
        subparser.add_argument("--project-dir", default="", help="The project directory")
        
class RaxelBuildUtil:
    @staticmethod
    def get_build_dir(project_dir : str) -> str:
        return os.path.join(RaxelToolUtil.get_raxel_work_dir(project_dir), "build")
    
    @staticmethod
    def build_project(options : RaxelBuildOptions):
        if not options.is_valid():
            print("Error: Invalid build options")
            return
        
        # get the build directory

