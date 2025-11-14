#!/bin/bash

# generate_project.sh
# Description: Creates the full directory and file structure for the
# Sharded Blockchain MPI project based on the project documents.

# --- Configuration ---
PROJECT_ROOT="."

# --- Main Directories ---
MAIN_DIRS=(
    "build"
    "src"
    "tests"
    "resources"
    "scripts"
)

# --- Include Header Subdirectories ---
INCLUDE_SUBS=(
    "include/sbmpi/core"
    "include/sbmpi/consensus"
    "include/sbmpi/network"
    "include/sbmpi/util"
)

# --- Source Subdirectories (Mirroring Includes) ---
SRC_SUBS=(
    "src/core"
    "src/consensus"
    "src/network"
    "src/util"
)

# --- Files to Create (outside main subdirs) ---
ROOT_FILES=(
    "Makefile"
)
TEST_FILES=(
    "tests/test_pbft.cpp"
    "tests/test_sharding.cpp"
)
SCRIPT_FILES=(
    "scripts/run_experiment.sh"
    "scripts/plot_results.py"
)
# DOC_FILES=(
#     "docs/PROPOSAL.md"
#     "docs/IMPLEMENTATION_PLAN.md"
#     "docs/STRUCTURE.md"
#     "docs/PERFORMANCE.md"
# )

# --- Header Files (Placeholders only) ---
HEADER_FILES=(
    "include/sbmpi/core/block.h"
    "include/sbmpi/core/blockchain.h"
    "include/sbmpi/core/transaction.h"
    "include/sbmpi/core/node.h"
    "include/sbmpi/consensus/pbft.h"
    "include/sbmpi/network/shard.h"
    "include/sbmpi/network/final_committee.h"
    "include/sbmpi/util/config.h"
    "include/sbmpi/util/logging.h"
    "include/sbmpi/util/timer.h"
)

# --- Source Files (Placeholders only) ---
SOURCE_FILES=(
    "src/main.cpp"
    "src/core/block.cpp"
    "src/core/blockchain.cpp"
    "src/core/transaction.cpp"
    "src/core/node.cpp"
    "src/consensus/pbft.cpp"
    "src/network/shard.cpp"
    "src/network/final_committee.cpp"
    "src/util/logging.cpp"
    "src/util/timer.cpp"
)

# ----------------------------------------------------------------------
# FUNCTION DEFINITIONS
# ----------------------------------------------------------------------

# Creates all directories recursively.
create_dirs() {
    echo "Creating directories..."
    # mkdir -p "$PROJECT_ROOT"
    
    # Create main directories
    for dir in "${MAIN_DIRS[@]}"; do
        mkdir -p "$PROJECT_ROOT/$dir"
    done
    
    # Create include and source subdirectories
    for dir in "${INCLUDE_SUBS[@]}"; do
        mkdir -p "$PROJECT_ROOT/$dir"
    done
    for dir in "${SRC_SUBS[@]}"; do
        mkdir -p "$PROJECT_ROOT/$dir"
    done
    
    echo "Directory structure created successfully."
}

# Creates placeholder files for all defined file lists.
create_files() {
    echo "Creating placeholder files..."
    
    # Generic files
    for file in "${ROOT_FILES[@]}" "${TEST_FILES[@]}" "${SCRIPT_FILES[@]}"; do
        touch "$PROJECT_ROOT/$file"
    done
    
    # Header files
    for file in "${HEADER_FILES[@]}"; do
        echo "// C++ Header File (Declarations only)" > "$PROJECT_ROOT/$file"
    done
    
    # Source files (Implementation)
    for file in "${SOURCE_FILES[@]}"; do
        echo "// C++ Source File (Implementation logic)" > "$PROJECT_ROOT/$file"
    done

    # Add shebang and basic command to script files
    echo -e '#!/bin/bash\n# MPI experiment execution script for kraken' > "$PROJECT_ROOT/scripts/run_experiment.sh"
    echo -e '#!/usr/bin/env python3\n# Python script for plotting performance results (e.g., Matplotlib)' > "$PROJECT_ROOT/scripts/plot_results.py"

    echo "Placeholder files created successfully."
}

# --- Execution ---
main() {
    echo "Starting project structure generation for: $PROJECT_ROOT"
    
    if [ -d "$PROJECT_ROOT" ]; then
        echo "Warning: Directory $PROJECT_ROOT already exists."
        read -r -p "Do you want to delete and recreate it? (y/N) " response
        if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
            rm -rf "$PROJECT_ROOT"
            echo "Existing directory removed."
        else
            echo "Aborting script. No changes made."
            exit 0
        fi
    fi

    create_dirs
    create_files
    
    echo "---"
    echo "Project structure is ready in /$PROJECT_ROOT."
    echo "Next steps: Populate files and run 'make'."
}

main "$@"