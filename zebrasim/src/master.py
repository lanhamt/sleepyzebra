'''
Simulator master. Coordinates simulation by updating node state. 
'''

from node import ZebraNode

class ZebraMaster(object):
    def __init__(self, epochs, tl, th, k, num_nodes):
        self._epochs = epochs
        self._nodes = []

        for i in range(num_nodes):
            self._nodes.append(ZebraNode(tl, th, k))

        self._k = k
        self._th = th
        self._tl = tl

        self._message_bus = []

    def simulate(self):
        for i in range(self._epochs):
            self.process_epoch()

    def process_epoch(self):
        for node in self._nodes:
            self._message_bus.append(node.transmit())

        for message in self._message_bus:
            for node in self._nodes:
                node.recieve(message)
