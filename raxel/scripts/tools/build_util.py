# build_util.py

import os
from tools.tool_util import RaxelToolUtil
import subprocess
import enum
import shutil


class RaxelBuildType(enum.Enum):
    Debug = "Debug"
    Release = "Release"

    @staticmethod
    def options() -> list:
        return [RaxelBuildType.Debug, RaxelBuildType.Release]
    
    @staticmethod
    def to_clean_str(option: "RaxelBuildType") -> str:
        base = str(option)
        return base[base.index(".") + 1:]

    @staticmethod
    def options_as_strings() -> list:
        return [RaxelBuildType.to_clean_str(option) for option in RaxelBuildType.options()]

    @staticmethod
    def from_string(build_type_str: str) -> "RaxelBuildType":
        for option in RaxelBuildType.options():
            clean_str = RaxelBuildType.to_clean_str(option)
            if clean_str in build_type_str or build_type_str in clean_str:
                return option
        return None


class RaxelBuildOptions:
    def __init__(self):
        self.build_type = RaxelBuildType.Release
        self.project_dir = ""
        self.__project_name = ""

    @property
    def project_name(self) -> str:
        if self.__project_name == "":
            return os.path.basename(self.project_dir)
        return self.__project_name

    def set_build_type(self, build_type: RaxelBuildType) -> "RaxelBuildOptions":
        self.build_type = build_type
        return self
    
    def set_project_name(self, project_name: str) -> "RaxelBuildOptions":
        self.__project_name = project_name
        return self

    def set_project_dir(self, project_dir: str) -> "RaxelBuildOptions":
        self.project_dir = project_dir
        return self

    def is_valid(self) -> bool:
        return self.project_dir != ""

    @staticmethod
    def add_subparser_args(subparser):
        subparser.add_argument(
            "--build-type",
            choices=RaxelBuildType.options_as_strings(),
            default=str(RaxelBuildType.Release),
            help="The build type",
        )
        subparser.add_argument(
            "--project-dir", default=".", help="The project directory"
        )


class RaxelBuildUtil:
    @staticmethod
    def get_build_dir(project_dir: str) -> str:
        return os.path.join(RaxelToolUtil.get_raxel_work_dir(project_dir), "build")

    @staticmethod
    def build_project(options: RaxelBuildOptions):
        if not options.is_valid():
            print("Error: Invalid build options")
            return

        # Check if the project directory actually exists
        project_dir = os.path.abspath(options.project_dir)
        if not os.path.exists(project_dir):
            print(f"Error: Project directory does not exist: {project_dir}")
            return

        # Figure out build directory
        build_dir = RaxelBuildUtil.get_build_dir(project_dir)

        # Create the build directory if it doesn't exist
        if not os.path.exists(build_dir):
            os.makedirs(build_dir)

        print(build_dir)
        raxel_dir = RaxelToolUtil.get_raxel_dir()
        rel_project_dir = os.path.relpath(project_dir, raxel_dir)
        # replace backslashes with forward slashes for CMake
        rel_project_dir = rel_project_dir.replace("\\", "/")
        work_dir = raxel_dir

        # get the build directory
        # Run CMake configure
        cmake_command = [
            "cmake",
            "-G",
            "Unix Makefiles",
            "-S",
            ".",  # Assume the current directory is the root CMakeLists.txt
            "-B",
            build_dir,
            f"-DPROJECT={options.project_name}",
            f"-DPROJECT_CMAKE_DIR={rel_project_dir}",
            f"-DCMAKE_BUILD_TYPE={RaxelBuildType.to_clean_str(options.build_type)}",
        ]

        print(f"Configuring with CMake: {' '.join(cmake_command)}\n")

        try:
            subprocess.run(cmake_command, check=True, cwd=work_dir)
        except subprocess.CalledProcessError as e:
            print(f"Error configuring with CMake: {e}")
            return

        # 5. Run the build for the requested project target
        build_command = ["cmake", "--build", build_dir, "--target", options.project_name]
        print (f"Building project: {' '.join(build_command)}\n")
        try:
            subprocess.run(build_command, check=True, cwd=work_dir)
        except subprocess.CalledProcessError as e:
            cli_width = shutil.get_terminal_size().columns

            print("")
            print("=" * cli_width)
            print("")

            print(f"Error building project: {e}")
            return

        print(f"Build complete. The executable should be in {build_dir}")

    @staticmethod
    def run_project(options: RaxelBuildOptions):
        if not options.is_valid():
            print("Error: Invalid build options")
            return

        # Check if the project directory actually exists
        project_dir = os.path.abspath(options.project_dir)
        if not os.path.exists(project_dir):
            print(f"Error: Project directory does not exist: {project_dir}")
            return

        # Figure out build directory
        build_dir = RaxelBuildUtil.get_build_dir(project_dir)

        # Check if the build directory exists
        if not os.path.exists(build_dir):
            print(f"Error: Build directory does not exist: {build_dir}")
            return

        # Run the project
        run_command = [os.path.join(build_dir, options.project_name)]
        print(f"Running project: {' '.join(run_command)}\n")

        try:
            subprocess.run(run_command, check=True)
        except subprocess.CalledProcessError as e:
            print(f"Error running project: {e}")
            return
