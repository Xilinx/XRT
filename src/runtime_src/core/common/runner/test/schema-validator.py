# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#
# pip install jsonschema
import json
from jsonschema import validate, ValidationError, SchemaError
import argparse

def load_json_file(file_path):
    """Load a JSON file from the specified path."""
    try:
        with open(file_path, 'r') as file:
            return json.load(file)
    except FileNotFoundError:
        print(f"Error: File not found at {file_path}")
        exit(1)
    except json.JSONDecodeError as e:
        print(f"Error: Failed to parse JSON file at {file_path}. {e}")
        exit(1)

def validate_json(json_data, schema_data):
    """Validate the JSON data against the schema."""
    try:
        validate(instance=json_data, schema=schema_data)
        print("JSON is valid against the schema.")
    except ValidationError as e:
        print(f"Validation Error: {e.message}")
    except SchemaError as e:
        print(f"Schema Error: {e.message}")

def main():
    # Set up argument parser
    parser = argparse.ArgumentParser(description="Validate a JSON file against a JSON Schema.")
    parser.add_argument("json_file", help="Path to the JSON file to be validated.")
    parser.add_argument("schema_file", help="Path to the JSON Schema file.")

    # Parse command-line arguments
    args = parser.parse_args()

    # Load the JSON and schema files
    json_data = load_json_file(args.json_file)
    schema_data = load_json_file(args.schema_file)

    # Validate the JSON against the schema
    validate_json(json_data, schema_data)

if __name__ == "__main__":
    main()
