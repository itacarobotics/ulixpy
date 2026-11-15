#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>

#include "ulixarm_cpp/ulixarm.hpp"

namespace py = pybind11;

using ulixarm::UlixArm;

PYBIND11_MODULE(_core, m)
{
     m.attr("NUM_JOINTS") = ulixarm::kNumJoints;

     py::class_<UlixArm>(m, "UlixArm")

     // Constructor
     .def(py::init([](bool gripper, const std::string& urdf_path) {
               std::string path = urdf_path;
               if (path.empty()) {
                    // robot.urdf is installed next to this module
                    py::object urdf = py::module_::import("importlib.resources")
                         .attr("files")("ulixpy").attr("joinpath")("robot.urdf");
                    path = py::str(urdf);
               }
               return new UlixArm(gripper, path);
          }),
          py::arg("gripper") = true,
          py::arg("urdf_path") = "",
          "Creates the arm object and sets up everything it needs to run.\n\n"
          "Loads the kinematic/dynamic model used for gravity compensation "
          "and prepares the motor lists for the manipulator joints and, "
          "if present, the gripper. It does not communicate with the "
          "hardware yet; call connect() for that.\n\n"
          "Args:\n"
          "    gripper: True if a gripper is mounted on the arm, False otherwise.\n"
          "    urdf_path: Path to the robot URDF. Leave empty to use the "
          "robot.urdf shipped with ulixpy.")

     // Connection and motor control
     .def("connect", &UlixArm::connect,
          py::arg("port"),
          "Opens the serial connection to the arm.\n\n"
          "Connects to the motor controller on the given port and checks "
          "that every motor runs in MIT mode with the expected PMAX/VMAX/TMAX "
          "mapping. This must happen before enabling the motors or sending "
          "commands.\n\n"
          "Args:\n"
          "    port: Path of the serial device, e.g. '/dev/ttyACM0'.")

     .def("disconnect", &UlixArm::disconnect,
          "Closes the serial connection to the arm.\n\n"
          "Does not disable the motors: they keep executing the last "
          "command. Safe to call when not connected.")

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
          "current values. Works while connected, enabled or disabled.\n\n"
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
          "Before anything is sent, the targets and gains are checked "
          "against the joint limits, joints with kp > 0 must be close to "
          "their current position, and no motor may be overheating. "
          "Violations raise an exception.\n\n"
          "Note:\n"
          "    This method does not control the gripper. Use open_width(), "
          "open_ratio() and close() instead.\n\n"
          "Warning:\n"
          "    Self-collision is NOT checked.\n\n"
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
     .def("open", &UlixArm::open,
          "Opens the gripper to the default opening (80 %).")

     .def("open_width", &UlixArm::open_width,
          py::arg("meters"),
          "Opens the gripper to a given width.\n\n"
          "Args:\n"
          "    meters: Opening between the fingers, from 0.0 (closed) to "
          "0.090 (fully open). Raises an exception if outside the "
          "gripper's range.")

     .def("open_ratio", &UlixArm::open_ratio,
          py::arg("ratio"),
          "Opens the gripper to a fraction of its full opening.\n\n"
          "Args:\n"
          "    ratio: Normalized opening, from 0.0 (closed) to 1.0 (fully "
          "open). Raises an exception if outside the gripper's range.")

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