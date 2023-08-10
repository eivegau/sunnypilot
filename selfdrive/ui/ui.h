#pragma once

#include <memory>
#include <string>
#include <optional>

#include <QObject>
#include <QTimer>
#include <QColor>
#include <QFuture>
#include <QPolygonF>
#include <QTransform>

#include "cereal/messaging/messaging.h"
#include "common/modeldata.h"
#include "common/params.h"
#include "common/timing.h"

const int UI_BORDER_SIZE = 30;
const int UI_HEADER_HEIGHT = 420;

const QRect speed_sgn_rc(UI_BORDER_SIZE * 2, UI_BORDER_SIZE * 2.5 + 202, 184, 184);
const float DRIVING_PATH_WIDE = 0.9;
const float DRIVING_PATH_NARROW = 0.25;

const int UI_FREQ = 20; // Hz
const int BACKLIGHT_OFFROAD = 50;
typedef cereal::CarControl::HUDControl::AudibleAlert AudibleAlert;

const mat3 DEFAULT_CALIBRATION = {{ 0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0 }};

const vec3 default_face_kpts_3d[] = {
  {-5.98, -51.20, 8.00}, {-17.64, -49.14, 8.00}, {-23.81, -46.40, 8.00}, {-29.98, -40.91, 8.00}, {-32.04, -37.49, 8.00},
  {-34.10, -32.00, 8.00}, {-36.16, -21.03, 8.00}, {-36.16, 6.40, 8.00}, {-35.47, 10.51, 8.00}, {-32.73, 19.43, 8.00},
  {-29.30, 26.29, 8.00}, {-24.50, 33.83, 8.00}, {-19.01, 41.37, 8.00}, {-14.21, 46.17, 8.00}, {-12.16, 47.54, 8.00},
  {-4.61, 49.60, 8.00}, {4.99, 49.60, 8.00}, {12.53, 47.54, 8.00}, {14.59, 46.17, 8.00}, {19.39, 41.37, 8.00},
  {24.87, 33.83, 8.00}, {29.67, 26.29, 8.00}, {33.10, 19.43, 8.00}, {35.84, 10.51, 8.00}, {36.53, 6.40, 8.00},
  {36.53, -21.03, 8.00}, {34.47, -32.00, 8.00}, {32.42, -37.49, 8.00}, {30.36, -40.91, 8.00}, {24.19, -46.40, 8.00},
  {18.02, -49.14, 8.00}, {6.36, -51.20, 8.00}, {-5.98, -51.20, 8.00},
};

struct Alert {
  QString text1;
  QString text2;
  QString type;
  cereal::ControlsState::AlertSize size;
  cereal::ControlsState::AlertStatus status;
  AudibleAlert sound;

  bool equal(const Alert &a2) {
    return text1 == a2.text1 && text2 == a2.text2 && type == a2.type && sound == a2.sound;
  }

  static Alert get(const SubMaster &sm, uint64_t started_frame, uint64_t display_debug_alert_frame = 0) {
    const cereal::ControlsState::Reader &cs = sm["controlsState"].getControlsState();
    const uint64_t controls_frame = sm.rcv_frame("controlsState");

    Alert alert = {};
    if (display_debug_alert_frame > 0 && (sm.frame - display_debug_alert_frame) <= 1 * UI_FREQ) {
      return {"Debug snapshot collected", "",
              "debugTapDetected", cereal::ControlsState::AlertSize::SMALL,
              cereal::ControlsState::AlertStatus::NORMAL,
              AudibleAlert::WARNING_SOFT};
    } else if (controls_frame >= started_frame) {  // Don't get old alert.
      alert = {cs.getAlertText1().cStr(), cs.getAlertText2().cStr(),
               cs.getAlertType().cStr(), cs.getAlertSize(),
               cs.getAlertStatus(),
               cs.getAlertSound()};
    }

    if (!sm.updated("controlsState") && (sm.frame - started_frame) > 5 * UI_FREQ) {
      const int CONTROLS_TIMEOUT = 5;
      const int controls_missing = (nanos_since_boot() - sm.rcv_time("controlsState")) / 1e9;

      // Handle controls timeout
      if (controls_frame < started_frame) {
        // car is started, but controlsState hasn't been seen at all
        alert = {"openpilot Unavailable", "Waiting for controls to start",
                 "controlsWaiting", cereal::ControlsState::AlertSize::MID,
                 cereal::ControlsState::AlertStatus::NORMAL,
                 AudibleAlert::NONE};
      } else if (controls_missing > CONTROLS_TIMEOUT && !Hardware::PC()) {
        // car is started, but controls is lagging or died
        if (cs.getEnabled() && (controls_missing - CONTROLS_TIMEOUT) < 10) {
          alert = {"TAKE CONTROL IMMEDIATELY", "Controls Unresponsive",
                   "controlsUnresponsive", cereal::ControlsState::AlertSize::FULL,
                   cereal::ControlsState::AlertStatus::CRITICAL,
                   AudibleAlert::WARNING_IMMEDIATE};
        } else {
          alert = {"Controls Unresponsive", "Reboot Device",
                   "controlsUnresponsivePermanent", cereal::ControlsState::AlertSize::MID,
                   cereal::ControlsState::AlertStatus::NORMAL,
                   AudibleAlert::NONE};
        }
      }
    }
    return alert;
  }
};

typedef enum UIStatus {
  STATUS_DISENGAGED,
  STATUS_OVERRIDE,
  STATUS_ENGAGED,
  STATUS_MADS,
} UIStatus;

const QColor bg_colors [] = {
  [STATUS_DISENGAGED] = QColor(0x17, 0x33, 0x49, 0xc8),
  [STATUS_OVERRIDE] = QColor(0x91, 0x9b, 0x95, 0xf1),
  [STATUS_ENGAGED] = QColor(0x00, 0xc8, 0x00, 0xf1),
  [STATUS_MADS] = QColor(0x00, 0xc8, 0xc8, 0xf1),
};

static std::map<cereal::ControlsState::AlertStatus, QColor> alert_colors = {
  {cereal::ControlsState::AlertStatus::NORMAL, QColor(0x15, 0x15, 0x15, 0xf1)},
  {cereal::ControlsState::AlertStatus::USER_PROMPT, QColor(0xDA, 0x6F, 0x25, 0xf1)},
  {cereal::ControlsState::AlertStatus::CRITICAL, QColor(0xC9, 0x22, 0x31, 0xf1)},
};

const QColor tcs_colors [] = {
  [int(cereal::LongitudinalPlan::VisionTurnControllerState::DISABLED)] =  QColor(0x0, 0x0, 0x0, 0xff),
  [int(cereal::LongitudinalPlan::VisionTurnControllerState::ENTERING)] = QColor(0xC9, 0x22, 0x31, 0xf1),
  [int(cereal::LongitudinalPlan::VisionTurnControllerState::TURNING)] = QColor(0xDA, 0x6F, 0x25, 0xf1),
  [int(cereal::LongitudinalPlan::VisionTurnControllerState::LEAVING)] = QColor(0x17, 0x86, 0x44, 0xf1),
};

typedef struct UIScene {
  bool calibration_valid = false;
  bool calibration_wide_valid  = false;
  bool wide_cam = true;
  mat3 view_from_calib = DEFAULT_CALIBRATION;
  mat3 view_from_wide_calib = DEFAULT_CALIBRATION;
  cereal::PandaState::PandaType pandaType;
  cereal::ControlsState::Reader controlsState;

  // Debug UI
  bool show_debug_ui;
  bool debug_snapshot_enabled;
  uint64_t display_debug_alert_frame;

  // Speed limit control
  bool speed_limit_control_enabled;
  bool speed_limit_perc_offset;
  double last_speed_limit_sign_tap;

  // modelV2
  float lane_line_probs[4];
  float road_edge_stds[2];
  QPolygonF track_vertices;
  QPolygonF track_edge_vertices;
  QPolygonF lane_line_vertices[4];
  QPolygonF road_edge_vertices[2];
  QPolygonF lane_barrier_vertices[2];

  // lead
  QPointF lead_vertices[2];

  // DMoji state
  float driver_pose_vals[3];
  float driver_pose_diff[3];
  float driver_pose_sins[3];
  float driver_pose_coss[3];
  vec3 face_kpts_draw[std::size(default_face_kpts_3d)];

  bool navigate_on_openpilot = false;

  float light_sensor;
  bool started, ignition, is_metric, map_on_left, longitudinal_control;
  uint64_t started_frame;

  bool dynamic_lane_profile_status = true;

  bool visual_brake_lights;

  int onroadScreenOff, osoTimer, brightness, onroadScreenOffBrightness, awake;
  bool onroadScreenOffEvent;
  int sleep_time = -1;
  bool touched2 = false;

  bool stand_still_timer;

  bool hide_vego_ui, true_vego_ui;

  int chevron_data;

  bool gac;
  int gac_mode, gac_tr, gac_min, gac_max;

  bool map_visible;
  bool dev_ui_enabled;
  int dev_ui_info;
  int rn_offset;
  bool live_torque_toggle;
  bool custom_torque_toggle;

  bool touch_to_wake = false;
  int sleep_btn = -1;
  bool sleep_btn_fading_in = false;
  int sleep_btn_opacity = 20;
  bool button_auto_hide;

  bool reverse_dm_cam;

  bool e2e_long_alert_light, e2e_long_alert_lead, e2e_long_alert_ui;
  float e2eX[13] = {0};

  bool sidebar_temp;
  int sidebar_temp_options;

  // UI button sorting
  int gac_btn;

  float mads_path_scale = DRIVING_PATH_WIDE - DRIVING_PATH_NARROW;
  float mads_path_range = DRIVING_PATH_WIDE - DRIVING_PATH_NARROW;  // 0.9 - 0.25 = 0.65
} UIScene;

class UIState : public QObject {
  Q_OBJECT

public:
  UIState(QObject* parent = 0);
  void updateStatus();
  inline bool worldObjectsVisible() const {
    return sm->rcv_frame("liveCalibration") > scene.started_frame;
  }
  inline bool engaged() const {
    return scene.started && (*sm)["controlsState"].getControlsState().getEnabled();
  }

  void setPrimeType(int type);
  inline int primeType() const { return prime_type; }

  int fb_w = 0, fb_h = 0;

  std::unique_ptr<SubMaster> sm;

  UIStatus status;
  UIScene scene = {};

  QString language;

  QTransform car_space_transform;

signals:
  void uiUpdate(const UIState &s);
  void offroadTransition(bool offroad);
  void primeTypeChanged(int prime_type);

private slots:
  void update();

private:
  QTimer *timer;
  bool started_prev = false;
  int prime_type = -1;
  uint64_t last_update_params_sidebar;

  bool last_mads_enabled = false;
  bool mads_path_state = false;
  float mads_path_timestep = 4;  // UI runs at 20 Hz, therefore 0.2 second is [0.2 second / (1 / 20 Hz) = 4]
  float mads_path_count = 4;     // UI runs at 20 Hz, therefore 0.2 second is [0.2 second / (1 / 20 Hz) = 4]
};

UIState *uiState();

// device management class
class Device : public QObject {
  Q_OBJECT

public:
  Device(QObject *parent = 0);
  bool isAwake() { return awake; }
  void setOffroadBrightness(int brightness) {
    offroad_brightness = std::clamp(brightness, 0, 100);
  }

private:
  bool awake = false;
  int interactive_timeout = 0;
  bool ignition_on = false;

  int offroad_brightness = BACKLIGHT_OFFROAD;
  int last_brightness = 0;
  FirstOrderFilter brightness_filter;
  QFuture<void> brightness_future;

  void updateBrightness(const UIState &s);
  void updateWakefulness(const UIState &s);
  void setAwake(bool on);

signals:
  void displayPowerChanged(bool on);
  void interactiveTimeout();

public slots:
  void resetInteractiveTimeout();
  void update(const UIState &s);
};

Device *device();

void ui_update_params(UIState *s);
int get_path_length_idx(const cereal::XYZTData::Reader &line, const float path_height);
void update_model(UIState *s,
                  const cereal::ModelDataV2::Reader &model,
                  const cereal::UiPlan::Reader &plan);
void update_dmonitoring(UIState *s, const cereal::DriverStateV2::Reader &driverstate, float dm_fade_state, bool is_rhd);
void update_leads(UIState *s, const cereal::RadarState::Reader &radar_state, const cereal::XYZTData::Reader &line);
void update_line_data(const UIState *s, const cereal::XYZTData::Reader &line,
                      float y_off, float z_off_left, float z_off_right, QPolygonF *pvd, int max_idx, bool allow_invert);
