import matplotlib.pyplot as plt
import csv

# Caminho para o arquivo CSV
caminho = "ArquivosDados/imu_data.csv"

# Listas para armazenar os dados
amostras = []
accel_x = []
accel_y = []
accel_z = []
giro_x = []
giro_y = []
giro_z = []

# Leitura do CSV com separador ponto e vírgula (;)
with open(caminho, newline='') as csvfile:
    leitor = csv.DictReader(csvfile, delimiter=';')
    for linha in leitor:
        amostras.append(int(linha['numero_amostra']))
        accel_x.append(int(linha['accel_x']))
        accel_y.append(int(linha['accel_y']))
        accel_z.append(int(linha['accel_z']))
        giro_x.append(int(linha['giro_x']))
        giro_y.append(int(linha['giro_y']))
        giro_z.append(int(linha['giro_z']))

# Plot dos dados
plt.figure(figsize=(12, 8))

plt.subplot(2, 1, 1)
plt.title("Acelerômetro")
plt.plot(amostras, accel_x, label="X")
plt.plot(amostras, accel_y, label="Y")
plt.plot(amostras, accel_z, label="Z")
plt.ylabel("Aceleração")
plt.legend()

plt.subplot(2, 1, 2)
plt.title("Giroscópio")
plt.plot(amostras, giro_x, label="X")
plt.plot(amostras, giro_y, label="Y")
plt.plot(amostras, giro_z, label="Z")
plt.ylabel("Velocidade Angular")
plt.xlabel("Número da Amostra")
plt.legend()

plt.tight_layout()
plt.show()
