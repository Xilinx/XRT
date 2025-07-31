# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
import json
import csv
import sys
import os
import tkinter as tk
from tkinter import messagebox

# Define the available properties (based on your schema)
PROPERTY_MAP = {
    'cpu_elapsed': ('cpu', 'elapsed'),
    'cpu_iterations': ('cpu', 'iterations'),
    'cpu_latency': ('cpu', 'latency'),
    'cpu_throughput': ('cpu', 'throughput'),
    'hwctx_columns': ('hwctx', 'columns'),
    'resources_buffers': ('resources', 'buffers'),
    'resources_kernels': ('resources', 'kernels'),
    'resources_runlist': ('resources', 'runlist'),
    'resources_runlist_threshold': ('resources', 'runlist_threshold'),
    'resources_runs': ('resources', 'runs'),
    'resources_total_buffer_size': ('resources', 'total_buffer_size'),
    'xclbin_uuid': ('xclbin', 'uuid')
}

def select_properties_dialog(properties):
    root = tk.Tk()
    root.title("Select JSON Properties for CSV")
    vars = {}
    for i, prop in enumerate(properties):
        var = tk.BooleanVar(value=True)
        chk = tk.Checkbutton(root, text=prop, variable=var)
        chk.grid(row=i, sticky='w')
        vars[prop] = var

    def on_ok():
        root.quit()
        root.destroy()

    btn = tk.Button(root, text="OK", command=on_ok)
    btn.grid(row=len(properties), pady=10)
    root.mainloop()
    selected = [prop for prop, var in vars.items() if var.get()]
    return selected

def json_to_csv(json_file, csv_file, selected_props):
    with open(json_file, 'r') as f:
        data = json.load(f)
    jobs = data.get('jobs', {})
    headers = ['job_name'] + selected_props
    rows = []
    for job_name, job_data in jobs.items():
        row = {'job_name': job_name}
        for prop in selected_props:
            section, key = PROPERTY_MAP[prop]
            row[prop] = job_data.get(section, {}).get(key, '')
        rows.append(row)
    with open(csv_file, 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        writer.writeheader()
        writer.writerows(rows)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {os.path.basename(sys.argv[0])} input.json output.csv")
        sys.exit(1)
    json_path, csv_path = sys.argv[1], sys.argv[2]
    if not os.path.isfile(json_path):
        print(f"Error: File '{json_path}' does not exist.")
        sys.exit(1)
    selected_props = select_properties_dialog(list(PROPERTY_MAP.keys()))
    if not selected_props:
        print("No properties selected. Exiting.")
        sys.exit(0)
    try:
        json_to_csv(json_path, csv_path, selected_props)
        print(f"CSV file saved to {csv_path}")
    except Exception as e:
        print(f"Failed to convert JSON to CSV: {e}")

if __name__ == '__main__':
    main()

    
