#!/bin/bash
# MPI experiment execution script.
# This script builds the project and runs a simulation experiment.

# --- Configuration ---
# Number of MPI processes to use for the simulation.
# Usage: ./scripts/run_experiment.sh [num_processes] "[sim_args]"
# Example: ./scripts/run_experiment.sh 16 "--shards 4 --transactions 20000"
NP=${1:-8}

# Arguments to pass to the simulation executable.
ARGS=${2:-"--shards 2 --transactions 10000 --run-id 1"}

# --- Script ---
set -e # Exit immediately if a command exits with a non-zero status.

echo "Building the project..."
make all

echo "Running experiment on ${NP} processes..."
echo "Arguments: ${ARGS}"
mpirun -np "${NP}" build/main ${ARGS}

echo "Experiment finished."