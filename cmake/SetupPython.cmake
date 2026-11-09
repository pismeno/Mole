# SetupPython.cmake
include(FetchContent)

if(WIN32)
    set(PORTABLE_PYTHON_DIR "${CMAKE_BINARY_DIR}/tools/python")
    set(PORTABLE_PYTHON_EXE "${PORTABLE_PYTHON_DIR}/python.exe")
    set(PORTABLE_SITE_PACKAGES "${PORTABLE_PYTHON_DIR}/Lib/site-packages")
    set(PYTHON_SETUP_STAMP "${PORTABLE_PYTHON_DIR}/.setup_done")
    set(EDITOR_TOOLS_DIR "${CMAKE_SOURCE_DIR}/src/editor/tools")
    set(EDITOR_TOOLS_REQUIREMENTS "${EDITOR_TOOLS_DIR}/py_requirements.txt")

    # Tell CMake to re-run configuration if the requirements file changes
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EDITOR_TOOLS_REQUIREMENTS}")

    FetchContent_Declare(
            portable_python
            URL "https://www.python.org/ftp/python/3.12.8/python-3.12.8-embed-amd64.zip"
            SOURCE_DIR "${PORTABLE_PYTHON_DIR}"
    )
    FetchContent_MakeAvailable(portable_python)

    # Determine if we need to run pip install
    set(REQUIRES_INSTALL FALSE)

    if(NOT EXISTS "${PYTHON_SETUP_STAMP}")
        set(REQUIRES_INSTALL TRUE)
    elseif("${EDITOR_TOOLS_REQUIREMENTS}" IS_NEWER_THAN "${PYTHON_SETUP_STAMP}")
        set(REQUIRES_INSTALL TRUE)
        message(STATUS "Python requirements changed, scheduling clean install...")
    endif()

    if(REQUIRES_INSTALL)
        message(STATUS "Configuring embedded Python environment...")

        # 1. Write the tools path directly into the ._pth file
        file(WRITE "${PORTABLE_PYTHON_DIR}/python312._pth"
                "python312.zip
                .
                ${EDITOR_TOOLS_DIR}
                # Uncomment to run site.main() automatically
                #import site

                Lib/site-packages
                "
        )

        # 2. Clean the existing site-packages BEFORE bootstrapping pip
        if(EXISTS "${PORTABLE_SITE_PACKAGES}")
            message(STATUS "Cleaning old Python packages...")
            file(REMOVE_RECURSE "${PORTABLE_SITE_PACKAGES}")
        endif()

        # 3. Always bootstrap pip on a clean install (since step 2 wiped it)
        file(DOWNLOAD "https://bootstrap.pypa.io/get-pip.py" "${PORTABLE_PYTHON_DIR}/get-pip.py")
        execute_process(
                COMMAND "${PORTABLE_PYTHON_EXE}" "${PORTABLE_PYTHON_DIR}/get-pip.py" --no-warn-script-location
                WORKING_DIRECTORY "${PORTABLE_PYTHON_DIR}"
        )
        file(REMOVE "${PORTABLE_PYTHON_DIR}/get-pip.py")

        # Convert path to native Windows format for the pip CLI
        file(TO_NATIVE_PATH "${EDITOR_TOOLS_REQUIREMENTS}" NATIVE_REQ_PATH)

        # 4. Install packages cleanly from py_requirements.txt
        message(STATUS "Installing Python packages from py_requirements.txt...")
        execute_process(
                COMMAND "${PORTABLE_PYTHON_EXE}" -m pip install -r "${NATIVE_REQ_PATH}" --target "${PORTABLE_SITE_PACKAGES}" --no-user --no-warn-script-location
                WORKING_DIRECTORY "${PORTABLE_PYTHON_DIR}"
                RESULT_VARIABLE PIP_INSTALL_RES
        )

        # 5. Touch the stamp file ONLY if pip succeeded
        if(PIP_INSTALL_RES EQUAL 0)
            file(TOUCH "${PYTHON_SETUP_STAMP}")
            message(STATUS "Python environment successfully configured.")
        else()
            message(FATAL_ERROR "Failed to install Python requirements! Check the errors above.")
        endif()
    endif()

    set(ENGINE_PYTHON_EXECUTABLE "${PORTABLE_PYTHON_EXE}" CACHE FILEPATH "Engine portable python" FORCE)

    message(STATUS "Generating pyrightconfig.json for static analysis...")
    set(PYRIGHT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/pyrightconfig.json")

    file(WRITE "${PYRIGHT_CONFIG_FILE}" "{
      \"pythonVersion\": \"3.12\",
      \"reportMissingTypeArgument\": \"warning\",
      \"reportPrivateLocalImportUsage\": \"none\",
      \"extraPaths\": [
        \"${PORTABLE_SITE_PACKAGES}\",
        \"${EDITOR_TOOLS_DIR}\"
      ],
      \"executionEnvironments\": [
        {
          \"root\": \"${EDITOR_TOOLS_DIR}\"
        }
      ]
    }
    ")
endif()