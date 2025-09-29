# 1) Setting up build

## a) Pre-Requisits.
Make sure you have the following tools installed on your platform.
- CMake >= 3.10 
- Clang 20.1.8
- Python >= 3.10
- Ninja
- Git

## b) Cmake User Presets
In the project root create a file: *"CMakeUserEnv.json"* This file is ignored by git.
Copy the following template in the code and for the environment variables *"USER_CLANG_20_PATH"* and *"USER_CLANG++_20_PATH"* replace the string values with the full path to your installed clang20 executeables. (To check the version of the compiler, in that directory run `./clang -v`). 
``` json
{
  "version": 9,
  "configurePresets": [
    {
      "name": "user_clang_paths",
      "environment": {
        "USER_CLANG_20_PATH": "/Path/to/clang20/executeable",
        "USER_CLANG++_20_PATH": "/Path/to/clang20++/executeable"
      }
  }]
}
``` 

### My Windows Example :) 

```json
{
  "version": 9,
  "configurePresets": [
    {
      "name": "user_clang_paths",
      "environment": {
        "USER_CLANG_20_PATH": "C:/Program Files/LLVM/bin/clang.exe",
        "USER_CLANG++_20_PATH": "C:/Program Files/LLVM/bin/clang++.exe"
      }
  }]
}
```

## c) Submodules:
In the project root run `git submodule update --init --recursive`
This will populate the submodules directory with external dependencies for this project. 

## d) [optional] pre-commit formatting 
If this project is being used in a fork, it is recommended to setup the pre-commit hook to run clang-format.
1) `pip install pre-commit`
2) In project root run `pre-commit install`

The pre-commit hook will auto run when you use `git commit` and will only format staged files.
It manually trigger formatting for all staged files with  `pre-commit run`, or format all files in the dir with `pre-commit run --all-files`. 

# 2) Building the project.
The project is build with cmake and has two primary build presets.

## Default build
All source is built into a single executeable file inside the cmake build folder.
just run the exe found in `build/bin/<CMAKE_BUILD_TYPE>/DeckCAD`

## Hot Reload build
Enables hot-reloading and sets the `cmake_build_type` to debug.
- "Main" is build into the "DeckCAD" executeable.
- Graphics module is built as dynamic library that is compiletime linked.
- Core module is build as dynamic library without any linking, the library is then loaded at runtime in Main by using the LibraryLoader.
    - By Dynamically loading the Core Module, it allows us to preserve the memory context of the app while being able to modify and re-compile the Core runtime logic.

### Building in commandline
*In project root*
1) Generate cmake build `cmake -B./build`
2) Build project  `cmake --build --preset Default` or  `cmake --build --preset HotReload` 
