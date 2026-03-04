#!/bin/bash

# Define start and end dates (YYYYMMDD)
START_DATE="20260211"
END_DATE="20260303"

# Build the project in parallel
echo "Building project..."
make -j
if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

CURRENT_DATE="$START_DATE"

# Create log folder with current timestamp: MMDD_HHMM
LOG_FOLDER=$(date +%m%d_%H%M)
mkdir -p "./exec/log/${LOG_FOLDER}"
echo "Log folder: ./exec/log/${LOG_FOLDER}"

while [ "$CURRENT_DATE" -le "$END_DATE" ]; do
    SYMBOL_FILE="exec/files/Symbols_${CURRENT_DATE}.csv"
    
    if [ -f "$SYMBOL_FILE" ]; then
        echo "========================================"
        echo "Running for date: $CURRENT_DATE"
        echo "========================================"
        
        # Run the signal program with the date argument and log folder
        (cd ./exec/ && ./sv/signal "$CURRENT_DATE" "$LOG_FOLDER")
        
        echo "Finished for date: $CURRENT_DATE"
        echo ""
    else
        echo "Skipping $CURRENT_DATE (File not found: $SYMBOL_FILE)"
    fi

    # Increment date by 1 day
    CURRENT_DATE=$(date -d "$CURRENT_DATE + 1 day" +%Y%m%d)
done
