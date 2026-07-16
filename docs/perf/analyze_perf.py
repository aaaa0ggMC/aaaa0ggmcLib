#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["pandas", "matplotlib", "numpy"]
# ///
"""
Performance Analysis Script for Sorting Algorithms
Reads all .txt files in perf/ directory and generates comparison plots.

Usage: uv run analyze_perf.py
"""

import os
import glob
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path

plt.style.use('seaborn-v0_8-darkgrid')
colors = plt.cm.tab20(np.linspace(0, 1, 20))

def parse_perf_file(filepath):
    name = Path(filepath).stem
    name = name.replace('<', ' (').replace('>', ')')
    data = []
    with open(filepath, 'r') as f:
        lines = f.readlines()
        for line in lines[1:]:
            line = line.strip()
            if not line:
                continue
            parts = line.split(',')
            if len(parts) == 2:
                try:
                    data.append({'SetSize': int(parts[0].strip()),
                                 'Milliseconds': float(parts[1].strip())})
                except ValueError:
                    continue
    if data:
        df = pd.DataFrame(data)
        df['Algorithm'] = name
        return df
    return None

def categorize_algorithm(name):
    nl = name.lower()
    if any(x in nl for x in ['quick', 'std::sort', 'shell']):
        return 'O(n log n)'
    elif any(x in nl for x in ['bubble', 'insertion', 'selection', 'cocktail', 'comb', 'gnome', 'odd_even']):
        return 'O(n²)'
    elif any(x in nl for x in ['bozo', 'sleep']):
        return 'Pathological'
    return 'Unknown'

def load_all_perf_data(perf_dir='.'):
    dfs = [parse_perf_file(f) for f in glob.glob(os.path.join(perf_dir, '*.txt'))]
    dfs = [d for d in dfs if d is not None and len(d) > 0]
    return pd.concat(dfs, ignore_index=True) if dfs else pd.DataFrame()

def plot_all_algorithms(df, out='perf_analysis_all.png'):
    fig, ax = plt.subplots(figsize=(14, 10))
    for i, algo in enumerate(sorted(df['Algorithm'].unique())):
        d = df[df['Algorithm'] == algo].sort_values('SetSize')
        ax.plot(d['SetSize'], d['Milliseconds'], marker='o', lw=2, ms=4,
                label=algo, color=colors[i % len(colors)])
    ax.set_xlabel('Set Size (n)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
    ax.set_title('Sorting Algorithm Performance Comparison', fontsize=14, fontweight='bold')
    ax.set_xscale('log', base=2)
    ax.set_yscale('log')
    ax.legend(bbox_to_anchor=(1.05, 1), loc='upper left', fontsize=9)
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches='tight')
    print(f"Saved: {out}")
    plt.close()

def plot_by_complexity(df, out='perf_analysis_by_complexity.png'):
    df = df.copy()
    df['Complexity'] = df['Algorithm'].apply(categorize_algorithm)
    fig, axes = plt.subplots(2, 2, figsize=(16, 12))
    groups = sorted(df.groupby('Complexity'))
    for idx, (comp, group) in enumerate(groups):
        if idx >= 4:
            break
        ax = axes[idx // 2, idx % 2]
        for i, algo in enumerate(sorted(group['Algorithm'].unique())):
            d = group[group['Algorithm'] == algo].sort_values('SetSize')
            ax.plot(d['SetSize'], d['Milliseconds'], marker='o', lw=2, ms=4,
                    label=algo, color=colors[i % len(colors)])
        ax.set_xlabel('Set Size (n)', fontsize=11, fontweight='bold')
        ax.set_ylabel('Time (ms)', fontsize=11, fontweight='bold')
        ax.set_title(f'{comp} Algorithms', fontsize=12, fontweight='bold')
        ax.set_xscale('log', base=2)
        ax.set_yscale('log')
        ax.legend(fontsize=8, loc='best')
        ax.grid(True, alpha=0.3)
    for idx in range(len(groups), 4):
        axes[idx // 2, idx % 2].set_visible(False)
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches='tight')
    print(f"Saved: {out}")
    plt.close()

def plot_efficient_comparison(df, out='perf_analysis_efficient.png'):
    df = df.copy()
    df['Complexity'] = df['Algorithm'].apply(categorize_algorithm)
    eff = df[df['Complexity'] == 'O(n log n)']
    if len(eff) == 0:
        return
    fig, ax = plt.subplots(figsize=(12, 8))
    for i, algo in enumerate(sorted(eff['Algorithm'].unique())):
        d = eff[eff['Algorithm'] == algo].sort_values('SetSize')
        ax.plot(d['SetSize'], d['Milliseconds'], marker='o', lw=2.5, ms=5,
                label=algo, color=colors[i % len(colors)])
    ax.set_xlabel('Set Size (n)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time (ms)', fontsize=12, fontweight='bold')
    ax.set_title('Efficient Sorting Algorithms (O(n log n))', fontsize=14, fontweight='bold')
    ax.set_xscale('log', base=2)
    ax.set_yscale('log')
    ax.legend(fontsize=10, loc='best')
    ax.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(out, dpi=300, bbox_inches='tight')
    print(f"Saved: {out}")
    plt.close()

def generate_summary(df, out='perf_summary.txt'):
    rows = []
    for algo in sorted(df['Algorithm'].unique()):
        d = df[df['Algorithm'] == algo].sort_values('SetSize')
        max_sz = d['SetSize'].max()
        max_t = d[d['SetSize'] == max_sz]['Milliseconds'].iloc[0]
        min_sz = d['SetSize'].min()
        min_t = d[d['SetSize'] == min_sz]['Milliseconds'].iloc[0]
        growth = max_t / min_t if min_t > 0 else float('inf')
        rows.append({'Algorithm': algo, 'Complexity': categorize_algorithm(algo),
                     'Min Size': min_sz, 'Max Size': max_sz,
                     'Min Time (ms)': round(min_t, 3), 'Max Time (ms)': round(max_t, 3),
                     'Growth Factor': round(growth, 2), 'Points': len(d)})
    summary_df = pd.DataFrame(rows)
    with open(out, 'w') as f:
        f.write("Sorting Algorithm Performance Summary\n")
        f.write("=" * 120 + "\n\n")
        f.write(summary_df.to_string(index=False))
    print(f"Saved: {out}")
    return summary_df

def main():
    print("Loading performance data...")
    df = load_all_perf_data()
    if len(df) == 0:
        print("No data found!")
        return
    print(f"Loaded {df['Algorithm'].nunique()} algorithms, {len(df)} data points\n")

    print("Generating visualizations...")
    plot_all_algorithms(df)
    plot_by_complexity(df)
    plot_efficient_comparison(df)

    print("\nGenerating summary...")
    summary = generate_summary(df)
    print("\n" + summary.to_string(index=False))
    print("\nDone!")

if __name__ == '__main__':
    main()
