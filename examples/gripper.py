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

    try:
        # fully open
        robot.open()
        time.sleep(1)

        # close at default force
        robot.close()
        time.sleep(1)

        # opens 50% of max opening
        robot.open_ratio(0.5)
        time.sleep(1)

        # closes at 60% of max closure force
        robot.close(0.6)
        time.sleep(1)

        # opens at 5 cm
        robot.open_width(0.05)
        time.sleep(1)
    except KeyboardInterrupt:
        print("\nInterrupted, holding position.")
    finally:
        # Keep the arm in place, then close the connection
        robot.hold()
        robot.disconnect()


if __name__ == "__main__":
    main()
