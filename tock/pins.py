import time
import RPi.GPIO as gpio

from datetime import datetime


TIMEOUT = 1

MIN_PIN_NUMBER = 2
MAX_PIN_NUMBER = 27
NUM_PINS = 13


def count_high_pins(pins):
    count = 0
    for pin in pins:
        if gpio.input(pin) == gpio.HIGH:
            count += 1
    return count


def setup(pins):
    gpio.setmode(gpio.BCM)

    for pin in pins:
        gpio.setup(pin, gpio.OUT)
        gpio.output(pin, gpio.LOW)

    print 'initialization: %d high pins' % count_high_pins(pins)

    for pin in pins:
        print 'setting up pin %d' % pin
        gpio.setup(pin, gpio.IN)


def process_pins(pins):
    while True:
        num_high = count_high_pins(pins)
        if num_high == NUM_PINS:
            return
        print 'found %d pins high' % num_high

        time.sleep(TIMEOUT)


if __name__ == '__main__':
    pins = range(MIN_PIN_NUMBER, MAX_PIN_NUMBER + 1)
    channel = setup(pins)

    start_time = datetime.now()
    process_pins(pins)
    print datetime.now() - start_time
