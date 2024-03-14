import subprocess
import sys
import os
import csv
from io import StringIO

LIBSEE_SEPARATOR = "----------------------------------LIBSEE----------------------------------------"

def run_command_with_ld_preload(new_ld_preload: str, *args):
    original_ld_preload = os.environ.get("LD_PRELOAD", "")
    if original_ld_preload:
        new_ld_preload = f"{new_ld_preload}:{original_ld_preload}"

    # Prepare the environment for the subprocess
    env = os.environ.copy()
    env["LD_PRELOAD"] = new_ld_preload

    # Prepare the command based on the arguments provided
    command = list(args)

    # Flag to indicate if we are within the LIBSEE stats section
    in_libsee_section = False

    # Execute the command and capture the output
    with subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, env=env) as proc:
        for line in iter(proc.stdout.readline, ''):
            # Check if the LIBSEE stats section has started
            if LIBSEE_SEPARATOR in line:
                in_libsee_section = not in_libsee_section
                if in_libsee_section:
                    # We are starting the LIBSEE section, prepare to collect CSV data
                    csv_data = StringIO()
                else:
                    # We are ending the LIBSEE section, print the table
                    csv_data.seek(0)  # Go back to the beginning of the StringIO buffer
                    print_libsee_section_as_table(csv_data)
                    csv_data.close()  # Close the StringIO object
            else:
                # Print normally if not in the LIBSEE section
                if not in_libsee_section:
                    print(line, end='')
                else:
                    # Add line to CSV data
                    csv_data.write(line)

        returncode = proc.wait()  # Wait for the process to finish and get the return code

    return returncode

def print_libsee_section_as_table(csv_input):
    csv_reader = csv.reader(csv_input)
    rows = list(csv_reader)
    
    if rows:  # Check if there are any rows to print
        # Determine the width of each column based on the longest piece of data in each
        column_widths = [max(len(str(item)) for item in column) for column in zip(*rows)]
        
        # Print the table
        for row in rows:
            print(' | '.join(f'{item:<{column_widths[index]}}' for index, item in enumerate(row)))
            # After printing the header, print a separator line
            if rows.index(row) == 0:
                print('-' * (sum(column_widths) + len(column_widths) * 3 - 1))  # Adjust '-' count based on column widths and separators


def print_libsee_section_as_table(csv_input):
    csv_reader = csv.reader(csv_input)
    rows = list(csv_reader)
    
    # Find the maximum length of each column for proper alignment
    column_widths = [max(len(str(item)) for item in column) for column in zip(*rows)]
    
    # Print the table headers
    print('{:<{}} {:<{}} {:<{}}'.format('Function', column_widths[0], 'Total CPU Cycles', column_widths[1], 'Total Calls', column_widths[2]))
    print('-' * sum(column_widths) + '-----')  # Adjust for the number of columns and spaces
    
    # Print each row
    for row in rows:
        print('{:<{}} {:<{}} {:<{}}'.format(row[0], column_widths[0], row[1], column_widths[1], row[2], column_widths[2]))


if __name__ == "__main__":
    new_ld_preload = "/home/ubuntu/LibSee/libsee.so"
    return_code = run_command_with_ld_preload(new_ld_preload, *sys.argv[1:])
    sys.exit(return_code)
