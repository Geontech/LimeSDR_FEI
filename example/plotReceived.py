import matplotlib.pyplot as plt
import sys

if len(sys.argv) != 3:
	print("usage: ", sys.argv[0], " <xlim_min> <xlim_max>")
	sys.exit(255)

xlim_min = int(sys.argv[1])
xlim_max = int(sys.argv[2])

imag_data = []
with open("./imag.txt", "r+") as imag:
	i = 0
	for line in imag:
		line = line.strip()
		#print(line)
		if i > xlim_min:
			if i > xlim_max:
				break
			try:
				if line != "" and float(line) < 10 and float(line) >= -10:
					imag_data = imag_data + [float(line)]
					i = i + 1
			except:
				print(line)
		else:
			i = i + 1
		
print(len(imag_data))
plt.plot(range(xlim_min, xlim_max), imag_data)
plt.xlim(xlim_min, xlim_max)
plt.savefig("test.png")
