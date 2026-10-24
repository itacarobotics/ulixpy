#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>

#include "ulixarm.hpp"

namespace py = pybind11;

PYBIND11_MODULE(ulixpy, m)
{
     py::class_<UlixArm>(m, "UlixArm")

     // Constructor
     .def(py::init<bool>(),
          py::arg("gripper") = true,
          "Creates the arm object and sets up everything it needs to run.\n\n"
          "Loads the kinematic/dynamic model used for gravity compensation "
          "and prepares the motor lists for the manipulator joints and, "
          "if present, the gripper. It does not communicate with the "
          "hardware yet; call connect() for that.\n\n"
          "Args:\n"
          "    gripper: True if a gripper is mounted on the arm, False otherwise.")

     // Connection and motor control
     .def("connect", &UlixArm::connect,
          py::arg("port"),
          "Opens the serial connection to the arm.\n\n"
          "Connects to the motor controller on the given port. This must "
          "happen before enabling the motors or sending commands.\n\n"
          "Args:\n"
          "    port: Path of the serial device, e.g. '/dev/ttyACM0'.")

     .def("enable", &UlixArm::enable,
          "Powers up the motors so the arm can move.\n\n"
          "Enables the manipulator motors and, if present, the gripper "
          "motors. The arm must be connected first.")

     .def("disable", &UlixArm::disable,
          "Powers down the motors.\n\n"
          "Disables the manipulator motors and, if present, the gripper "
          "motors. Once disabled, the arm will no longer hold its position.\n\n"
          "Warning:\n"
          "    Disabling the arm may cause it to fall, risking injury or "
          "damage to the robot or surrounding objects. Ensure the arm "
          "is in a safe pose first.")

     .def("refresh", &UlixArm::refresh,
          "Updates the arm's state from the motors.\n\n"
          "Requests the latest motor data and stores it, so get_q(), "
          "get_dq(), get_tau(), and the temperature getters return "
          "current values.\n\n"
          "Note:\n"
          "    Use this function at initialization only. The control() "
          "function already updates the latest motor feedback.")

     // Joint impedance control
     .def("control", &UlixArm::control,
          py::arg("q_target"),
          py::arg("dq_target"),
          py::arg("tau_target"),
          py::arg("kp"),
          py::arg("kd"),
          py::arg("gravity"),
          "Sends a control command to all manipulator joints.\n\n"
          "Each joint follows an impedance control law: a spring pulls "
          "it toward the target position (kp), a damper tracks the target "
          "velocity (kd), and a feedforward torque is added. If gravity "
          "compensation is enabled, the torque required to hold the arm "
          "against gravity is added as well.\n\n"
          "The target positions are checked against joint limits and "
          "self-collision constraints before commands are sent.\n\n"
          "Note:\n"
          "    This method does not control the gripper. Use open() "
          "and close() instead.\n\n"
          "Args:\n"
          "    q_target: Desired joint positions, in radians.\n"
          "    dq_target: Desired joint velocities, in rad/s.\n"
          "    tau_target: Feedforward torques, in Nm.\n"
          "    kp: Position stiffness gains, one per joint.\n"
          "    kd: Velocity damping gains, one per joint.\n"
          "    gravity: True to enable gravity compensation, False otherwise.")

     .def("hold", &UlixArm::hold,
          "Keeps the arm where it currently is.\n\n"
          "Commands the joints to stay at their present positions so the "
          "arm stays in place instead of drifting or dropping.\n\n"
          "Note:\n"
          "    This method calls refresh() internally to obtain the "
          "latest motor positions.")

     // Joint state getters
     .def("get_q", &UlixArm::get_q,
          "Gets the current joint positions.\n\n"
          "Returns the latest positions reported by the motors, based on "
          "the most recent feedback received through refresh() or control().\n\n"
          "Returns:\n"
          "    Joint angles in radians.")

     .def("get_dq", &UlixArm::get_dq,
          "Gets the current joint velocities.\n\n"
          "Returns the latest velocities reported by the motors, based on "
          "the most recent feedback received.\n\n"
          "Returns:\n"
          "    Joint velocities in rad/s.")

     .def("get_tau", &UlixArm::get_tau,
          "Gets the torque currently reported by each joint.\n\n"
          "Returns the latest torque values from the motor feedback.\n\n"
          "Returns:\n"
          "    Joint torques in Nm.")

     // Motor temperature getters
     .def("get_tmos", &UlixArm::get_tmos,
          "Gets the MOSFET temperature of each motor driver.\n\n"
          "Useful for monitoring motor drivers to prevent overheating "
          "during long or demanding motions.\n\n"
          "Returns:\n"
          "    MOSFET temperatures in degrees Celsius.")

     .def("get_trot", &UlixArm::get_trot,
          "Gets the rotor (motor winding) temperature of each joint.\n\n"
          "Useful for identifying motors that are working too hard "
          "before they reach their thermal limits.\n\n"
          "Returns:\n"
          "    Rotor temperatures in degrees Celsius.")

     // Gripper control: open
     .def("open",
          py::overload_cast<>(&UlixArm::open),
          "Opens the gripper all the way.")

     .def("open",
          py::overload_cast<double, std::string>(&UlixArm::open),
          py::arg("d"),
          py::arg("type"),
          "Opens the gripper to a specified width or normalized position.\n\n"
          "Args:\n"
          "    d: Desired opening. Interpretation depends on type:\n"
          "        - If type is 'metric', the opening is specified in meters,\n"
          "          from 0.0 (closed) to 0.090 (fully open).\n"
          "        - If type is 'normal', the opening is normalized from 0.0\n"
          "          (closed) to 1.0 (fully open).\n\n"
          "Values outside the valid range are limited to the gripper's "
          "physical range.")
          
     // Gripper control: close
     .def("close",
          py::overload_cast<>(&UlixArm::close),
          "Closes the gripper fully with the default grip strength.")

     .def("close",
          py::overload_cast<double>(&UlixArm::close),
          py::arg("f"),
          "Closes the gripper with a chosen grip strength.\n\n"
          "Args:\n"
          "    f: Gripping factor from 0.0 (minimum strength) "
          "to 1.0 (maximum strength).")

     // Error logging
     .def("log_errors", &UlixArm::log_errors,
          py::arg("e"),
          py::arg("level") = 0,
          "Prints an error message, including any nested exceptions.\n\n"
          "Walks through the exception and any exceptions it wraps, "
          "printing each one. Nested errors are indented further so "
          "you can follow the chain from the outermost cause to the root.\n\n"
          "Args:\n"
          "    e: The exception to report.\n"
          "    level: Indentation depth. Leave at 0 when calling it directly.");
}