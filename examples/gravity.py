import select
import sys
import termios
import tty

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

    q = np.zeros(6)
    dq = np.zeros(6)
    tau = np.zeros(6)
    kp = np.zeros(6)
    kd = np.zeros(6)

    # Configure terminal for non-blocking, single-character input
    fd = sys.stdin.fileno()
    old_attr = termios.tcgetattr(fd)
    tty.setcbreak(fd)

    running = True

    try:
        while running:
            # Keep robot control running
            robot.control(q, dq, tau, kp, kd, True)

            key = sys.stdin.read(1) if select.select([sys.stdin], [], [], 0)[0] else ""
            if key == "o":
                robot.open()
            elif key == "c":
                robot.close(0.4)
            elif key == "q":
                running = False

        robot.hold()
    finally:
        # Restore terminal settings
        termios.tcsetattr(fd, termios.TCSANOW, old_attr)

    return 0


if __name__ == "__main__":
    sys.exit(main())
