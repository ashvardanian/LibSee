import subprocess
import sys
import os
import csv
from io import StringIO
import os
import subprocess
import sys
import csv
import threading
from io import StringIO
from time import sleep


LIBSEE_SEPARATOR = (
    "----------------------------------LIBSEE----------------------------------------"
)


def read_from_pipe(pipe_name, flag_list):
    with open(pipe_name, "r") as pipe:
        while not flag_list[0]:  # flag_list is used to signal when to stop the thread
            line = pipe.readline()
            if line:
                print(f"> {line}", end="")  # Process line here as before
            else:  # If readline returns an empty string, the writer has closed their end
                break


def run_command_with_ld_preload(new_ld_preload: str, *args):
    # Setup environment as before
    env = os.environ.copy()
    env["LD_PRELOAD"] = new_ld_preload

    # Create a named pipe
    pipe_name = "/tmp/my_named_pipe"
    try:
        os.mkfifo(pipe_name)
    except FileExistsError:
        pass  # The pipe already exists

    # Flag to control the reader thread
    flag_list = [False]  # Mutable container so it can be modified by the thread

    # Start the reader thread
    reader_thread = threading.Thread(target=read_from_pipe, args=(pipe_name, flag_list))
    reader_thread.start()

    # Construct the shell command
    shell_command = (
        " ".join(args) + f" > {pipe_name} 2>&1"
    )  # args should be a sequence of program arguments

    # Execute the command using shell
    proc = subprocess.Popen(shell_command, shell=True, env=env)
    proc.wait()
    sleep(5)

    # After the process finishes, signal the reader thread to stop and wait for it to finish
    flag_list[0] = True
    reader_thread.join()

    # Clean up: remove the named pipe
    os.unlink(pipe_name)

    return proc.returncode


def print_libsee_section_as_table(csv_input):
    csv_reader = csv.reader(csv_input)
    rows = list(csv_reader)

    if rows:  # Check if there are any rows to print
        # Determine the width of each column based on the longest piece of data in each
        column_widths = [
            max(len(str(item)) for item in column) for column in zip(*rows)
        ]

        # Print the table
        for row in rows:
            print(
                " | ".join(
                    f"{item:<{column_widths[index]}}" for index, item in enumerate(row)
                )
            )
            # After printing the header, print a separator line
            if rows.index(row) == 0:
                # Adjust '-' count based on column widths and separators
                print("-" * (sum(column_widths) + len(column_widths) * 3 - 1))


def print_libsee_section_as_table(csv_input):
    csv_reader = csv.reader(csv_input)
    rows = list(csv_reader)

    # Find the maximum length of each column for proper alignment
    column_widths = [max(len(str(item)) for item in column) for column in zip(*rows)]

    # Print the table headers
    print(
        "{:<{}} {:<{}} {:<{}}".format(
            "Function",
            column_widths[0],
            "Total CPU Cycles",
            column_widths[1],
            "Total Calls",
            column_widths[2],
        )
    )
    # Adjust for the number of columns and spaces
    print("-" * sum(column_widths) + "-----")

    # Print each row
    for row in rows:
        print(
            "{:<{}} {:<{}} {:<{}}".format(
                row[0],
                column_widths[0],
                row[1],
                column_widths[1],
                row[2],
                column_widths[2],
            )
        )


if __name__ == "__main__":
    new_ld_preload = "/home/ubuntu/LibSee/libsee.so"
    return_code = run_command_with_ld_preload(new_ld_preload, *sys.argv[1:])
    sys.exit(return_code)
