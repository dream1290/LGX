#!/usr/bin/env python3
"""
LGX Lifetime Probe — Offline Lifetime Analysis

Reads a binary probe log, matches alloc/free pairs, computes per-call-site
lifetime distributions, and answers the gate question:

    "For high-volume call sites, can return_address(0) alone predict
     which lifetime class an allocation belongs to with ≥99.9% accuracy?"

Usage:
    python3 analyze_lifetimes.py /tmp/lgx_probe_<pid>.bin

    # With explicit frame duration (default: auto-detect from markers, or 16.67ms)
    python3 analyze_lifetimes.py --frame-ms 16.67 /tmp/lgx_probe_<pid>.bin

    # Show only sites with >N allocations
    python3 analyze_lifetimes.py --min-count 100 /tmp/lgx_probe_<pid>.bin

    # Export CSV for further analysis
    python3 analyze_lifetimes.py --csv output.csv /tmp/lgx_probe_<pid>.bin

Output:
    Per-call-site breakdown showing:
    - Dominant lifetime class and its purity (% of allocations)
    - Full distribution across lifetime classes
    - Overall "routability" score: what fraction of total volume is
      covered by sites with ≥99.9% purity

Lifetime classes:
    sub-frame:   freed within the same frame as allocated
    1-2 frames:  freed within 2 additional frames (safe with triple-buffer)
    3-10 frames: short-lived but escapes triple-buffer window
    long-lived:  survives >10 frames
    session:     never freed during capture
"""

import struct
import sys
import os
import argparse
from collections import defaultdict

# ── Binary format ────────────────────────────────────────────────

HEADER_FORMAT = '<8sIIQQ'  # magic(8s) version(I) entry_size(I) count(Q) start_time(Q)
HEADER_SIZE = struct.calcsize(HEADER_FORMAT)

ENTRY_FORMAT = '<QQQQII'   # timestamp(Q) ret_addr(Q) ptr(Q) size(Q) type(I) tid(I) = 40 bytes
ENTRY_SIZE = struct.calcsize(ENTRY_FORMAT)  # 40

ENTRY_ALLOC        = 0
ENTRY_FREE         = 1
ENTRY_CALLOC       = 2
ENTRY_REALLOC_OLD  = 3
ENTRY_REALLOC_NEW  = 4
ENTRY_FRAME_MARKER = 5

ALLOC_TYPES = {ENTRY_ALLOC, ENTRY_CALLOC, ENTRY_REALLOC_NEW}
FREE_TYPES  = {ENTRY_FREE, ENTRY_REALLOC_OLD}

# ── Lifetime classes ─────────────────────────────────────────────

CLASS_SUBFRAME   = 'sub-frame'
CLASS_1_2_FRAMES = '1-2 frames'
CLASS_3_10       = '3-10 frames'
CLASS_LONG       = 'long-lived'
CLASS_SESSION    = 'session'

CLASSES_ORDERED = [CLASS_SUBFRAME, CLASS_1_2_FRAMES, CLASS_3_10,
                   CLASS_LONG, CLASS_SESSION]

def classify_lifetime(lifetime_frames):
    """Classify a lifetime (in frames) into a lifetime class.

    Boundaries use +0.5 offset to handle allocations freed at the start
    of frame N+1 (measured lifetime ≈ N.0 frames).  Without this,
    timing jitter splits a uniform site across two adjacent classes.
    """
    if lifetime_frames is None:
        return CLASS_SESSION
    if lifetime_frames < 0.5:
        return CLASS_SUBFRAME
    if lifetime_frames <= 2.5:
        return CLASS_1_2_FRAMES
    if lifetime_frames <= 10.5:
        return CLASS_3_10
    return CLASS_LONG


# ── Parsing ──────────────────────────────────────────────────────

def parse_log(filepath):
    """Parse binary probe log.  Yields (timestamp, ret_addr, ptr, size, type, tid)."""
    with open(filepath, 'rb') as f:
        header_data = f.read(HEADER_SIZE)
        if len(header_data) < HEADER_SIZE:
            print(f"ERROR: File too small for header ({len(header_data)} bytes)",
                  file=sys.stderr)
            sys.exit(1)

        magic, version, entry_size, count, start_time = \
            struct.unpack(HEADER_FORMAT, header_data)

        if magic != b'LGXPROBE':
            print(f"ERROR: Bad magic: {magic!r} (expected b'LGXPROBE')",
                  file=sys.stderr)
            sys.exit(1)

        if version != 1:
            print(f"ERROR: Unsupported version {version}", file=sys.stderr)
            sys.exit(1)

        if entry_size != ENTRY_SIZE:
            print(f"WARNING: Entry size mismatch: file={entry_size}, "
                  f"expected={ENTRY_SIZE}.  Skipping padding if needed.",
                  file=sys.stderr)

        print(f"Log: {count:,} entries, start_time={start_time} ns",
              file=sys.stderr)

        entries = []
        frame_markers = []
        buf = f.read()

        for i in range(count):
            offset = i * entry_size
            if offset + ENTRY_SIZE > len(buf):
                print(f"WARNING: Truncated at entry {i}/{count}",
                      file=sys.stderr)
                break

            ts, ret_addr, ptr, size, etype, tid = \
                struct.unpack_from(ENTRY_FORMAT, buf, offset)

            if etype == ENTRY_FRAME_MARKER:
                frame_markers.append(ts)
            else:
                entries.append((ts, ret_addr, ptr, size, etype, tid))

        return entries, frame_markers, start_time


# ── Analysis ─────────────────────────────────────────────────────

def compute_frame_duration(frame_markers, default_ms=16.667):
    """Compute average frame duration from markers, or use default."""
    if len(frame_markers) < 2:
        duration_ns = default_ms * 1e6
        source = 'default'
    else:
        deltas = [frame_markers[i+1] - frame_markers[i]
                  for i in range(len(frame_markers)-1)]
        # Use median to reject outliers
        deltas.sort()
        duration_ns = deltas[len(deltas) // 2]
        source = f'measured (median of {len(deltas)} intervals)'

    print(f"Frame duration: {duration_ns / 1e6:.2f} ms ({source})",
          file=sys.stderr)
    return duration_ns


def match_alloc_free(entries):
    """Match alloc/free pairs by pointer.  Returns list of completed allocations
       and set of unfreed pointers."""

    # Map ptr → list of (alloc_entry_index) for unfreed allocs at that ptr
    live = defaultdict(list)  # ptr → [index into results]
    results = []  # (ret_addr, size, alloc_ts, free_ts_or_None)

    for ts, ret_addr, ptr, size, etype, tid in entries:
        if etype in ALLOC_TYPES:
            idx = len(results)
            results.append((ret_addr, size, ts, None))
            live[ptr].append(idx)

        elif etype in FREE_TYPES:
            if ptr in live and live[ptr]:
                idx = live[ptr].pop(0)  # FIFO: free oldest alloc at this ptr
                r = results[idx]
                results[idx] = (r[0], r[1], r[2], ts)
                if not live[ptr]:
                    del live[ptr]

    # Count unfreed
    unfreed = sum(len(v) for v in live.values())

    return results, unfreed


def analyze_sites(results, frame_duration_ns, min_count=10):
    """Group allocations by call site (return address) and compute
       per-site lifetime distributions."""

    # Group by return address
    sites = defaultdict(list)  # ret_addr → [(lifetime_frames_or_None, size)]

    for ret_addr, size, alloc_ts, free_ts in results:
        if free_ts is not None:
            lifetime_ns = free_ts - alloc_ts
            lifetime_frames = lifetime_ns / frame_duration_ns
        else:
            lifetime_frames = None  # never freed

        sites[ret_addr].append((lifetime_frames, size))

    # Compute per-site statistics
    site_stats = []

    for ret_addr, allocs in sites.items():
        if len(allocs) < min_count:
            continue

        # Classify each allocation
        class_counts = defaultdict(int)
        class_bytes  = defaultdict(int)
        total_bytes  = 0

        for lifetime_frames, size in allocs:
            cls = classify_lifetime(lifetime_frames)
            class_counts[cls] += 1
            class_bytes[cls]  += size
            total_bytes       += size

        total = len(allocs)

        # Find dominant class
        dominant_class = max(class_counts, key=class_counts.get)
        dominant_count = class_counts[dominant_class]
        purity = dominant_count / total

        # Lifetime stats
        lifetimes = [lf for lf, _ in allocs if lf is not None]
        if lifetimes:
            median_lt = sorted(lifetimes)[len(lifetimes) // 2]
            p99_lt = sorted(lifetimes)[int(len(lifetimes) * 0.99)]
            max_lt = max(lifetimes)
        else:
            median_lt = p99_lt = max_lt = None

        site_stats.append({
            'ret_addr':       ret_addr,
            'count':          total,
            'total_bytes':    total_bytes,
            'dominant_class': dominant_class,
            'purity':         purity,
            'class_counts':   dict(class_counts),
            'class_bytes':    dict(class_bytes),
            'median_frames':  median_lt,
            'p99_frames':     p99_lt,
            'max_frames':     max_lt,
        })

    # Sort by total count (highest-volume sites first)
    site_stats.sort(key=lambda s: s['count'], reverse=True)

    return site_stats


# ── Reporting ────────────────────────────────────────────────────

def print_report(site_stats, total_allocs, unfreed_count):
    """Print human-readable analysis report."""

    print()
    print("=" * 80)
    print("  LGX LIFETIME ANALYSIS — GATE EXPERIMENT RESULTS")
    print("=" * 80)
    print()

    # ── Overall summary ──
    total_volume = sum(s['count'] for s in site_stats)
    pure_volume = sum(s['count'] for s in site_stats if s['purity'] >= 0.999)
    high_pure_volume = sum(s['count'] for s in site_stats if s['purity'] >= 0.99)

    routability_999 = pure_volume / total_volume if total_volume > 0 else 0
    routability_99  = high_pure_volume / total_volume if total_volume > 0 else 0

    print(f"  Total allocations parsed:   {total_allocs:>12,}")
    print(f"  Matched alloc/free pairs:   {sum(1 for s in site_stats for _ in range(s['count'])):>12,}")
    print(f"  Unfreed at capture end:     {unfreed_count:>12,}")
    print(f"  Unique call sites:          {len(site_stats):>12,}")
    print()
    print(f"  ┌─────────────────────────────────────────────────────┐")
    print(f"  │  GATE RESULT                                        │")
    print(f"  │                                                     │")
    print(f"  │  Volume at ≥99.9% purity:  {routability_999:>6.1%}  "
          f"{'✅ PASS' if routability_999 >= 0.90 else '❌ FAIL':>14}  │")
    print(f"  │  Volume at ≥99.0% purity:  {routability_99:>6.1%}  "
          f"{'✅ PASS' if routability_99 >= 0.90 else '❌ FAIL':>14}  │")
    print(f"  │                                                     │")
    print(f"  │  (Target: ≥90% of volume from pure sites)           │")
    print(f"  └─────────────────────────────────────────────────────┘")
    print()

    # ── Triple-buffer safety ──
    # How much volume escapes the 2-frame window?
    escape_volume = 0
    for s in site_stats:
        for cls in [CLASS_3_10, CLASS_LONG]:
            escape_volume += s['class_counts'].get(cls, 0)

    escape_frac = escape_volume / total_volume if total_volume > 0 else 0
    print(f"  Triple-buffer escape rate:  {escape_frac:.4%} of allocations")
    print(f"  (Allocations surviving >2 frames — these CANNOT go in frame arena)")
    print()

    # ── Per-site breakdown ──
    print("─" * 80)
    print("  PER-SITE BREAKDOWN (sorted by volume)")
    print("─" * 80)
    print()

    for i, s in enumerate(site_stats[:30]):  # Top 30 sites
        addr = s['ret_addr']
        print(f"  Site {i+1}: 0x{addr:016x}")
        print(f"    Volume:  {s['count']:>10,} allocs  "
              f"({s['total_bytes'] / 1024:.1f} KB total)")
        print(f"    Dominant: {s['dominant_class']:<14}  "
              f"purity: {s['purity']:>7.3%}  "
              f"{'✅' if s['purity'] >= 0.999 else '⚠️' if s['purity'] >= 0.99 else '❌'}")

        if s['median_frames'] is not None:
            print(f"    Lifetime: median={s['median_frames']:.2f}f  "
                  f"p99={s['p99_frames']:.2f}f  "
                  f"max={s['max_frames']:.2f}f")

        # Show distribution
        dist_parts = []
        for cls in CLASSES_ORDERED:
            cnt = s['class_counts'].get(cls, 0)
            if cnt > 0:
                pct = cnt / s['count'] * 100
                dist_parts.append(f"{cls}:{pct:.1f}%")

        print(f"    Distribution: {', '.join(dist_parts)}")
        print()

    if len(site_stats) > 30:
        print(f"  ... and {len(site_stats) - 30} more sites (use --min-count "
              f"to filter)\n")

    # ── Class summary ──
    print("─" * 80)
    print("  LIFETIME CLASS SUMMARY")
    print("─" * 80)
    print()

    class_totals = defaultdict(int)
    for s in site_stats:
        for cls, cnt in s['class_counts'].items():
            class_totals[cls] += cnt

    for cls in CLASSES_ORDERED:
        cnt = class_totals.get(cls, 0)
        pct = cnt / total_volume * 100 if total_volume > 0 else 0
        bar = '█' * int(pct / 2)
        print(f"    {cls:<14}: {cnt:>10,} ({pct:>5.1f}%) {bar}")

    print()

    # ── Recommendation ──
    print("─" * 80)
    print("  RECOMMENDATION")
    print("─" * 80)
    print()

    if routability_999 >= 0.90:
        print("  ✅ Per-call-site routing is VIABLE.")
        print(f"     {routability_999:.1%} of allocation volume comes from sites")
        print(f"     where a single lifetime class covers ≥99.9% of allocations.")
        print(f"     A static call-site → lifetime-class lookup table would")
        print(f"     correctly route the vast majority of allocations.")
    elif routability_99 >= 0.90:
        print("  ⚠️  Per-call-site routing is MARGINAL.")
        print(f"     {routability_99:.1%} of volume is ≥99% pure, but only")
        print(f"     {routability_999:.1%} reaches the ≥99.9% bar.")
        print(f"     A misprediction rate of {1 - routability_99:.2%} may be")
        print(f"     acceptable for heap-tier routing (cost = fragmentation)")
        print(f"     but NOT for frame-arena routing (cost = use-after-free).")
    else:
        print("  ❌ Per-call-site routing is NOT VIABLE with return-address alone.")
        print(f"     Only {routability_999:.1%} of volume meets ≥99.9% purity.")
        print(f"     Call sites have mixed lifetime patterns that a static")
        print(f"     lookup table cannot resolve.  Consider:")
        print(f"       - Additional features (allocation size, thread ID)")
        print(f"       - Finer-grained call-site identification")
        print(f"       - Abandoning automatic routing in favor of intent-based API")

    print()


def write_csv(site_stats, filepath):
    """Export site statistics to CSV."""
    import csv
    with open(filepath, 'w', newline='') as f:
        writer = csv.writer(f)
        header = ['ret_addr_hex', 'count', 'total_bytes',
                  'dominant_class', 'purity',
                  'median_frames', 'p99_frames', 'max_frames']
        header += [f'count_{cls}' for cls in CLASSES_ORDERED]
        writer.writerow(header)

        for s in site_stats:
            row = [
                f'0x{s["ret_addr"]:016x}',
                s['count'],
                s['total_bytes'],
                s['dominant_class'],
                f'{s["purity"]:.6f}',
                f'{s["median_frames"]:.4f}' if s['median_frames'] is not None else '',
                f'{s["p99_frames"]:.4f}' if s['p99_frames'] is not None else '',
                f'{s["max_frames"]:.4f}' if s['max_frames'] is not None else '',
            ]
            for cls in CLASSES_ORDERED:
                row.append(s['class_counts'].get(cls, 0))
            writer.writerow(row)

    print(f"CSV exported to {filepath}", file=sys.stderr)


# ── Main ─────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='LGX Lifetime Probe — Offline Analysis',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('logfile', help='Path to probe binary log (.bin)')
    parser.add_argument('--frame-ms', type=float, default=None,
                        help='Override frame duration in ms (default: auto-detect)')
    parser.add_argument('--min-count', type=int, default=10,
                        help='Minimum allocations per site to include (default: 10)')
    parser.add_argument('--csv', type=str, default=None,
                        help='Export results to CSV file')

    args = parser.parse_args()

    if not os.path.exists(args.logfile):
        print(f"ERROR: File not found: {args.logfile}", file=sys.stderr)
        sys.exit(1)

    # Parse
    print(f"Parsing {args.logfile}...", file=sys.stderr)
    entries, frame_markers, start_time = parse_log(args.logfile)

    # Frame duration
    if args.frame_ms is not None:
        frame_duration_ns = args.frame_ms * 1e6
        print(f"Frame duration: {args.frame_ms:.2f} ms (user-specified)",
              file=sys.stderr)
    else:
        frame_duration_ns = compute_frame_duration(frame_markers)

    # Match pairs
    print("Matching alloc/free pairs...", file=sys.stderr)
    results, unfreed = match_alloc_free(entries)
    total_allocs = len(results)
    print(f"  {total_allocs:,} allocations, {unfreed:,} unfreed",
          file=sys.stderr)

    # Analyze
    print("Analyzing per-site lifetimes...", file=sys.stderr)
    site_stats = analyze_sites(results, frame_duration_ns,
                               min_count=args.min_count)

    # Report
    print_report(site_stats, total_allocs, unfreed)

    # CSV export
    if args.csv:
        write_csv(site_stats, args.csv)


if __name__ == '__main__':
    main()
