import pandas as pd
import matplotlib.pyplot as plt

# 1. 读取之前生成的 CSV 文件
df = pd.read_csv("../output/model_convergence.csv")

# 2. 图1：固定 p=0.6，对不同 M 的结果
#    - x轴: M
#    - 左y轴: avg_steps
#    - 右y轴: fraction_reached_bottom
df_p06 = df[df["p"] == 0.6].copy()
df_p06.sort_values(by="M", inplace=True)

fig1 = plt.figure()
ax1 = fig1.add_subplot(111)

# 左轴绘 Average Steps
ax1.plot(df_p06["M"], df_p06["avg_steps"], marker='o', label="Average Steps")
ax1.set_xlabel("M (number of repeats)")
ax1.set_ylabel("Average Steps before fire stops")

# 右轴绘 Fraction Reached Bottom
ax2 = ax1.twinx()
ax2.plot(df_p06["M"], df_p06["fraction_reached_bottom"], marker='s', label="Fraction Reached Bottom")
ax2.set_ylabel("Fraction Reached Bottom")

# 为了同时显示两条曲线的图例，可简单使用 fig1.legend()
fig1.legend(loc="upper left", bbox_to_anchor=(0.12, 0.88))

plt.title("Convergence Analysis at p=0.6")

# 3. 图2：固定 M=50，对不同 p 的结果
#    - x轴: p
#    - y轴: avg_steps
df_M50 = df[df["M"] == 50].copy()
df_M50.sort_values(by="p", inplace=True)

fig2 = plt.figure()
ax3 = fig2.add_subplot(111)

ax3.plot(df_M50["p"], df_M50["avg_steps"], marker='o')
ax3.set_xlabel("p (tree probability)")
ax3.set_ylabel("Average Steps before fire stops")
plt.title("Average Steps vs p (M=50)")

# 4. 显示所有图表
plt.show()
