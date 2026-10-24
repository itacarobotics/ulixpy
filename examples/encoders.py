import numpy as np

from ulixarm import UlixArm


def main():
    robot = UlixArm(gripper=True)

    try:
        robot.connect("/dev/ttyACM0")
        robot.enable()
        robot.refresh()
    except RuntimeError as e:
        print(e)
        return -1

    while True:
        robot.refresh()
        q = robot.get_q()
        print("q: ", q)


if __name__ == "__main__":
    main()
