files = [100, 200, 300]

for i in files:
    name = f'{i}.txt'
    with open(name, 'w') as f:
        for counter in range(0, 100):
            f.write(f'{i + counter}\n')