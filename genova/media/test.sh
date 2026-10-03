#!/bin/bash

# Path to the log file provided as argument
LOG_FILE="$1"

# Run the existing AWK classification and capture output
OUTPUT=$(awk -F= '
/bm_per_kb=/ {
    sum += $2
    count++
    vals[count] = $2
}
END {
    if (count > 0) {
        avg = sum / count

        printf "Average bm_per_kb = %.4f\n", avg

        # --- Largest gap / total range ---
        n = count
        for (i = 1; i <= n; i++) sorted[i] = vals[i]

        # Sort ascending (bubble sort)
        for (i = 1; i <= n; i++) {
            for (j = i + 1; j <= n; j++) {
                if (sorted[i] > sorted[j]) {
                    tmp = sorted[i]
                    sorted[i] = sorted[j]
                    sorted[j] = tmp
                }
            }
        }

        min_val = sorted[1]
        max_val = sorted[n]
        range = max_val - min_val

        max_gap = 0
        for (i = 2; i <= n; i++) {
            gap = sorted[i] - sorted[i-1]
            if (gap > max_gap) max_gap = gap
        }

        if (range > 0)
            gap_ratio = max_gap / range
        else
            gap_ratio = 0

        printf "Gap ratio = %.3f\n", gap_ratio

        det = (1 - gap_ratio) * avg + avg
        printf "Det = %.3f\n", det

        if (det <= 8.080)
            print "Classification: Healthy"
        else if (det > 8.080 && avg <= 12.000)
            print "Classification: Level 1"
        else if (det > 12.000 && avg <= 28.000)
            print "Classification: Level 2"
        else if (det > 28.000 && avg <= 49.000)
            print "Classification: Level 3"
        else if (det > 49.000 && avg <= 215.000)
            print "Classification: Level 4"
        else if (det > 215.000 && avg <= 657.000)
            print "Classification: Level 5"
        else if (det > 657.000 && avg <= 1093.000)
            print "Classification: Level 6"
        else
            print "Classification: Level 7"
    }
    else {
        print "No bm_per_kb values found"
    }
}
' "$LOG_FILE")

# Print the existing output (as required)
echo "$OUTPUT"

# Extract the classification line (the one starting with "Classification:")
CLASSIFICATION_LINE=$(echo "$OUTPUT" | grep "Classification:")
if [ -z "$CLASSIFICATION_LINE" ]; then
    CLASSIFICATION_LINE="Classification: Healthy"
fi

# Extract just the classification value (e.g., "Healthy", "Level 1", etc.)
CLASSIFICATION_VALUE=$(echo "$CLASSIFICATION_LINE" | cut -d' ' -f2-)

# Extract gap ratio from the AWK output ("Gap ratio = 0.778")
GAP_RATIO_VALUE=$(echo "$OUTPUT" | awk -F'= ' '/Gap ratio/ {print $2}')
if [ -z "$GAP_RATIO_VALUE" ]; then
    GAP_RATIO_VALUE=0
fi

# Cluster percentage = (1 - gap_ratio) * 100
CLUSTER_PERCENT_VALUE=$(awk -v g="$GAP_RATIO_VALUE" 'BEGIN { printf "%.1f", (1 - g) * 100 }')

# Export the values as environment variables for the Python server
export GENOVA_CLASSIFICATION="$CLASSIFICATION_VALUE"
export GENOVA_CLUSTER_PERCENT="$CLUSTER_PERCENT_VALUE"

# Change to the web directory to serve files from there
cd "$(dirname "$0")/../web" || exit 1

# Get the IP address for display (try hostname -I, fallback to ip route)
if command -v hostname >/dev/null 2>&1; then
    IP_ADDRESS=$(hostname -I | awk '{print $1}')
fi
if [ -z "$IP_ADDRESS" ]; then
    IP_ADDRESS=$(ip -4 route get 1 2>/dev/null | awk '{print $7;exit}')
fi
if [ -z "$IP_ADDRESS" ]; then
    IP_ADDRESS="<YOUR_IP_ADDRESS>"
fi

# Start the Python HTTP server in the background on all interfaces
python3 server.py &
SERVER_PID=$!

# Function to handle shutdown on Ctrl+C
shutdown() {
    echo
    echo "Shutting down the dashboard..."
    kill "$SERVER_PID" 2>/dev/null
    wait "$SERVER_PID" 2>/dev/null
    echo "Dashboard stopped."
    exit 0
}

# Trap SIGINT (Ctrl+C)
trap shutdown SIGINT

# Print the dashboard information
echo
echo "============================================================"
echo "              GENOVA SCREENING DASHBOARD"
echo "============================================================"
echo
echo "Classification   : $CLASSIFICATION_VALUE"
echo "Cluster Percent  : $CLUSTER_PERCENT_VALUE %"
echo
echo "Open the following URL on the other laptop:"
echo "http://$IP_ADDRESS:8080"
echo
echo "Click the link above to open the visual dashboard."
echo "Press Ctrl+C to close the dashboard and stop the server."
echo
echo "============================================================"
echo

# Wait for the server process to finish (which it won't until we kill it)
wait "$SERVER_PID"
