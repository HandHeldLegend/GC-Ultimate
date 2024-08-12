# Set the path to your header file
set(VERSION_HEADER "${CMAKE_SOURCE_DIR}/include/board_config.h")

# Set the path to your manifest.json
set(MANIFEST_JSON "${CMAKE_SOURCE_DIR}/${TARGET_NAME}_manifest.json")

# Check if manifest.json exists, create it if it doesn't
if(NOT EXISTS ${MANIFEST_JSON})
message(STATUS "${TARGET_NAME}_manifest.json not found. Creating a new one.")
    file(WRITE ${MANIFEST_JSON} "{\n  \"changelog\": \"\",\n  \"fw_version\": 0,\n  \"checksum\": \"\"\n}")
endif()

set(BIN_FILE "${CMAKE_SOURCE_DIR}/build/${TARGET_NAME}.bin")

# Check if BIN_FILE is provided as an argument
if(NOT DEFINED BIN_FILE)
    message(FATAL_ERROR "BIN_FILE not provided. Please call this script with -DBIN_FILE=/path/to/your/firmware.bin")
endif()

# Verify that the BIN file exists
if(NOT EXISTS ${BIN_FILE})
    message(FATAL_ERROR "The specified BIN file does not exist: ${BIN_FILE}")
endif()

# Read the FW version from the header file
file(READ ${VERSION_HEADER} VERSION_CONTENT)

# Extract the version number from the file content
string(REGEX MATCH "HOJA_FW_VERSION[ ]*0x([0-9A-Fa-f]+)" VERSION_HEX "${VERSION_CONTENT}")

if (NOT VERSION_HEX)
    message(FATAL_ERROR "Version not found in ${VERSION_HEADER}")
else()
    string(TOUPPER "${CMAKE_MATCH_1}" VERSION_HEX_UPPER)
    math(EXPR VERSION_INT "0x${VERSION_HEX_UPPER}")

    # Calculate SHA256 checksum of the BIN file
    file(SHA256 ${BIN_FILE} BIN_CHECKSUM)

    # Read the existing JSON data from manifest.json
    file(READ ${MANIFEST_JSON} MANIFEST_JSON_CONTENT)

    # Find and replace the "version" key in JSON content
    string(REGEX REPLACE "\"fw_version\"[ ]*:[ ]*[0-9]+" "\"fw_version\": ${VERSION_INT}" UPDATED_JSON "${MANIFEST_JSON_CONTENT}")

    # Find and replace the "checksum" key in JSON content
    string(REGEX REPLACE "\"checksum\"[ ]*:[ ]*\"[a-fA-F0-9]*\"" "\"checksum\": \"${BIN_CHECKSUM}\"" UPDATED_JSON "${UPDATED_JSON}")

    # Write the updated JSON content back to manifest.json
    file(WRITE ${MANIFEST_JSON} ${UPDATED_JSON})

    message(STATUS "Processed BIN file: ${BIN_FILE}")
    # Print the extracted version
    message(STATUS "Extracted version: ${VERSION_INT}")
    message(STATUS "Calculated checksum: ${BIN_CHECKSUM}")
endif()
