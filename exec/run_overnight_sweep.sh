#!/bin/bash
# Overnight sweep for strategy-vwap-0930
# Phase 1: SignalA 1D sweeps (3 values each, 7 batches, ~5hr)
# Analysis includes PV ratio (entry distance from VWAP) as post-filter dimension
#
# Usage: nohup bash run_overnight_sweep.sh > sweep_signala/sweep.log 2>&1 &

set -e
cd /root/jerry/signal/exec

SWEEP_DIR="sweep_signala"
mkdir -p "$SWEEP_DIR"

# Backup parameter.cfg
cp cfg/parameter.cfg cfg/parameter.cfg.sweep_backup
echo "$(date): Sweep started, backup saved to cfg/parameter.cfg.sweep_backup"

DATES=$(ls data/TSEQuote.* | sed 's/.*\.//' | grep -v volcache | sort)
TOTAL=$(echo $DATES | wc -w)

run_batch() {
    local BATCH_NAME="$1"
    local BATCH_DIR="${SWEEP_DIR}/${BATCH_NAME}"
    mkdir -p "$BATCH_DIR"

    # Check if already complete
    EXISTING=$(ls ${BATCH_DIR}/report_trades_*.csv 2>/dev/null | wc -l)
    if [ "$EXISTING" -ge "$TOTAL" ]; then
        echo "$(date): [$BATCH_NAME] Already complete ($EXISTING files), skipping"
        return
    fi

    echo "$(date): [$BATCH_NAME] Starting ($EXISTING/$TOTAL done)..."
    local COUNT=0
    for d in $DATES; do
        COUNT=$((COUNT+1))
        if [ -f "${BATCH_DIR}/report_trades_${d}.csv" ]; then
            continue
        fi
        timeout 180 ./sv/signal "$d" > /dev/null 2>&1
        LATEST=$(ls -td log/${d}_*/ 2>/dev/null | head -1)
        if [ -n "$LATEST" ] && [ -f "${LATEST}report_trades.csv" ]; then
            cp "${LATEST}report_trades.csv" "${BATCH_DIR}/report_trades_${d}.csv"
        fi
    done
    TRADES_TOTAL=$(cat ${BATCH_DIR}/report_trades_*.csv 2>/dev/null | grep -v "^Symbol" | wc -l)
    echo "$(date): [$BATCH_NAME] Done: ${TRADES_TOTAL} trades"
}

set_param() {
    sed -i "s/^${1}=.*/${1}=${2}/" cfg/parameter.cfg
}

restore_baseline() {
    cp cfg/parameter.cfg.sweep_backup cfg/parameter.cfg
}

########################################
# Phase 1: SignalA 1D Sweeps (7 batches)
########################################
echo ""
echo "========================================"
echo "Phase 1: SignalA 1D Parameter Sweeps"
echo "========================================"

# --- Sweep A: bounce_ratio (3 values) ---
# Fixed: vwap_near_ratio=1.003, max_near_to_entry_sec=1500
echo ""
echo "--- Sweep A: bounce_ratio ---"
for BR in 0.006 0.007 0.008 0.009 0.010; do
    BATCH_NAME="br_$(echo $BR | sed 's/0\.0*//')"
    restore_baseline
    set_param "bounce_ratio" "$BR"
    run_batch "$BATCH_NAME"
done

# --- Sweep B: vwap_near_ratio (3 values) ---
# Fixed: bounce_ratio=0.008, max_near_to_entry_sec=1500
echo ""
echo "--- Sweep B: vwap_near_ratio ---"
for VN in 1.001 1.002; do
    # Skip 1.003 (same as br_8)
    BATCH_NAME="vn_$(echo $VN | tr -d '.')"
    restore_baseline
    set_param "vwap_near_ratio" "$VN"
    run_batch "$BATCH_NAME"
done

# --- Sweep C: max_near_to_entry_sec (3 values) ---
# Fixed: bounce_ratio=0.008, vwap_near_ratio=1.003
echo ""
echo "--- Sweep C: max_near_to_entry_sec ---"
for MN in 750 2250; do
    # Skip 1500 (same as br_8)
    BATCH_NAME="mn_${MN}"
    restore_baseline
    set_param "max_near_to_entry_sec" "$MN"
    run_batch "$BATCH_NAME"
done

########################################
# Symlinks + Analysis
########################################
echo ""
echo "========================================"
echo "Analysis"
echo "========================================"
restore_baseline

# br_8 = vn_1003 = mn_1500 (same params, shared baseline)
if [ -d "${SWEEP_DIR}/br_8" ] && [ ! -e "${SWEEP_DIR}/vn_1003" ]; then
    ln -sf br_8 "${SWEEP_DIR}/vn_1003"
fi
if [ -d "${SWEEP_DIR}/br_8" ] && [ ! -e "${SWEEP_DIR}/mn_1500" ]; then
    ln -sf br_8 "${SWEEP_DIR}/mn_1500"
fi

python3 scripts/analyze_sweep_0930.py "$SWEEP_DIR" | tee "${SWEEP_DIR}/phase1_report.txt"

echo ""
echo "$(date): Phase 1 complete!"

########################################
# Phase 2: SL Sweep (3 batches, ~1.5hr)
########################################
echo ""
echo "========================================"
echo "Phase 2: SL Sweep (VWAP-based, no TP)"
echo "========================================"

for SL in 0.997 0.995 0.993; do
    BATCH_NAME="sl_$(echo $SL | sed 's/0\.//')"
    restore_baseline
    set_param "stop_loss_ratio_a" "$SL"
    run_batch "$BATCH_NAME"
done

########################################
# Phase 3: Finer SignalA grid (5 batches, ~3hr)
# Fill in gaps between Phase 1 values
########################################
echo ""
echo "========================================"
echo "Phase 3: Finer SignalA Grid"
echo "========================================"

# bounce_ratio: fill between 0.006-0.007-0.008
echo "--- Finer bounce_ratio ---"
for BR in 0.0065 0.0075; do
    # Use full digits after decimal: 0.0065 -> br_0065
    BATCH_NAME="br_$(echo $BR | sed 's/0\.//')"
    restore_baseline
    set_param "bounce_ratio" "$BR"
    run_batch "$BATCH_NAME"
done

# vwap_near_ratio: fill between 1.001-1.002-1.003
echo "--- Finer vwap_near_ratio ---"
for VN in 1.0015 1.0025; do
    BATCH_NAME="vn_$(echo $VN | tr -d '.')"
    restore_baseline
    set_param "vwap_near_ratio" "$VN"
    run_batch "$BATCH_NAME"
done

# max_near_to_entry_sec: fill between 750-1500-2250
echo "--- Finer max_near_to_entry_sec ---"
for MN in 1000 1750; do
    BATCH_NAME="mn_${MN}"
    restore_baseline
    set_param "max_near_to_entry_sec" "$MN"
    run_batch "$BATCH_NAME"
done

########################################
# Final Report
########################################
echo ""
echo "========================================"
echo "Final Report (Phase 1 + 2 + 3)"
echo "========================================"
restore_baseline
python3 scripts/analyze_sweep_0930.py "$SWEEP_DIR" | tee "${SWEEP_DIR}/morning_report.txt"

echo ""
echo "$(date): Overnight sweep complete!"
echo "Report saved to: ${SWEEP_DIR}/morning_report.txt"
