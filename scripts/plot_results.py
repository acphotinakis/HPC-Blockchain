#!/usr/bin/env python3
# Python script for plotting performance results (e.g., Matplotlib)
# To run, create a venv, install the necessary libraries, and start venv using: "source {venvName}/bin/activate"
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import os

working_directory = os.getcwd()
metrics_path = "../metrics"

contents = os.listdir(metrics_path)
print(contents)

# Only creating graphs for simulation metrics right now as it provides execution data
simulation_csv = f"{metrics_path}/simulation_metrics.csv"
df = pd.read_csv(simulation_csv)

# Group number of processors, shards, and transaction sizes by the average execution time
grouped = df.groupby(['NP', 'NumShards', 'NumTransactions'])['TotalTime'].mean().reset_index()
grouped = grouped.rename(columns={'TotalTime': 'AvgTime'})  # Change our total time column to average time
# Combine NP, NumShards, NumTransactions into one column (Config)
grouped['Config'] = grouped.apply(lambda x: f"({x['NP']}, {x['NumShards']}, {x['NumTransactions']})", axis=1)

# Init our graph
fig, ax = plt.subplots(figsize=(12, 8))

unique_configs = grouped['Config'].unique()  # Separate unique configuration
unique_np = sorted(grouped['NP'].unique())  # Sort the configurations by number of processors
num_np = len(unique_np)  # Number of groups
bar_width = 0.8 / num_np  # 0.8 is the best factor to align the y-axis labels
x_pos = np.arange(len(unique_configs))  # Evenly space our configs

# Creation of graph below
for i, np_val in enumerate(unique_np):
    # Filter data for current NP
    np_data = grouped[grouped['NP'] == np_val]
    
    # Get values in correct order matching unique configurations
    values = []
    for config in unique_configs:
        match = np_data[np_data['Config'] == config]
        if not match.empty:
            values.append(match['AvgTime'].values[0])
        else:
            values.append(0)
    
    # Calculate position for this processor count's bars
    positions = x_pos + i * bar_width - (num_np - 1) * bar_width / 2
    # Create bars
    bars = ax.bar(positions, values, bar_width, label=f'NP={np_val}')

# Style for plot
ax.set_xlabel('Configuration (NP, NumShards, NumTransactions)')
ax.set_ylabel('Average Execution Time')
ax.set_title('Average Execution Time by Configuration')
ax.set_xticks(x_pos)
ax.set_xticklabels(unique_configs, rotation=45, ha='right')
ax.legend(title='NP Legend')
ax.grid(axis='y', alpha=0.3)  # Alpha for partially transparent axis lines

# Save plot as picture
plt.tight_layout()  # Gives more padding
plt.savefig('simulation_metrics_plot.png')
plt.close()