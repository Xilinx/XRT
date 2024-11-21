import re

def extract_variables_from_cpp():
    """
    This function reads the C++ header file, extracts variable names and their hexadecimal values
    within the 'namespace X', and returns a dictionary of variable names and their values.
    """
    #variables = {}
    variables = []
    inside_namespace = False

    try:
        # Open the C++ header file and read its content line by line
        with open("aie2_registers.h", 'r') as file:
            for line in file:
                # Check if we're inside namespace X
                if 'namespace aie2 {' in line:
                    inside_namespace = True
                    continue
                if '}' in line and inside_namespace:
                    inside_namespace = False
                    continue

                if inside_namespace:
                    # Regular expression to match variables like: const unsigned int var_name = 0x....
                    #match = re.match(r'\s*//.*|/\*.*?\*/|const\s+unsigned\s+int\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+)\s*;', line)
                    match = re.match(r'\s*//.*|/\*.*?\*/|const\s+unsigned\s+int\s+(\w+)\s*=\s*(0x[0-9a-fA-F]+)\s*;', line)
                    if match:
                        #var_name, hex_value = match.groups()
                        #variables[var_name] = hex_value  # Store variable and its hexadecimal value
                        var_name = match.group(1)
                        variables.append('{"%s" , aie2::%s}' % (var_name, var_name))

        return variables

    except IOError:
        print("Error: The file was not found.")
        return {}

def write_to_log_file(variables):
    """
    This function writes the dictionary of variables to a log file in the format:
    {"a1":a, "a2":a2, "b":b, ..., "c10":c10}
    """
    if variables:
        # Open (or create) the log file in write mode
        with open("pythonlogfile2.txt", "w") as log_file:
            #for var_name, hex_value in variables.items():
            for line in variables:
                #log_file.write('{{"{}": "{}"}},\n'.format(var_name, hex_value))  # Write each variable on a new line
                if line.startswith('{"None"'):
                    continue
                else:
                    log_file.write(line + ",\n")  # Write each variable on a new line
        print("Dictionary has been written to pythonlogfile2.txt")

def main():
    # Ask the user for the filename of the C++ header file
    #header_file = input("Enter the C++ header file name (e.g., abc.h): ")

    # Extract the variables and their values from the C++ header file
    variables = extract_variables_from_cpp()

    # Write the extracted variables to the log file
    write_to_log_file(variables)

if __name__ == "__main__":
    main()
