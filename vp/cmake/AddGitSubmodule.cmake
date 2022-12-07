#thanks @ https://gist.github.com/scivision/bb1d47a9529e153617414e91ff5390af

cmake_minimum_required(VERSION 3.16)

function(add_git_submodule dir)
# add a Git submodule directory to CMake, assuming the
# Git submodule directory is a CMake project.
#
# Usage: in CMakeLists.txt
# 
# include(AddGitSubmodule.cmake)
# add_git_submodule(mysubmod_dir)

find_package(Git REQUIRED)
if(NOT EXISTS ${dir})
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
endif()

if(NOT EXISTS "${dir}/CMakeLists.txt")
  message("checking out submodule ${dir}")
  execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${abs_dir}
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    #COMMAND_ERROR_IS_FATAL		# cmake 3.19+
    #ANY						# cmake 3.19+
   )
endif()

add_subdirectory(${dir})

endfunction(add_git_submodule)