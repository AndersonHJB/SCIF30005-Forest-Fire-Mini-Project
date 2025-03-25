import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("../output/performance_results.csv")
plt.figure()
unique_N_values = df['N'].unique()
for n in unique_N_values:
    data_n = df[df['N'] == n].sort_values(by='nprocs')

    plt.plot(data_n['nprocs'], data_n['a'], marker='o', label=f"N={n}")

plt.xlabel("Number of Processes")
plt.ylabel("Speedup")
plt.title("Speedup vs Number of Processes")

plt.legend()
plt.grid(True)
plt.savefig("Speedup-Vs-Number-of-Processes.png")
plt.show()
plt.close()
