"""Generates plots from results.csv (TestApp output).

Usage:  python plot_results.py            (reads results.csv from the current directory)
        python plot_results.py path.csv
Requirements:    python -m pip install matplotlib
"""
import csv
import sys
from collections import defaultdict

import matplotlib.pyplot as plt

path = sys.argv[1] if len(sys.argv) > 1 else "results.csv"

# data[strategy][thread_count] = (median, min, max)
data = defaultdict(dict)
with open(path, newline="") as f:
    for row in csv.DictReader(f):
        data[row["strategy"]][int(row["threads"])] = (
            float(row["median_s"]),
            float(row["min_s"]),
            float(row["max_s"]),
        )

threads = sorted(data["malloc"])
x = list(range(len(threads)))

order = ["malloc", "ahm_map", "ahm_header"]
labels = {
    "malloc": "malloc/free",
    "ahm_map": "AHM (AllocationMap)",
    "ahm_header": "AHM (Header)",
}
# Use the same colors for both plots
colors = {
    "malloc": "tab:blue",
    "ahm_map": "tab:orange",
    "ahm_header": "tab:green",
}

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5))

# ---- 1) Execution time by thread count (lower is better) ----
width = 0.27
for k, name in enumerate(order):
    med = [data[name][t][0] for t in threads]
    below = [data[name][t][0] - data[name][t][1] for t in threads]
    above = [data[name][t][2] - data[name][t][0] for t in threads]
    pos = [i + (k - 1) * width for i in x]
    ax1.bar(pos, med, width, yerr=[below, above], capsize=3,
            color=colors[name], label=labels[name])

ax1.set_xticks(x)
ax1.set_xticklabels(threads)
ax1.set_xlabel("Number of threads")
ax1.set_ylabel("Execution time (s) - lower is better")
ax1.set_title("Allocation and deallocation of 4 GB (median, min-max)")
ax1.legend()
ax1.grid(axis="y", alpha=0.3)

# ---- 2) Relative performance compared to malloc (malloc = 1.0) ----
# Values > 1.0 indicate FASTER performance than malloc, while values < 1.0 indicate slower performance
highest = 1.0
for name in order:
    if name == "malloc":
        rel = [1.0 for _ in threads]
    else:
        rel = [data["malloc"][t][0] / data[name][t][0] for t in threads]
    highest = max(highest, max(rel))
    ax2.plot(x, rel, marker="o", color=colors[name], label=labels[name],
             linestyle="--" if name == "malloc" else "-")

ax2.set_ylim(0, max(1.5, highest * 1.1))   # Start the axis at zero so small differences are not exaggerated
ax2.set_xticks(x)
ax2.set_xticklabels(threads)
ax2.set_xlabel("Number of threads")
ax2.set_ylabel("Performance relative to malloc (malloc = 1.0), higher is better")
ax2.set_title("Relative performance (baseline: malloc/free)")
ax2.legend()
ax2.grid(alpha=0.3)

plt.tight_layout()
plt.savefig("results.png", dpi=150)
print("Saved: results.png")