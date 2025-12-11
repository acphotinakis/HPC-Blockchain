#!/usr/bin/env python3
import subprocess
import itertools
import os

# Define NP values, shards, and transaction counts
np_values = [8, 16, 32, 64]
shards = [1, 4, 8, 16, 32]
transactions = [10000, 50000, 100000, 200000, 500000]

# Set MPI to allow oversubscription
os.environ["MPIEXEC_PREFLAGS"] = "--map-by :OVERSUBSCRIBE"

# Loop through all valid combinations
run_id = 0
for np_val, shard_count, tx_count in itertools.product(np_values, shards, transactions):
    # Skip invalid combinations: need at least 1 process for Final Committee
    if np_val < shard_count + 1:
        
        if np_val != 1:
            print(f"Skipping NP={np_val}, shards={shard_count} (not enough nodes)")
            continue

    run_id += 1
    seed = 1234 + run_id
    args = f"--shards {shard_count} --transactions {tx_count} --run-id {run_id} --seed {seed}"
    print("-" * 60)
    print(f"Running make run with NP={np_val} ARGS=\"{args}\"")
    subprocess.run(["make", "run", f"NP={np_val}", f"ARGS={args}"])
    print("-" * 60)

print("All simulations completed.")
