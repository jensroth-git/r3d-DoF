find_program(PYTHON_EXECUTABLE python3 REQUIRED)

# Inline Python script to generate the main assets.h header
set(ASSETS_HEADER_GENERATOR_SCRIPT "
import sys
import os

def generate_assets_header(asset_names, output_file):
    with open(output_file, 'w') as f:
        f.write('#ifndef ASSETS_H\\n')
        f.write('#define ASSETS_H\\n\\n')
        f.write('// Auto-generated header that includes all asset headers\\n\\n')
        
        for asset_name in asset_names:
            f.write(f'#include \"./assets/{asset_name}.h\"\\n')
        
        f.write('\\n#endif // ASSETS_H\\n')

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('Usage: python script.py <output_file> <asset_name1> [asset_name2] ...')
        sys.exit(1)
    
    output_file = sys.argv[1]
    asset_names = sys.argv[2:]
    generate_assets_header(asset_names, output_file)
")

# Create the temporary Python script only once
file(WRITE "${CMAKE_BINARY_DIR}/generate_assets_header.py" "${ASSETS_HEADER_GENERATOR_SCRIPT}")

# Internal function to create a custom command for an asset
function(_add_asset_command asset_file output_file)
    get_filename_component(asset_name "${asset_file}" NAME)
    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated/include/assets"
        COMMAND ${PYTHON_EXECUTABLE} "${R3D_ROOT_PATH}/scripts/bin2c.py" -f "${asset_file}" -n "${asset_name}" "${output_file}"
        DEPENDS "${asset_file}"
        COMMENT "Processing asset: ${asset_file}"
        VERBATIM
    )
endfunction()

function(_add_assets_header_command asset_names output_file)
    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/generated/include"
        COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_BINARY_DIR}/generate_assets_header.py" 
                "${output_file}" ${asset_names}
        COMMENT "Generating main assets header: ${output_file}"
        VERBATIM
    )
endfunction()

# Public function to process an asset (used by EmbedAssets)
function(ProcessAsset asset_file output_file_var)
    # Extract the file name without the path to use as the array name
    get_filename_component(asset_name "${asset_file}" NAME)
    # Define the output file
    set(output_file "${CMAKE_BINARY_DIR}/generated/include/assets/${asset_name}.h")
    # Return the output file via the variable
    set("${output_file_var}" "${output_file}" PARENT_SCOPE)
    # Create the custom command
    _add_asset_command("${asset_file}" "${output_file}")
endfunction()

function(EmbedAssets target_name)
    # Retrieve the list of asset files (all arguments except the first)
    set(asset_files ${ARGN})
    # Ensure at least one file has been provided
    if(NOT asset_files)
        message(FATAL_ERROR "EmbedAssets: No asset file specified")
    endif()
    
    # List to store all output files and asset names
    set(output_files)
    set(asset_names)
    # Count the number of assets
    list(LENGTH asset_files num_assets)
    message(STATUS "Configuring processing of ${num_assets} asset(s) for target ${target_name}...")
    
    # Process each asset file
    foreach(asset_file ${asset_files})
        # Ensure the file exists
        if(NOT EXISTS "${asset_file}")
            message(FATAL_ERROR "EmbedAssets: Asset file not found: ${asset_file}")
        endif()
        
        get_filename_component(asset_name "${asset_file}" NAME)
        list(APPEND asset_names "${asset_name}")
        
        # Process the asset and retrieve the output file
        ProcessAsset("${asset_file}" output_file)
        list(APPEND output_files "${output_file}")
        message(STATUS "  - ${asset_file} -> assets/${asset_name}.h")
    endforeach()
    
    # Generate the main assets.h header
    set(main_assets_header "${CMAKE_BINARY_DIR}/generated/include/assets.h")
    _add_assets_header_command("${asset_names}" "${main_assets_header}")
    list(APPEND output_files "${main_assets_header}")
    
    # Create a custom target to group all assets
    set(assets_target "${target_name}_assets")
    add_custom_target(${assets_target}
        DEPENDS ${output_files}
        COMMENT "Generating asset headers for ${target_name}"
    )
    
    # Add dependency to the main target
    add_dependencies(${target_name} ${assets_target})
    # Add the generated include directory to the target
    target_include_directories(${target_name} PRIVATE "${CMAKE_BINARY_DIR}/generated/include")
    
    message(STATUS "Target ${assets_target} created with ${num_assets} asset(s)")
    message(STATUS "Main assets header: ${main_assets_header}")
    message(STATUS "Assets will be automatically recompiled if modified")
endfunction()
