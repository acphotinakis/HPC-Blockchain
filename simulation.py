#!/usr/bin/env python3
import subprocess
import itertools

# Define NP values, shards, and transaction counts
np_values = [8, 16, 32, 64]
shards = [4, 8, 16, 32]
transactions = [10000, 50000, 100000, 200000, 500000]

# Loop through all valid combinations
for np_val, shard_count, tx_count in itertools.product(np_values, shards, transactions):
    # Skip invalid combinations: need at least 1 process for Final Committee
    if np_val < shard_count + 1:
        print(f"Skipping NP={np_val}, shards={shard_count} (not enough nodes)")
        continue

    args = f"--shards {shard_count} --transactions {tx_count}"
    print("-" * 60)
    print(f"Running make run with NP={np_val} ARGS=\"{args}\"")
    subprocess.run(["make", "run", f"NP={np_val}", f"ARGS={args}"])
    print("-" * 60)

print("All simulations completed.")
