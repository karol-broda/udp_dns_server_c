#!/bin/sh
set -e

# Set the path to the include directory
INCLUDE_DIR="./lib"

# Create a temporary file for the executable
tmpFile=$(mktemp)

# Compile the server and cJSON together, specifying the include directory
gcc -o $tmpFile app/server.c lib/cJSON/cJSON.c -I$INCLUDE_DIR

# Execute the compiled program
exec "$tmpFile" "$@"
