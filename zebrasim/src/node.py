'''
ZebraSim node object.
Represents node in lossy wireless sensor network
'''

class ZebraNode(object):
    def __init__(self, tl, th, k, travel_distance=0, loss=0):
        initial_x = random.uniform(MIN_X / 2, MAX_X / 2)
        initial_y = random.uniform(MIN_Y / 2, MAX_Y / 2)
        self._coord = (initial_x, initial_y)
        self._version = 0
        self._event_log = []
        self.update_log()
        self._counter = 0
        self._k = k
        self._tl = tl
        self._th = th
        self._broadcast_buffer = []
        self._t = 0

        self._travel = travel_distance
        self._loss = loss

    def transmit(self):
        buf = self._broadcast_buffer[:]
        if not buf:
            if self._t * 2 > self._th:
                self._t = self._th
            else:
                self._t *= 2
        self._broadcast_buffer.clear()
        return buf

    def get_location(self):
        return self._coord

    def recieve(self, version):
        t = random.randrange(0, self._t)

        if self._loss and random.random() < self._loss:
            pass
        else:
            if version <= self._version:
                self._counter += 1
                if self._counter >= self._k:
                    self._counter = 0
                else:
                    self.broadcast_version(t)
            else:
                self._version = version
                self.broadcast_version()

    def broadcast_version(self, t):
        self._broadcast_buffer.append((t, self._version))

    def update_log(self):
        self._event_log.append((datetime.now(), self._version))

    def update_location(self):
        distance = random.uniform(0, MAX_TRAVEL_DISTANCE)
        degree = random.uniform(0, 360)
        delta_x = math.degrees(math.sin(degree)) * distance
        delta_y = math.degrees(math.cos(degree)) * distance

        last_x = self._coord[0]
        last_y = self._coord[1]

        self._coord[0] = min(MAX_X, last_x + delta_x)
        self._coord[1] = min(MAX_Y, last_y + delta_y)

        self._coord[0] = max(MIN_X, self._coord[0])
        self._coord[1] = max(MIN_Y, self._coord[1])


def calculate_distance(x1, y1, x2, y2):
    return math.sqrt((x2 - x1)**2 + (y2 - y1)**2)
