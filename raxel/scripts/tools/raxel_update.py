# raxel_update.py

from tools.tool_util import RaxelToolUtil
import subprocess

def main(args):
    raxel_root = RaxelToolUtil.get_root_dir()

    # now we can run a git pull command, but first ask the user if they want to
    print("Attempting to update Raxel")
    print("This will run a git pull command in the Raxel directory")
    print("Specifically, this will run 'git pull' in the following directory:")
    print(raxel_root)

    # ask the user if they want to continue
    while True:
        user_input = input("Continue? (y/n): ")
        if user_input.lower() == "y":
            break
        elif user_input.lower() == "n":
            print("Exiting")
            return
        else:
            print("Invalid input")

    # run the git pull command
    print("Running git pull")
    try:
        subprocess.run(["git", "pull"], cwd=raxel_root, check=True)
    except subprocess.CalledProcessError as e:
        print(f"Error running git pull: {e}")
        return
