#pragma once
#include "lh/session.hpp"
#include "runtime/room_map.hpp"

namespace lh::openvr {
// Keep the host's raw/chaperone frame distinct from our optimizable room frame.
// Align through one shared physical base; never rewrite SteamVR's own universes.
class RawAlignment {
 public:
  bool Load(const std::string& path, std::string* error);
  bool LoadText(const std::string& lighthousedb, std::string* error);  // a lighthousedb.json's text
  bool Update(const SessionSnapshot& snapshot);
  const Eigen::Isometry3d& RawFromWorld() const { return rawFromWorld_; }
  const std::string& RoomId() const { return roomId_; }
  // How far SteamVR's level was off ours (deg), once decided this session; < 0 until then.
  double LevelTrimDegrees() const { return trimDecided_ ? trimDegrees_ : -1; }
  int LevelTrimDecisions() const { return trimDecisions_; }  // one per room this session
 private:
  runtime::RoomMap reference_;
  uint32_t anchor_ = 0;
  Eigen::Isometry3d rawFromWorld_ = Eigen::Isometry3d::Identity();
  std::string roomId_;
  static constexpr double kGrossLevelRad = 5.0 * 0.017453292519943295;
  bool levelDecided_ = false;  // once per session (Load)
  Eigen::Matrix3d levelFix_ = Eigen::Matrix3d::Identity();
  // The rest of the level (under the gross limit), once our room's own level has settled: glided
  // in at kTrimRad per second about SteamVR's origin, then held for the session.
  static constexpr double kTrimRad = 0.5 * 0.017453292519943295, kTrimWaitSeconds = 120;
  bool trimDecided_ = false;
  double trimDegrees_ = 0, trimTime_ = -1, firstTime_ = -1;
  std::string trimRoom_;  // the room the trim was decided against
  int trimDecisions_ = 0;
  Eigen::Matrix3d trimTarget_ = Eigen::Matrix3d::Identity(), trim_ = Eigen::Matrix3d::Identity();
};
}
