import pandas as pd
import matplotlib.pyplot as plt


df = pd.read_csv("Implementation.csv")

unique_N = sorted(df["N"].unique())

for N_val in unique_N:
    sub_df = df[df["N"] == N_val].copy()
    sub_df.sort_values("p", inplace=True)

    plt.plot(sub_df["p"], sub_df["fraction_reached_bottom"], marker='o', label=f"N={N_val}")

plt.xlabel("p")
plt.ylabel("Fraction of runs that reached bottom")
plt.title("Fire Spread Probability vs p")
plt.legend()
# plt.show()  # 生成第一张图
plt.savefig("fire_spread_probability.png")


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