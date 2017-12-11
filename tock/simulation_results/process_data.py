import os
import sys


def process_file(file):
    seconds = 0
    data = []
    for line in f:
        if 'found' in line:
            num_updated = line.split(' ')[1]
            data.append((seconds, num_updated))
            seconds += 1

    return data


def output_data(filename, data):
    with open(filename + '.csv', 'w') as f:
        for entry in data:
            f.write('%s, %s\n' % (entry))


if __name__ == '__main__':
    directory = sys.argv[1]
    for file in os.listdir(directory):
        with open(directory + '/' + file, 'r') as f:
            data = process_file(f)
            output_data(file, data)
