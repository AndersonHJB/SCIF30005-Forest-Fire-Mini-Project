import pandas as pd
import matplotlib.pyplot as plt

def plot_speedup_vs_procs(csv_file):
    # 读取CSV
    df = pd.read_csv(csv_file)

    # 创建画布
    plt.figure()

    # 按不同的 N 进行分组，在同一张图上绘制 speedup vs nprocs
    unique_N_values = df['N'].unique()
    for n in unique_N_values:
        # 过滤出 N == n 的数据，并按 nprocs 升序排序（以便连线有序）
        data_n = df[df['N'] == n].sort_values(by='nprocs')

        # 绘制曲线: 横轴 nprocs，纵轴 speedup
        plt.plot(data_n['nprocs'], data_n['speedup'], marker='o', label=f"N={n}")

    # 设置坐标轴标签和标题
    plt.xlabel("Number of Processes")
    plt.ylabel("Speedup")
    plt.title("Speedup vs Number of Processes")

    # 显示图例和网格
    plt.legend()
    plt.grid(True)

    # 显示图形
    plt.show()


if __name__ == "__main__":
    # 假设 CSV 文件与脚本在同一目录，文件名为 performance_results.csv
    plot_speedup_vs_procs("../output/performance_results.csv")
