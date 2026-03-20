#!/usr/bin/env python3
"""
Analyze overnight sweep results for strategy-vwap-0930.
SignalA param sweep + PV ratio (entry distance from VWAP) analysis.
Usage: python3 scripts/analyze_sweep_0930.py sweep_signala/
"""
import csv, glob, os, sys
from collections import defaultdict

def adj_pnl(r):
    pnl = float(r['PnL'])
    is_lu = 'lockedLimitUp' in r.get('LeaveCause', '')
    return pnl - 20000 + (120000 if is_lu else 0)

def pv_ratio(r):
    """Entry price / VWAP - 1 (how far above VWAP at entry)."""
    ep = float(r['EntryPrice'])
    ev = float(r['EntryVWAP'])
    return (ep / ev - 1) if ev > 0 else 0

def make_filter(max_gr, require_m1=True, no_prev_lu=True, min_dip=0.005, max_drop=0.05, max_pv=None):
    def f(r):
        try:
            gr = int(r['GroupRank']); mr = int(r['MemberRank']); prev_lu = int(r['IsPrevDayLU'])
            ep = float(r['EntryPrice']); dh = float(r['DayHigh'])
            e0050 = float(r['0050EntryChg%']); h0050 = float(r['0050AtDayHighChg%'])
        except (ValueError, KeyError):
            return False
        if gr > max_gr: return False
        if require_m1 and mr != 1: return False
        if no_prev_lu and prev_lu != 0: return False
        dip = (dh - ep) / dh if dh > 0 else 0
        drop = e0050 - h0050
        if dip < min_dip: return False
        if drop > max_drop: return False
        if max_pv is not None and pv_ratio(r) >= max_pv: return False
        return True
    return f

def calc_metrics(rows):
    if not rows:
        return {'n': 0, 'wr': 0, 'pf': 0, 'adj': 0, 'maxdd': 0, 'rf': 0}
    n = len(rows)
    w = sum(1 for r in rows if adj_pnl(r) > 0)
    t = sum(adj_pnl(r) for r in rows)
    gp = sum(adj_pnl(r) for r in rows if adj_pnl(r) > 0)
    gl = sum(-adj_pnl(r) for r in rows if adj_pnl(r) < 0)
    pf = gp / gl if gl > 0 else float('inf')
    daily = defaultdict(float)
    for r in rows: daily[r['_date']] += adj_pnl(r)
    equity = 0; peak = 0; max_dd = 0
    for d in sorted(daily):
        equity += daily[d]
        if equity > peak: peak = equity
        dd = peak - equity
        if dd > max_dd: max_dd = dd
    rf = t / max_dd if max_dd > 0 else float('inf')
    return {'n': n, 'wr': w/n*100, 'pf': pf, 'adj': t, 'maxdd': max_dd, 'rf': rf}

def load_batch(batch_dir):
    rows = []
    for f in sorted(glob.glob(f'{batch_dir}/report_trades_*.csv')):
        date = os.path.basename(f).split('_')[-1].replace('.csv', '')
        with open(f) as fh:
            for r in csv.DictReader(fh):
                r['_date'] = date
                rows.append(r)
    return rows

def fmt(m):
    if m['n'] == 0:
        return '0T'
    rf_str = f" RF={m['rf']:.1f}x" if m['rf'] < 999 else ""
    return f"{m['n']}T WR={m['wr']:.0f}% PF={m['pf']:.2f} Adj={m['adj']/1000:+.0f}K{rf_str}"

def classify_batch(name):
    if name.startswith('br_'):
        # Phase 1: br_6=0.006, br_7=0.007, br_8=0.008 (1 digit)
        # Phase 3: br_0065=0.0065, br_0075=0.0075 (4 digits starting with 00)
        digits = name[3:]
        if len(digits) <= 2 and not digits.startswith('00'):
            val = float(digits) / 1000.0  # br_6 -> 0.006
        else:
            val = float('0.' + digits)  # br_0065 -> 0.0065
        return 'bounce_ratio', val, f'br={val:.4f}'
    elif name.startswith('vn_'):
        # vn_1001=1.001, vn_1003=1.003 (4 digits)
        # vn_10015=1.0015, vn_10025=1.0025 (5 digits)
        digits = name[3:]
        if len(digits) == 4:
            val = float(digits) / 1000.0
        else:
            val = float(digits) / 10000.0
        return 'vwap_near_ratio', val, f'vn={val:.4f}'
    elif name.startswith('mn_'):
        val = int(name[3:])
        return 'max_near_to_entry_sec', val, f'mn={val}'
    elif name.startswith('sl_'):
        val = float(name[3:]) / 1000.0
        return 'stop_loss', val, f'SL={val:.3f}'
    return 'other', name, name

def pv_analysis(rows, label):
    """Analyze PV ratio distribution for a set of rows."""
    print(f'\n  --- PV Analysis ({label}) ---')
    buckets = [
        ('PV < 0.3%',   0, 0.003),
        ('PV 0.3-0.5%', 0.003, 0.005),
        ('PV 0.5-0.8%', 0.005, 0.008),
        ('PV 0.8-1.0%', 0.008, 0.010),
        ('PV >= 1.0%',  0.010, 999),
    ]
    for blabel, lo, hi in buckets:
        subset = [r for r in rows if lo <= pv_ratio(r) < hi]
        m = calc_metrics(subset)
        if m['n'] > 0:
            print(f'    {blabel:16s}: {fmt(m)}')

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 scripts/analyze_sweep_0930.py <sweep_dir>")
        sys.exit(1)

    sweep_dir = sys.argv[1]

    # Collect all batches
    categories = defaultdict(list)
    for d in sorted(os.listdir(sweep_dir)):
        batch_path = os.path.join(sweep_dir, d)
        if not os.path.isdir(batch_path) and not os.path.islink(batch_path):
            continue
        rows = load_batch(batch_path)
        if not rows:
            continue
        cat, sort_val, display = classify_batch(d)
        if cat == 'other':
            continue
        categories[cat].append((sort_val, display, rows))

    if not categories:
        print(f"No results found in {sweep_dir}")
        sys.exit(1)

    for cat in categories:
        categories[cat].sort(key=lambda x: x[0] if isinstance(x[0], (int, float)) else 0)

    # Define filter sets
    filters = [
        ('Raw (all)',          lambda r: True),
        ('GR<=4 M1 NoPLU D/D', make_filter(4)),
        ('GR<=10 M1 NoPLU D/D', make_filter(10)),
        ('GR<=10 M1 NoPLU D/D PV<0.8%', make_filter(10, max_pv=0.008)),
        ('GR<=10 M1 NoPLU D/D PV<0.5%', make_filter(10, max_pv=0.005)),
    ]

    baselines = {'bounce_ratio': 0.008, 'vwap_near_ratio': 1.003, 'max_near_to_entry_sec': 1500}

    print("=" * 120)
    print("  SignalA Parameter Sweep Report — strategy-vwap-0930")
    print("=" * 120)
    print("  Filters: M1=MemberRank==1, NoPLU=NoPrevDayLimitUp, D/D=dip>=0.5%+drop<=0.05pp")
    print("  PV = EntryPrice/EntryVWAP - 1 (how far above VWAP at entry)")
    print()

    # --- Per-parameter sweep ---
    for cat_name, cat_label in [
        ('bounce_ratio', 'bounce_ratio (fix vn=1.003, mn=1500)'),
        ('vwap_near_ratio', 'vwap_near_ratio (fix br=0.008, mn=1500)'),
        ('max_near_to_entry_sec', 'max_near_to_entry_sec (fix br=0.008, vn=1.003)')
    ]:
        if cat_name not in categories:
            continue
        results = categories[cat_name]
        bl = baselines.get(cat_name)

        print(f"\n{'=' * 120}")
        print(f"  {cat_label}")
        print(f"{'=' * 120}")

        for fname, ffunc in filters:
            print(f"\n  [{fname}]")
            best_pf = 0; best_label = ''
            for sort_val, display, rows in results:
                subset = [r for r in rows if ffunc(r)]
                m = calc_metrics(subset)
                marker = ' <--' if bl is not None and abs(sort_val - bl) < 0.0001 else ''
                print(f"    {display:<12} {fmt(m)}{marker}")
                if m['n'] >= 5 and m['pf'] > best_pf:
                    best_pf = m['pf']; best_label = display
            if best_label:
                print(f"    -> Best: {best_label} (PF={best_pf:.2f})")

    # --- PV distribution for baseline batch ---
    print(f"\n{'=' * 120}")
    print(f"  PV Ratio Analysis (baseline: br=0.008, vn=1.003, mn=1500)")
    print(f"{'=' * 120}")

    # Find baseline batch
    baseline_rows = None
    if 'bounce_ratio' in categories:
        for sv, disp, rows in categories['bounce_ratio']:
            if abs(sv - 0.008) < 0.0001:
                baseline_rows = rows
                break

    if baseline_rows:
        for fname, ffunc in filters[1:]:  # skip Raw
            filtered = [r for r in baseline_rows if ffunc(r)]
            pv_analysis(filtered, fname)

        # GR range analysis with PV
        print(f'\n  --- GR range × PV (M1 NoPLU dip/drop) ---')
        print(f'    {"GR range":<12} {"All":^30} {"PV<0.5%":^30} {"PV 0.5-0.8%":^30} {"PV>=0.8%":^30}')
        for gr_lo, gr_hi, gr_label in [(1,4,'GR 1-4'), (5,10,'GR 5-10'), (1,10,'GR 1-10')]:
            def grf(r, lo=gr_lo, hi=gr_hi):
                try:
                    gr = int(r['GroupRank']); mr = int(r['MemberRank']); prev_lu = int(r['IsPrevDayLU'])
                    ep = float(r['EntryPrice']); dh = float(r['DayHigh'])
                    e0050 = float(r['0050EntryChg%']); h0050 = float(r['0050AtDayHighChg%'])
                except: return False
                dip = (dh - ep) / dh if dh > 0 else 0
                drop = e0050 - h0050
                return lo <= gr <= hi and mr == 1 and prev_lu == 0 and dip >= 0.005 and drop <= 0.05
            base = [r for r in baseline_rows if grf(r)]
            pv_lo = [r for r in base if pv_ratio(r) < 0.005]
            pv_mid = [r for r in base if 0.005 <= pv_ratio(r) < 0.008]
            pv_hi = [r for r in base if pv_ratio(r) >= 0.008]
            m_all = calc_metrics(base)
            m_lo = calc_metrics(pv_lo)
            m_mid = calc_metrics(pv_mid)
            m_hi = calc_metrics(pv_hi)
            print(f'    {gr_label:<12} {fmt(m_all):^30} {fmt(m_lo):^30} {fmt(m_mid):^30} {fmt(m_hi):^30}')

    # --- Post-filter dimension sweep (baseline batch only, all FREE) ---
    if baseline_rows:
        print(f"\n{'=' * 120}")
        print(f"  Post-Filter Dimension Sweep (baseline batch, no extra compute)")
        print(f"{'=' * 120}")

        # GR threshold sweep
        print(f'\n  --- GR threshold (M1 NoPLU dip>=0.5% drop<=0.05) ---')
        for max_gr in [4, 6, 8, 10, 15, 20]:
            f = make_filter(max_gr)
            m = calc_metrics([r for r in baseline_rows if f(r)])
            print(f'    GR<={max_gr:<3d}: {fmt(m)}')

        # dip threshold sweep
        print(f'\n  --- dip threshold (GR<=4 M1 NoPLU drop<=0.05) ---')
        for min_dip in [0.0, 0.003, 0.005, 0.007, 0.010]:
            f = make_filter(4, min_dip=min_dip)
            m = calc_metrics([r for r in baseline_rows if f(r)])
            print(f'    dip>={min_dip:.1%}:  {fmt(m)}')

        # dip threshold sweep GR<=10
        print(f'\n  --- dip threshold (GR<=10 M1 NoPLU drop<=0.05) ---')
        for min_dip in [0.0, 0.003, 0.005, 0.007, 0.010]:
            f = make_filter(10, min_dip=min_dip)
            m = calc_metrics([r for r in baseline_rows if f(r)])
            print(f'    dip>={min_dip:.1%}:  {fmt(m)}')

        # drop threshold sweep
        print(f'\n  --- drop threshold (GR<=4 M1 NoPLU dip>=0.5%) ---')
        for max_drop in [0.02, 0.03, 0.05, 0.10, 999]:
            label = f'drop<={max_drop:.2f}' if max_drop < 100 else 'no limit'
            f = make_filter(4, max_drop=max_drop)
            m = calc_metrics([r for r in baseline_rows if f(r)])
            print(f'    {label:>14s}: {fmt(m)}')

        # entry time analysis
        print(f'\n  --- Entry time (GR<=4 M1 NoPLU dip/drop) ---')
        strict_f = make_filter(4)
        strict_rows = [r for r in baseline_rows if strict_f(r)]
        for max_time, label in [('10:00', '<10:00'), ('10:30', '<10:30'), ('11:00', '<11:00'), ('12:00', '<12:00'), ('13:00', 'all')]:
            subset = [r for r in strict_rows if r.get('EntryTime','') < max_time or label == 'all']
            m = calc_metrics(subset)
            print(f'    Entry {label:>7s}: {fmt(m)}')

        # entry time × GR<=10
        print(f'\n  --- Entry time (GR<=10 M1 NoPLU dip/drop) ---')
        gr10_f = make_filter(10)
        gr10_rows = [r for r in baseline_rows if gr10_f(r)]
        for max_time, label in [('10:00', '<10:00'), ('10:30', '<10:30'), ('11:00', '<11:00'), ('12:00', '<12:00'), ('13:00', 'all')]:
            subset = [r for r in gr10_rows if r.get('EntryTime','') < max_time or label == 'all']
            m = calc_metrics(subset)
            print(f'    Entry {label:>7s}: {fmt(m)}')

    # --- SL Sweep ---
    if 'stop_loss' in categories:
        print(f"\n{'=' * 120}")
        print(f"  Stop Loss Sweep (VWAP-based, no TP, baseline SignalA)")
        print(f"{'=' * 120}")
        print(f"  SL = price <= entryVWAP * ratio → stopLoss")

        # Include baseline (no SL) for reference
        sl_results = categories['stop_loss']

        for fname, ffunc in filters:
            print(f"\n  [{fname}]")
            # Show NoSL baseline first
            if baseline_rows:
                subset = [r for r in baseline_rows if ffunc(r)]
                m = calc_metrics(subset)
                sl_cnt = sum(1 for r in subset if 'stopLoss' in r.get('LeaveCause',''))
                lu_cnt = sum(1 for r in subset if 'lockedLimitUp' in r.get('LeaveCause',''))
                print(f"    {'NoSL':<12} {fmt(m)}  [SL=0 LU={lu_cnt}]")
            for sort_val, display, rows in sl_results:
                subset = [r for r in rows if ffunc(r)]
                m = calc_metrics(subset)
                sl_cnt = sum(1 for r in subset if 'stopLoss' in r.get('LeaveCause',''))
                lu_cnt = sum(1 for r in subset if 'lockedLimitUp' in r.get('LeaveCause',''))
                print(f"    {display:<12} {fmt(m)}  [SL={sl_cnt} LU={lu_cnt}]")

    # --- Summary ---
    print(f"\n{'=' * 120}")
    print("  Summary")
    print(f"{'=' * 120}")
    print("  Current baseline: br=0.008, vn=1.003, mn=1500, NoSL, NoTP")

    # Best per param for strict filter (GR<=4)
    strict_f = make_filter(4)
    for cat_name in ['bounce_ratio', 'vwap_near_ratio', 'max_near_to_entry_sec']:
        if cat_name not in categories:
            continue
        best_pf = 0; best_label = ''; best_n = 0
        for sv, disp, rows in categories[cat_name]:
            subset = [r for r in rows if strict_f(r)]
            m = calc_metrics(subset)
            if m['n'] >= 5 and m['pf'] > best_pf:
                best_pf = m['pf']; best_label = disp; best_n = m['n']
        if best_label:
            print(f"  Best {cat_name} (GR<=4 strict): {best_label} ({best_n}T PF={best_pf:.2f})")

    print()

if __name__ == '__main__':
    main()
