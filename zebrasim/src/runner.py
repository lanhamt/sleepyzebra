from master import ZebraMaster

if __name__ == '__main__':
    epochs = 500
    tl = 1
    th = 1000
    k = 1
    num_nodes = 100
    zm = ZebraMaster(epochs, tl, th, k, num_nodes)
    zm.simulate()
