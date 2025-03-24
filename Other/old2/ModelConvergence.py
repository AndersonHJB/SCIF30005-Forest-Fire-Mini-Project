import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("model_convergence.csv")

df_p06 = df[df["p"] == 0.6].copy()
df_p06.sort_values(by="M", inplace=True)

fig1 = plt.figure()
ax1 = fig1.add_subplot(111)

ax1.plot(df_p06["M"], df_p06["avg_steps"], marker='o', label="Average Steps")
ax1.set_xlabel("M (number of repeats)")
ax1.set_ylabel("Average Steps before fire stops")
ax1.set_xticks(df_p06["M"].tolist())

# 右轴：Fraction Reached Bottom
ax2 = ax1.twinx()
ax2.plot(df_p06["M"], df_p06["fraction_reached_bottom"], marker='s', label="Fraction Reached Bottom", linestyle='--')
ax2.set_ylabel("Fraction Reached Bottom")

fig1.legend(loc="upper left", bbox_to_anchor=(0.12, 0.88))
plt.title("Convergence Analysis at p = 0.6")

fig1.savefig("figure1_convergence_p06.png", dpi=300)
print("✅ 图1 已保存为 figure1_convergence_p06.png")

df_M50 = df[df["M"] == 50].copy()
df_M50.sort_values(by="p", inplace=True)

fig2 = plt.figure()
ax3 = fig2.add_subplot(111)

ax3.plot(df_M50["p"], df_M50["avg_steps"], marker='o')
ax3.set_xlabel("p (tree probability)")
ax3.set_ylabel("Average Steps before fire stops")
plt.title("Average Steps vs p (M = 50)")

fig2.savefig("figure2_avg_steps_vs_p.png", dpi=300)
print("✅ 图2 已保存为 figure2_avg_steps_vs_p.png")

# 可选：也可以显示图（交互查看）
# plt.show()
