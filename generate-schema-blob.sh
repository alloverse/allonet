#!/bin/sh
set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

echo "static const unsigned char alloverse_schema_bytes[] = {" > "$SCRIPT_DIR/src/alloverse_binary_schema.h"
hexdump -ve '1/1 "0x%02x, "' "$SCRIPT_DIR/include/allonet/schema/alloverse.bfbs" >> "$SCRIPT_DIR/src/alloverse_binary_schema.h"
echo "0x00}; static const int alloverse_schema_size = sizeof(alloverse_schema_bytes); " >> "$SCRIPT_DIR/src/alloverse_binary_schema.h"