#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# TODO: Plan A: bash -> txt -> Python txt to csv -> Python 可视化
# TODO: Plan B: bash -> csv -> Python 可视化

"""
用途：
  1. 打开 all_results.txt
  2. 逐块读取 N, p, M, average_steps, fraction_reached_bottom, average_wall_time 等信息
  3. 写出到 all_results.csv

用法：
  python parse_fire_results.py
"""

import re

def main():
    input_file = "/all_results.txt"
    output_file = "all_results.csv"

    # 正则表达式（或字符串查找）来捕捉所需信息
    # 1) "Running N=50, p=0.1, M=50"
    run_header_pattern = re.compile(r"Running\s+N\s*=\s*([\d\.]+),\s*p\s*=\s*([\d\.]+),\s*M\s*=\s*(\d+)")

    # 2) 输出内容中的几行
    #    "  Average steps before fire stops: 49.32"
    avg_steps_pattern  = re.compile(r"Average steps before fire stops:\s*([\d\.]+)")
    #    "  Fraction of runs that reached bottom: 0.24"
    frac_bottom_pattern = re.compile(r"Fraction of runs that reached bottom:\s*([\d\.]+)")
    #    "  Average wall time \(max among procs\): 0.006403"
    wall_time_pattern   = re.compile(r"Average wall time \(max among procs\):\s*([\d\.]+)")

    results = []

    # 用于缓存当前块的 N, p, M
    current_block = {
        'N': None,
        'p': None,
        'M': None
    }
    # 用于标记是否读到 simulation results
    got_simulation_data = False

    with open(input_file, "r", encoding="utf-8") as fin:
        for line in fin:
            line = line.strip()

            # 检测 "Running N=..., p=..., M=..." 行
            match_run = run_header_pattern.search(line)
            if match_run:
                # 如果已经有缓存的block且还没记录到 results，说明上一个block没有查到有效结果
                # 这里若想保留可以存, 若不要也可忽略
                # 重置 current_block
                current_block = {
                    'N': float(match_run.group(1)),
                    'p': float(match_run.group(2)),
                    'M': int(match_run.group(3))
                }
                got_simulation_data = False
                continue

            # 检测 "Average steps..." 行
            match_steps = avg_steps_pattern.search(line)
            if match_steps:
                current_block['avg_steps'] = float(match_steps.group(1))
                got_simulation_data = True
                continue

            # 检测 "Fraction of runs that reached bottom: ..." 行
            match_frac = frac_bottom_pattern.search(line)
            if match_frac:
                current_block['frac_bottom'] = float(match_frac.group(1))
                got_simulation_data = True
                continue

            # 检测 "Average wall time..." 行
            match_time = wall_time_pattern.search(line)
            if match_time:
                current_block['avg_wall_time'] = float(match_time.group(1))
                got_simulation_data = True
                continue

            # 如果遇到分隔线 "-----------------------------"
            # 说明一个 block 结束，可把当前 block 存下来（如果它包含模拟数据）
            if line.startswith("---"):
                # 如果确实在此block里拿到了有用数据，就存下
                if got_simulation_data and 'avg_steps' in current_block:
                    # 补齐没有抓到的字段
                    # 若没有抓到(可能某行缺失)，也可用 None 或 0 代替
                    if 'frac_bottom' not in current_block:
                        current_block['frac_bottom'] = 0.0
                    if 'avg_wall_time' not in current_block:
                        current_block['avg_wall_time'] = 0.0
                    # 记录本次结果
                    results.append(current_block)
                # 为下一块做准备
                current_block = {
                    'N': None,
                    'p': None,
                    'M': None
                }
                got_simulation_data = False

    # 可能最后一个块没有以"---"结尾，这时检查并补存
    if got_simulation_data and 'avg_steps' in current_block:
        if 'frac_bottom' not in current_block:
            current_block['frac_bottom'] = 0.0
        if 'avg_wall_time' not in current_block:
            current_block['avg_wall_time'] = 0.0
        results.append(current_block)

    # 写 CSV
    with open(output_file, "w", encoding="utf-8") as fout:
        # 写表头
        fout.write("N,p,M,average_steps,fraction_reached_bottom,average_wall_time\n")
        for row in results:
            # 有的字段是 None 就补 0
            N = row.get('N', 0)
            p = row.get('p', 0)
            M = row.get('M', 0)
            avg_steps = row.get('avg_steps', 0)
            frac_bottom = row.get('frac_bottom', 0)
            avg_wall_time = row.get('avg_wall_time', 0)
            fout.write(f"{N},{p},{M},{avg_steps},{frac_bottom},{avg_wall_time}\n")

    print(f"Done. Parsed {len(results)} records from {input_file}.")
    print(f"Output saved in {output_file}.")

if __name__ == "__main__":
    main()

