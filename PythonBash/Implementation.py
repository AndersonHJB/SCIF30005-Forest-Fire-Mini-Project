import pandas as pd
import matplotlib.pyplot as plt

# 读取CSV文件
df = pd.read_csv("../output/Implementation.csv")
# -------------------------------
# 第一张图: 火到达底部的概率 vs p
# -------------------------------
# 按 N 分组, 分别绘制 p ~ fraction_reached_bottom
unique_N = sorted(df["N"].unique())

for N_val in unique_N:
    sub_df = df[df["N"] == N_val].copy()
    # 排序以便在 p 递增情况下绘图
    sub_df.sort_values("p", inplace=True)

    plt.plot(sub_df["p"], sub_df["fraction_reached_bottom"], marker='o', label=f"N={N_val}")

plt.xlabel("p")
plt.ylabel("Fraction of runs that reached bottom")
plt.title("Fire Spread Probability vs p")
plt.legend()
# plt.show()  # 生成第一张图
plt.savefig("fire_spread_probability.png")

# -------------------------------
# 第二张图: 平均步数 vs p
# -------------------------------
for N_val in unique_N:
    sub_df = df[df["N"] == N_val].copy()
    sub_df.sort_values("p", inplace=True)

    plt.plot(sub_df["p"], sub_df["avg_steps"], marker='o', label=f"N={N_val}")

plt.xlabel("p")
plt.ylabel("Average Steps Before Fire Stops")
plt.title("Average Steps vs p")
plt.legend()
# plt.show()  # 生成第二张图
plt.savefig("average_steps.png")