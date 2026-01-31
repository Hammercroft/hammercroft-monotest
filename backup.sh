#!/bin/bash

# Get current date and time
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Define backup name
BACKUP_NAME="monotest_backup_${TIMESTAMP}.tar.gz"

# Move to parent directory and tar the directory
# Assuming the script is run from inside the MONOTEST directory
cd .. && tar -czvf "${BACKUP_NAME}" MONOTEST

echo "Backup created: ${BACKUP_NAME}"
