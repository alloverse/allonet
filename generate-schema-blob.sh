#!/bin/sh
set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# Input variables
OUT_FILE="$SCRIPT_DIR/src/alloverse_binary_schema.h"
SCHEMA_FILE="$SCRIPT_DIR/alloverse.fbs"
BINARY_FILE="$SCRIPT_DIR/include/allonet/schema/alloverse.bfbs"
VAR_NAME="alloverse_schema"

# Script

# Binary rep
echo "static const unsigned char ${VAR_NAME}_bytes[] = {" > "$OUT_FILE"
hexdump -ve '1/1 "0x%02x, "' "$BINARY_FILE" >> "$OUT_FILE"
echo "0x00}; static const int ${VAR_NAME}_size = sizeof(${VAR_NAME}_bytes); " >> "$OUT_FILE"

# Text rep
echo "static const unsigned char ${VAR_NAME}_text[] = {" >> "$OUT_FILE"
hexdump -ve '1/1 "0x%02x, "' "$SCHEMA_FILE" >> "$OUT_FILE"
echo "0x00}; static const int ${VAR_NAME}_text_size = sizeof(${VAR_NAME}_text); " >> "$OUT_FILE"
