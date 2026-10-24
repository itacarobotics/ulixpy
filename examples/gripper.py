import numpy as np

from ulixpy import UlixArm
import time

def main():
    robot = UlixArm(gripper=True)

    try:
        robot.connect("/dev/ttyACM0")
        robot.enable()
        robot.refresh()
    except RuntimeError as e:
        print(e)
        return -1

    # fully open
    robot.open()
    time.sleep(1)

    # close at default force
    robot.close()
    time.sleep(1)

    # opens 80% of max opening
    robot.open(0.8, "normal")
    time.sleep(1)

    # closes at 60% of max closure force
    robot.close(0.6)
    time.sleep(1)

    # opens at 5 cm
    robot.open(0.05, "metric")
    time.sleep(1)


if __name__ == "__main__":
    main()
