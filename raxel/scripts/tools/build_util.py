# build_util.py

import os
from tools.tool_util import RaxelToolUtil
import subprocess
import enum
import shutil
import hashlib


class RaxelBuildType(enum.Enum):
    Debug = "Debug"
    Release = "Release"

    @staticmethod
    def options() -> list:
        return [RaxelBuildType.Debug, RaxelBuildType.Release]

    @staticmethod
    def to_clean_str(option: "RaxelBuildType") -> str:
        base = str(option)
        return base[base.index(".") + 1 :]

    @staticmethod
    def options_as_strings() -> list:
        return [
            RaxelBuildType.to_clean_str(option) for option in RaxelBuildType.options()
        ]

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
    def generate_checksum(dirs: list) -> str:
        checked_file_types = [
            # C/C++ sources
            ".cpp",
            ".h",
            ".c",
            ".hpp",
            # CMake files
            "CMakeLists.txt",
        ]

        shader_types = [
            # Shader sources
            ".vert",
            ".frag",
            ".comp",
            ".geom",
            ".tesc",
            ".tese",
            ".glsl",
        ]

        def add_sha(file: str, sha: hashlib.sha256):
            with open(file, "rb") as f:
                while True:
                    data = f.read(65536)
                    if not data:
                        break
                    sha.update(data)

        source_sha256 = hashlib.sha256()
        shader_sha256 = hashlib.sha256()

        for dir in dirs:
            for root, _, files in os.walk(dir):
                for file in files:
                    if os.path.splitext(file)[1] in checked_file_types:
                        add_sha(os.path.join(root, file), source_sha256)
                    if os.path.splitext(file)[1] in shader_types:
                        add_sha(os.path.join(root, file), shader_sha256)


        return (source_sha256.hexdigest(), shader_sha256.hexdigest())

    @staticmethod
    def create_build_checksum(build_options: RaxelBuildOptions):
        source_checksum, shader_sha = RaxelBuildUtil.generate_checksum(
            [build_options.project_dir, RaxelToolUtil.get_raxel_dir()]
        )
        checksum_file = os.path.join(
            RaxelBuildUtil.get_build_dir(build_options.project_dir), "checksum.txt"
        )
        with open(checksum_file, "w") as f:
            f.write(source_checksum)
            f.write("\n")
            f.write(shader_sha)

    @staticmethod
    def is_checksum_valid(build_options: RaxelBuildOptions) -> bool:
        checksum_file = os.path.join(
            RaxelBuildUtil.get_build_dir(build_options.project_dir), "checksum.txt"
        )
        if not os.path.exists(checksum_file):
            return False
        
        content = ""
        with open(checksum_file, "r") as f:
            content = f.read()

        checksums = content.split("\n")

        new_checksums = RaxelBuildUtil.generate_checksum(
            [build_options.project_dir, RaxelToolUtil.get_raxel_dir()]
        )
        return (checksums[0] == new_checksums[0], checksums[1] == new_checksums[1])

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
            f"-DRAXEL_PROJECT_ROOT_DIR={rel_project_dir}",
            f"-DCMAKE_BUILD_TYPE={RaxelBuildType.to_clean_str(options.build_type)}",
        ]

        print(f"Configuring with CMake: {' '.join(cmake_command)}\n")

        try:
            subprocess.run(cmake_command, check=True, cwd=work_dir)
        except subprocess.CalledProcessError as e:
            print(f"Error configuring with CMake: {e}")
            return

        # 5. Run the build for the requested project target
        build_command = [
            "cmake",
            "--build",
            build_dir,
            "--target",
            options.project_name,
        ]
        print(f"Building project: {' '.join(build_command)}\n")
        try:
            subprocess.run(build_command, check=True, cwd=work_dir)
        except subprocess.CalledProcessError as e:
            cli_width = shutil.get_terminal_size().columns

            print("")
            print("=" * cli_width)
            print("")

            print(f"Error building project: {e}")
            return

        # now we should build a checksum of the project, and store it in the build directory for later use
        RaxelBuildUtil.create_build_checksum(options)

        print(f"Build complete. The executable should be in {build_dir}")

    @staticmethod
    def run_project(options: RaxelBuildOptions, use_gdb: bool = False):
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
        executable_name = (
            options.project_name + ".exe" if os.name == "nt" else options.project_name
        )

        # Check if the build directory exists
        if not os.path.exists(build_dir):
            print(f"Error: Build directory does not exist: {build_dir}")
            return

        if not os.path.exists(os.path.join(build_dir, executable_name)):
            print(
                f"Error: Project executable does not exist in build directory: {build_dir}"
            )
            return
        

        source_valid, shader_valid =  RaxelBuildUtil.is_checksum_valid(options)

        if not source_valid or not shader_valid:
            # print in a nice red color that there have been changes to the project
            print(
                "\033[91m"
                + "Error: The project has changed since the last build. Please rebuild the project."
                + "\033[0m"
            )
            print("We recommend running the following command(s):\n")
            if not source_valid:
                print("    raxel build\n")
            if not shader_valid:
                print("    raxel sc\n")
            print(
                "However, you can still run the project with the current build if you want.\n"
            )

        # Run the project
        executable_path = os.path.join(build_dir, executable_name)
        if use_gdb:
            run_command = ["gdb", executable_path]
            print(f"Running project with gdb: {' '.join(run_command)}\n")
        else:
            run_command = [executable_path]
            print(f"Running project: {' '.join(run_command)}\n")

        try:
            subprocess.run(run_command, check=True, cwd=build_dir)
        except subprocess.CalledProcessError as e:
            print(f"Error running project: {e}")
            return
        
    @staticmethod
    def compile_shaders(project_dir: str):
        raxel_sc_script = os.path.join(RaxelToolUtil.get_raxel_tool_dir(), "raxel_sc.sh")
        if not os.path.exists(raxel_sc_script):
            print(f"Error: The raxel shader compiler script {raxel_sc_script} does not exist")
            return

        # raxel_sc.sh <project_root> <raxel_root>
        os.system(f"bash {raxel_sc_script} {project_dir} {RaxelToolUtil.get_raxel_dir()}")
