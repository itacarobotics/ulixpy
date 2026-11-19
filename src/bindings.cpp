#include <pybind11/pybind11.h>
#include <pybind11/eigen.h>

#include <cstdlib>
#include <exception>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "ulixarm_cpp/ulixarm.hpp"

namespace py = pybind11;

using ulixarm::UlixArm;

namespace
{

// UlixArm plus a per-instance mutex. The GIL is released during calls into the
// library (they block on serial I/O), so the mutex keeps two Python threads
// from using the same arm at once.
struct PyUlixArm : UlixArm
{
     using UlixArm::UlixArm;
     std::mutex mutex;
};

// Wraps a UlixArm method: releases the GIL and holds the arm's mutex during the call.
// Arguments are converted from Python before the lambda runs and the return value
// is converted after it returns, both with the GIL held.
template <typename Ret, typename... Args>
auto guarded(Ret (UlixArm::*method)(Args...))
{
     return [method](PyUlixArm& self, Args... args) -> Ret {
          py::gil_scoped_release release;
          std::lock_guard<std::mutex> lock(self.mutex);
          return (self.*method)(std::forward<Args>(args)...);
     };
}

template <typename Ret, typename... Args>
auto guarded(Ret (UlixArm::*method)(Args...) const)
{
     return [method](PyUlixArm& self, Args... args) -> Ret {
          py::gil_scoped_release release;
          std::lock_guard<std::mutex> lock(self.mutex);
          return (self.*method)(std::forward<Args>(args)...);
     };
}

// Python type raised for every error coming from the C++ library.
// Kept alive for the lifetime of the process.
PyObject* ulixarm_error = nullptr;

// Flattens a std::throw_with_nested chain, outermost first.
void collect_messages(const std::exception& e, std::vector<std::string>& out)
{
     out.emplace_back(e.what());
     try {
          std::rethrow_if_nested(e);
     }
     catch (const std::exception& nested) {
          collect_messages(nested, out);
     }
     catch (...) {
          out.emplace_back("Unknown exception");
     }
}

py::object make_error(const std::string& msg)
{
     py::object text = py::reinterpret_steal<py::object>(
          PyUnicode_DecodeUTF8(msg.data(), static_cast<Py_ssize_t>(msg.size()), "replace"));
     if (!text) {
          throw py::error_already_set();
     }
     return py::reinterpret_borrow<py::object>(ulixarm_error)(text);
}

// Raises the chain as UlixArmError instances linked through __cause__, so the
// traceback shows every nested error down to the root cause.
void raise_chained(const std::exception& e)
{
     std::vector<std::string> messages;
     collect_messages(e, messages);

     py::object cause = py::none();
     for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
          py::object err = make_error(*it);
          if (!cause.is_none()) {
               err.attr("__cause__") = cause;
          }
          cause = err;
     }
     PyErr_SetObject(ulixarm_error, cause.ptr());
}

} // namespace

PYBIND11_MODULE(_core, m)
{
     m.attr("NUM_JOINTS") = ulixarm::kNumJoints;

     ulixarm_error = PyErr_NewExceptionWithDoc(
          "ulixpy.UlixArmError",
          "Error raised by the arm library.\n\n"
          "The underlying causes, if any, are chained through __cause__.",
          PyExc_RuntimeError, nullptr);
     if (!ulixarm_error) {
          throw py::error_already_set();
     }
     m.add_object("UlixArmError", py::handle(ulixarm_error));

     py::register_exception_translator([](std::exception_ptr p) {
          try {
               if (p) {
                    std::rethrow_exception(p);
               }
          }
          // pybind11's own exceptions already carry a Python error: pass them on.
          catch (const py::error_already_set&) {
               throw;
          }
          catch (const py::builtin_exception&) {
               throw;
          }
          catch (const std::exception& e) {
               raise_chained(e);
          }
     });

     py::class_<PyUlixArm>(m, "UlixArm")

     // Constructor
     .def(py::init([](bool gripper, bool camera, const std::string& urdf_path) {
               std::string path = urdf_path;
               // An empty path lets the library check $ULIXARM_DESCRIPTION_PATH.
               // Without it, fall back to the URDF installed next to this module,
               // picked from the mounted hardware the same way the library does.
               // An invalid combination (camera without gripper) is rejected by
               // the library before the URDF is loaded.
               if (path.empty() && std::getenv("ULIXARM_DESCRIPTION_PATH") == nullptr) {
                    const char* urdf_file = (gripper && camera) ? "arm_gripper_camera.urdf"
                                          : gripper           ? "arm_gripper.urdf"
                                                              : "arm.urdf";
                    py::object urdf = py::module_::import("importlib.resources")
                         .attr("files")("ulixpy").attr("joinpath")("urdf").attr("joinpath")(urdf_file);
                    path = py::str(urdf);
               }
               return new PyUlixArm(gripper, camera, path);
          }),
          py::arg("gripper") = true,
          py::arg("camera") = false,
          py::arg("urdf_path") = "",
          "Creates the arm object and sets up everything it needs to run.\n\n"
          "Loads the kinematic/dynamic model used for gravity compensation "
          "and prepares the motor lists for the manipulator joints and, "
          "if present, the gripper. It does not communicate with the "
          "hardware yet; call connect() for that.\n\n"
          "The URDF file is picked from the mounted hardware: arm.urdf "
          "(no gripper), arm_gripper.urdf (gripper) or "
          "arm_gripper_camera.urdf (gripper and camera).\n\n"
          "Args:\n"
          "    gripper: True if a gripper is mounted on the arm, False otherwise.\n"
          "    camera: True if a camera is mounted on the gripper, False otherwise.\n"
          "    urdf_path: Path to the robot URDF. Leave empty to use "
          "$ULIXARM_DESCRIPTION_PATH/urdf/<file> if that environment "
          "variable is set, or the URDF shipped with ulixpy otherwise.\n\n"
          "Raises:\n"
          "    UlixArmError: if camera is True and gripper is False (the "
          "camera is mounted on the gripper), or if the URDF cannot be loaded.")

     // Connection and motor control
     .def("connect", guarded(&UlixArm::connect),
          py::arg("port"),
          "Opens the serial connection to the arm.\n\n"
          "Connects to the motor controller on the given port and checks "
          "that every motor runs in MIT mode with the expected PMAX/VMAX/TMAX "
          "mapping. This must happen before enabling the motors or sending "
          "commands.\n\n"
          "Args:\n"
          "    port: Path of the serial device, e.g. '/dev/ttyACM0'.")

     .def("disconnect", guarded(&UlixArm::disconnect),
          "Closes the serial connection to the arm.\n\n"
          "Does not disable the motors: they keep executing the last "
          "command. Safe to call when not connected.")

     .def("enable", guarded(&UlixArm::enable),
          "Powers up the motors so the arm can move.\n\n"
          "Enables the manipulator motors and, if present, the gripper "
          "motors. The arm must be connected first.")

     .def("disable", guarded(&UlixArm::disable),
          "Powers down the motors.\n\n"
          "Disables the manipulator motors and, if present, the gripper "
          "motors. Once disabled, the arm will no longer hold its position.\n\n"
          "Warning:\n"
          "    Disabling the arm may cause it to fall, risking injury or "
          "damage to the robot or surrounding objects. Ensure the arm "
          "is in a safe pose first.")

     .def("refresh", guarded(&UlixArm::refresh),
          "Updates the arm's state from the motors.\n\n"
          "Requests the latest motor data and stores it, so get_q(), "
          "get_dq(), get_tau(), and the temperature getters return "
          "current values. Works while connected, enabled or disabled.\n\n"
          "Note:\n"
          "    Use this function at initialization only. The control() "
          "function already updates the latest motor feedback.")

     // Joint impedance control
     .def("control", guarded(&UlixArm::control),
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

     .def("hold", guarded(&UlixArm::hold),
          "Keeps the arm where it currently is.\n\n"
          "Commands the joints to stay at their present positions so the "
          "arm stays in place instead of drifting or dropping.\n\n"
          "Note:\n"
          "    This method calls refresh() internally to obtain the "
          "latest motor positions.")

     // Joint state getters
     .def("get_q", guarded(&UlixArm::get_q),
          "Gets the current joint positions.\n\n"
          "Returns the latest positions reported by the motors, based on "
          "the most recent feedback received through refresh() or control().\n\n"
          "Returns:\n"
          "    Joint angles in radians.")

     .def("get_dq", guarded(&UlixArm::get_dq),
          "Gets the current joint velocities.\n\n"
          "Returns the latest velocities reported by the motors, based on "
          "the most recent feedback received.\n\n"
          "Returns:\n"
          "    Joint velocities in rad/s.")

     .def("get_tau", guarded(&UlixArm::get_tau),
          "Gets the torque currently reported by each joint.\n\n"
          "Returns the latest torque values from the motor feedback.\n\n"
          "Returns:\n"
          "    Joint torques in Nm.")

     // Motor temperature getters
     .def("get_tmos", guarded(&UlixArm::get_tmos),
          "Gets the MOSFET temperature of each motor driver.\n\n"
          "Useful for monitoring motor drivers to prevent overheating "
          "during long or demanding motions.\n\n"
          "Returns:\n"
          "    MOSFET temperatures in degrees Celsius.")

     .def("get_trot", guarded(&UlixArm::get_trot),
          "Gets the rotor (motor winding) temperature of each joint.\n\n"
          "Useful for identifying motors that are working too hard "
          "before they reach their thermal limits.\n\n"
          "Returns:\n"
          "    Rotor temperatures in degrees Celsius.")

     // Gripper control: open
     .def("open", guarded(&UlixArm::open),
          "Opens the gripper to the default opening (80 %).")

     .def("open_width", guarded(&UlixArm::open_width),
          py::arg("meters"),
          "Opens the gripper to a given width.\n\n"
          "Args:\n"
          "    meters: Opening between the fingers, from 0.0 (closed) to "
          "0.090 (fully open). Raises an exception if outside the "
          "gripper's range.")

     .def("open_ratio", guarded(&UlixArm::open_ratio),
          py::arg("ratio"),
          "Opens the gripper to a fraction of its full opening.\n\n"
          "Args:\n"
          "    ratio: Normalized opening, from 0.0 (closed) to 1.0 (fully "
          "open). Raises an exception if outside the gripper's range.")

     // Gripper control: close
     .def("close",
          guarded(py::overload_cast<>(&UlixArm::close)),
          "Closes the gripper fully with the default grip strength.")

     .def("close",
          guarded(py::overload_cast<double>(&UlixArm::close)),
          py::arg("f"),
          "Closes the gripper with a chosen grip strength.\n\n"
          "Args:\n"
          "    f: Gripping factor from 0.0 (minimum strength) "
          "to 1.0 (maximum strength).");
}
