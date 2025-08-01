/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
/*
   main Rover class, containing all vehicle specific state
*/
#pragma once

#include <cmath>
#include <stdarg.h>
#include <stdint.h>

// Libraries
#include <AP_Common/AP_Common.h>
#include <AP_HAL/AP_HAL.h>
#include <AP_BattMonitor/AP_BattMonitor.h>          // Battery monitor library
#include <AP_Camera/AP_Camera.h>                    // Camera triggering
#include <AP_Mount/AP_Mount.h>                      // Camera/Antenna mount
#include <AP_Param/AP_Param.h>
#include <AP_RangeFinder/AP_RangeFinder.h>          // Range finder library
#include <AP_RCMapper/AP_RCMapper.h>                // RC input mapping library
#include <AP_RPM/AP_RPM.h>                          // RPM input library
#include <AP_Scheduler/AP_Scheduler.h>              // main loop scheduler
#include <AP_Vehicle/AP_Vehicle.h>                  // needed for AHRS build
#include <AP_WheelEncoder/AP_WheelEncoder.h>
#include <AP_WheelEncoder/AP_WheelRateControl.h>
#include <AP_Logger/AP_Logger.h>
#include <AP_OSD/AP_OSD.h>
#include <AR_Motors/AP_MotorsUGV.h>
#include <AP_Mission/AP_Mission.h>
#include <AP_Mission/AP_Mission_ChangeDetector.h>
#include <AR_WPNav/AR_WPNav_OA.h>
#include <AP_OpticalFlow/AP_OpticalFlow.h>
#include <AC_PrecLand/AC_PrecLand_config.h>
#include <AP_Follow/AP_Follow_config.h>
#include <AP_ExternalControl/AP_ExternalControl_config.h>
#if AP_EXTERNAL_CONTROL_ENABLED
#include "AP_ExternalControl_Rover.h"
#endif

// Configuration
#include "defines.h"
#include "config.h"

#if AP_SCRIPTING_ENABLED
#include <AP_Scripting/AP_Scripting.h>
#endif

// Local modules
#include "AP_Arming_Rover.h"
#include "sailboat.h"
#if AP_ROVER_ADVANCED_FAILSAFE_ENABLED
#include "afs_rover.h"
#endif
#include "Parameters.h"
#include "GCS_MAVLink_Rover.h"
#include "GCS_Rover.h"
#include "AP_Rally.h"
#if AC_PRECLAND_ENABLED
#include <AC_PrecLand/AC_PrecLand.h>
#endif
#include "RC_Channel_Rover.h"                  // RC Channel Library

#include "mode.h"

class Rover : public AP_Vehicle {
public:
    friend class GCS_MAVLINK_Rover;
    friend class Parameters;
    friend class ParametersG2;
    friend class AP_Rally_Rover;
    friend class AP_Arming_Rover;
#if AP_ROVER_ADVANCED_FAILSAFE_ENABLED
    friend class AP_AdvancedFailsafe_Rover;
#endif
#if AP_EXTERNAL_CONTROL_ENABLED
    friend class AP_ExternalControl_Rover;
#endif
    friend class GCS_Rover;
    friend class Mode;
    friend class ModeAcro;
    friend class ModeAuto;
    friend class ModeCircle;
    friend class ModeGuided;
    friend class ModeHold;
    friend class ModeLoiter;
    friend class ModeSteering;
    friend class ModeManual;
    friend class ModeRTL;
    friend class ModeSmartRTL;
#if MODE_FOLLOW_ENABLED
    friend class ModeFollow;
#endif
    friend class ModeSimple;
#if MODE_DOCK_ENABLED
    friend class ModeDock;
#endif

    friend class RC_Channel_Rover;
    friend class RC_Channels_Rover;

    friend class Sailboat;

    Rover(void);

private:

    // must be the first AP_Param variable declared to ensure its
    // constructor runs before the constructors of the other AP_Param
    // variables
    AP_Param param_loader;

    // all settable parameters
    Parameters g;
    ParametersG2 g2;

    // mapping between input channels
    RCMapper rcmap;

    // primary control channels
    RC_Channel *channel_steer;
    RC_Channel *channel_throttle;
    RC_Channel *channel_lateral;
    RC_Channel *channel_roll;
    RC_Channel *channel_pitch;
    RC_Channel *channel_walking_height;

    // flight modes convenience array
    AP_Int8 *modes;
    const uint8_t num_modes = 6;

    // Arming/Disarming management class
    AP_Arming_Rover arming;

    // external control implementation
#if AP_EXTERNAL_CONTROL_ENABLED
    AP_ExternalControl_Rover external_control;
#endif

#if AP_OPTICALFLOW_ENABLED
    AP_OpticalFlow optflow;
#endif

#if OSD_ENABLED || OSD_PARAM_ENABLED
    AP_OSD osd;
#endif
#if AC_PRECLAND_ENABLED
    AC_PrecLand precland;
#endif
    // GCS handling
    GCS_Rover _gcs;  // avoid using this; use gcs()
    GCS_Rover &gcs() { return _gcs; }

    // RC Channels:
    RC_Channels_Rover &rc() { return g2.rc_channels; }

    // The rover's current location
    Location current_loc;

    // Camera
#if AP_CAMERA_ENABLED
    AP_Camera camera{MASK_LOG_CAMERA};
#endif

    // Camera/Antenna mount tracking and stabilisation stuff
#if HAL_MOUNT_ENABLED
    AP_Mount camera_mount;
#endif

    // true if initialisation has completed
    bool initialised;

    // This is the state of the flight control system
    // There are multiple states defined such as MANUAL, AUTO, ...
    Mode *control_mode;

    // Used to maintain the state of the previous control switch position
    // This is set to -1 when we need to re-read the switch
    uint8_t oldSwitchPosition;

    // structure for holding failsafe state
    struct {
        uint8_t bits;               // bit flags of failsafes that have started (but not necessarily triggered an action)
        uint32_t start_time;        // start time of the earliest failsafe
        uint8_t triggered;          // bit flags of failsafes that have triggered an action
        uint32_t last_valid_rc_ms;  // system time of most recent RC input from pilot
        bool ekf;
    } failsafe;

    // true if we have a position estimate from AHRS
    bool have_position;

#if AP_RANGEFINDER_ENABLED
    // range finder last update for each instance (used for DPTH logging)
    uint32_t rangefinder_last_reading_ms[RANGEFINDER_MAX_INSTANCES];
#endif

    // Ground speed
    // The amount current ground speed is below min ground speed.  meters per second
    float ground_speed;

    // Battery Sensors
    AP_BattMonitor battery{MASK_LOG_CURRENT,
                           FUNCTOR_BIND_MEMBER(&Rover::handle_battery_failsafe, void, const char*, const int8_t),
                           _failsafe_priorities};

    // flyforward timer
    uint32_t flyforward_start_ms;

    static const AP_Scheduler::Task scheduler_tasks[];

    static const AP_Param::Info var_info[];
#if HAL_LOGGING_ENABLED
    static const LogStructure log_structure[];
#endif

    // latest wheel encoder values
    float wheel_encoder_last_distance_m[WHEELENCODER_MAX_INSTANCES];    // total distance recorded by wheel encoder (for reporting to GCS)
    bool wheel_encoder_initialised;                                     // true once arrays below have been initialised to sensors initial values
    float wheel_encoder_last_angle_rad[WHEELENCODER_MAX_INSTANCES];     // distance in radians at time of last update to EKF
    uint32_t wheel_encoder_last_reading_ms[WHEELENCODER_MAX_INSTANCES]; // system time of last ping from each encoder
    uint8_t wheel_encoder_last_index_sent;                              // index of the last wheel encoder sent to the EKF

    // True when we are doing motor test
    bool motor_test;

    ModeInitializing mode_initializing;
    ModeHold mode_hold;
    ModeManual mode_manual;
    ModeAcro mode_acro;
    ModeGuided mode_guided;
    ModeAuto mode_auto;
    ModeLoiter mode_loiter;
    ModeSteering mode_steering;
    ModeRTL mode_rtl;
    ModeSmartRTL mode_smartrtl;
#if MODE_FOLLOW_ENABLED
    ModeFollow mode_follow;
#endif
    ModeSimple mode_simple;
#if MODE_DOCK_ENABLED
    ModeDock mode_dock;
#endif

    // cruise throttle and speed learning
    typedef struct {
        LowPassFilterFloat speed_filt{2.0f};
        LowPassFilterFloat throttle_filt{2.0f};
        uint32_t learn_start_ms;
        uint32_t log_count;
    } cruise_learn_t;
    cruise_learn_t cruise_learn;

    // Rover.cpp
#if AP_SCRIPTING_ENABLED || AP_EXTERNAL_CONTROL_ENABLED
    bool set_target_location(const Location& target_loc) override;
#endif

#if AP_SCRIPTING_ENABLED
    bool set_target_velocity_NED(const Vector3f& vel_ned) override;
    bool set_steering_and_throttle(float steering, float throttle) override;
    bool get_steering_and_throttle(float& steering, float& throttle) override;
    // set desired turn rate (degrees/sec) and speed (m/s). Used for scripting
    bool set_desired_turn_rate_and_speed(float turn_rate, float speed) override;
    bool set_desired_speed(float speed) override;
    bool get_control_output(AP_Vehicle::ControlOutput control_output, float &control_value) override;
    bool nav_scripting_enable(uint8_t mode) override;
    bool nav_script_time(uint16_t &id, uint8_t &cmd, float &arg1, float &arg2, int16_t &arg3, int16_t &arg4) override;
    void nav_script_time_done(uint16_t id) override;
#endif // AP_SCRIPTING_ENABLED
    void ahrs_update();
    void gcs_failsafe_check(void);
    void update_logging1(void);
    void update_logging2(void);
    void one_second_loop(void);
    void update_current_mode(void);

    // balance_bot.cpp
    void balancebot_pitch_control(float &throttle);
    bool is_balancebot() const;

    // commands.cpp
    bool set_home_to_current_location(bool lock) override WARN_IF_UNUSED;
    bool set_home(const Location& loc, bool lock) override WARN_IF_UNUSED;
    void update_home();

    // crash_check.cpp
    void crash_check();

    // cruise_learn.cpp
    void cruise_learn_start();
    void cruise_learn_update();
    void cruise_learn_complete();
    void log_write_cruise_learn() const;

    // ekf_check.cpp
    void ekf_check();
    bool ekf_over_threshold();
    bool ekf_position_ok();
    void failsafe_ekf_event();
    void failsafe_ekf_off_event(void);

    // failsafe.cpp
    void failsafe_trigger(uint8_t failsafe_type, const char* type_str, bool on);
    void handle_battery_failsafe(const char* type_str, const int8_t action);
#if AP_ROVER_ADVANCED_FAILSAFE_ENABLED
    void afs_fs_check(void);
#endif
#if AP_FENCE_ENABLED
    // fence.cpp
    void fence_checks_async() override;
    void fence_check();
#endif
    // GCS_Mavlink.cpp
    void send_wheel_encoder_distance(mavlink_channel_t chan);

#if HAL_LOGGING_ENABLED
    // methods for AP_Vehicle:
    const AP_Int32 &get_log_bitmask() override { return g.log_bitmask; }
    const struct LogStructure *get_log_structures() const override {
        return log_structure;
    }
    uint8_t get_num_log_structures() const override;

    // Log.cpp
    void Log_Write_Attitude();
    void Log_Write_Depth();
    void Log_Write_GuidedTarget(uint8_t target_type, const Vector3f& pos_target, const Vector3f& vel_target);
    void Log_Write_Nav_Tuning();
    void Log_Write_Sail();
    void Log_Write_Steering();
    void Log_Write_Throttle();
    void Log_Write_RC(void);
    void Log_Write_Vehicle_Startup_Messages();
    void Log_Read(uint16_t log_num, uint16_t start_page, uint16_t end_page);
#endif

    // mode.cpp
    Mode *mode_from_mode_num(enum Mode::Number num);

    // Parameters.cpp
    void load_parameters(void) override;

    // precision_landing.cpp
    void init_precland();
    void update_precland();

    // radio.cpp
    void set_control_channels(void) override;
    void init_rc_in();
    void read_radio();
    void radio_failsafe_check(uint16_t pwm);

    // sensors.cpp
    void update_compass(void);
    void update_wheel_encoder();
#if AP_RANGEFINDER_ENABLED
    void read_rangefinders(void);
#endif

    // Steering.cpp
    void set_servos(void);

    // Rover.cpp
    void get_scheduler_tasks(const AP_Scheduler::Task *&tasks,
                             uint8_t &task_count,
                             uint32_t &log_bit) override;

    // system.cpp
    void init_ardupilot() override;
    void startup_ground(void);
    void update_ahrs_flyforward();
    bool gcs_mode_enabled(const Mode::Number mode_num) const;
    bool set_mode(Mode &new_mode, ModeReason reason);
    bool set_mode(const uint8_t new_mode, ModeReason reason) override;
    bool set_mode(Mode::Number new_mode, ModeReason reason);
    uint8_t get_mode() const override { return (uint8_t)control_mode->mode_number(); }
    bool current_mode_requires_mission() const override {
        return control_mode == &mode_auto;
    }

    void startup_INS(void);
    void notify_mode(const Mode *new_mode);
    uint8_t check_digital_pin(uint8_t pin);
    bool should_log(uint32_t mask);
    bool is_boat() const;

    // vehicle specific waypoint info helpers
    bool get_wp_distance_m(float &distance) const override;
    bool get_wp_bearing_deg(float &bearing) const override;
    bool get_wp_crosstrack_error_m(float &xtrack_error) const override;

    enum class FailsafeAction: int8_t {
        None          = 0,
        RTL           = 1,
        Hold          = 2,
        SmartRTL      = 3,
        SmartRTL_Hold = 4,
        Terminate     = 5,
        Loiter_Hold   = 6,
    };

    enum class Failsafe_Options : uint32_t {
        Failsafe_Option_Active_In_Hold = (1<<0)
    };

    static constexpr int8_t _failsafe_priorities[] = {
                                                       (int8_t)FailsafeAction::Terminate,
                                                       (int8_t)FailsafeAction::Hold,
                                                       (int8_t)FailsafeAction::RTL,
                                                       (int8_t)FailsafeAction::SmartRTL_Hold,
                                                       (int8_t)FailsafeAction::SmartRTL,
                                                       (int8_t)FailsafeAction::None,
                                                       -1 // the priority list must end with a sentinel of -1
                                                      };
    static_assert(_failsafe_priorities[ARRAY_SIZE(_failsafe_priorities) - 1] == -1,
                  "_failsafe_priorities is missing the sentinel");


public:
    void failsafe_check();
    // Motor test
    void motor_test_output();
    bool mavlink_motor_test_check(const GCS_MAVLINK &gcs_chan, bool check_rc, AP_MotorsUGV::motor_test_order motor_instance, uint8_t throttle_type, int16_t throttle_value);
    MAV_RESULT mavlink_motor_test_start(const GCS_MAVLINK &gcs_chan, AP_MotorsUGV::motor_test_order motor_instance, uint8_t throttle_type, int16_t throttle_value, float timeout_sec);
    void motor_test_stop();

    // frame type
    uint8_t get_frame_type() const { return g2.frame_type.get(); }
    AP_WheelRateControl& get_wheel_rate_control() { return g2.wheel_rate_control; }

    // Simple mode
    float simple_sin_yaw;

#if AP_ROVER_AUTO_ARM_ONCE_ENABLED
    struct {
        uint32_t last_arm_attempt_ms;
        bool done;
    } auto_arm_once;
    void handle_auto_arm_once();
#endif  // AP_ROVER_AUTO_ARM_ONCE_ENABLED
};

extern Rover rover;

using AP_HAL::millis;
using AP_HAL::micros;
