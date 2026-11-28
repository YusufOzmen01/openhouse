#include "raw_alignment.hpp"

#include "platform/platform.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace lh::openvr {
bool RawAlignment::Load(const std::string& path, std::string* error) {
  std::ifstream f(lh::platform::FsPath(path));
  if (!f) { if (error) *error = "cannot open reference room " + path; return false; }
  std::ostringstream text; text << f.rdbuf();
  return LoadText(text.str(), error);
}
bool RawAlignment::LoadText(const std::string& lighthousedb, std::string* error) {
  reference_ = {};
  anchor_ = 0; roomId_.clear(); rawFromWorld_.setIdentity();
  levelDecided_ = false; levelFix_.setIdentity();
  trimDecided_ = false; trimDegrees_ = 0; trimTime_ = firstTime_ = -1;
  trimTarget_.setIdentity(); trim_.setIdentity(); trimRoom_.clear();
  return reference_.ImportSteamDatabase(lighthousedb, error);
}
bool RawAlignment::Update(const SessionSnapshot& snapshot) {
  // Select the reference universe using observed base IDs, not a made-up room ID.
  if (!anchor_) {
    for (bool originOnly : {true, false}) {
      for (const auto& b : snapshot.bases) {
        if (!b.known || (originOnly && !b.origin)) continue;
        auto known = reference_.KnownConfigs().find(b.id);
        if (known == reference_.KnownConfigs().end()) continue;
        reference_.OnBaseSeen(known->second.first, known->second.second);
        if (reference_.Base(b.id)) { anchor_ = b.id; break; }
      }
      if (anchor_) break;
    }
  }
  for (const auto& b : snapshot.bases) {
    if (b.id != anchor_ || !b.known) continue;
    const auto* ref = reference_.Base(anchor_);
    if (!ref) continue;
    const auto& p = b.worldFromBase;
    Eigen::Isometry3d worldFromBase = Eigen::Isometry3d::Identity();
    worldFromBase.linear() = Eigen::Quaterniond(p.orientation[0], p.orientation[1], p.orientation[2], p.orientation[3]).normalized().toRotationMatrix();
    worldFromBase.translation() = Eigen::Vector3d(p.position.data());
    rawFromWorld_ = ref->worldFromBase * worldFromBase.inverse();
    // The host frame is SteamVR's (lighthousedb.json): its yaw and origin, with the anchor
    // where SteamVR has it, whatever our room does inside (our own re-levelling cancels out
    // here). Tools that align SteamVR's lighthouse frame to something else (motoc, Space
    // Calibrator and the like) count on the anchor staying put: some start over when it
    // turns a few degrees. A level that's grossly off (> 5 deg: a base moved since SteamVR
    // last saw it) is corrected at once, about the anchor.
    if (!levelDecided_) {
      levelDecided_ = true;
      const Eigen::Vector3d upRaw = rawFromWorld_.rotation() * Eigen::Vector3d::UnitY();
      if (std::acos(std::clamp(upRaw.y(), -1.0, 1.0)) > kGrossLevelRad)
        levelFix_ = Eigen::Quaterniond::FromTwoVectors(upRaw, Eigen::Vector3d::UnitY()).toRotationMatrix();
    }
    const Eigen::Vector3d at = ref->worldFromBase.translation();
    rawFromWorld_.linear() = levelFix_ * rawFromWorld_.rotation();
    rawFromWorld_.translation() = at + levelFix_ * (rawFromWorld_.translation() - at);
    // The rest of SteamVR's level error, which the stock driver estimated from Valve's
    // accelerometers, maybe years ago: 1.4-1.9 deg in a user's 2021 universe made the view
    // lean one way facing forward and the other way turned around, and no floor tool fixes
    // a tilt. Our room's level comes from the devices' own gravity, so once it has settled
    // (or after kTrimWaitSeconds, whatever it is) the frame is turned to it, gliding in, about
    // SteamVR's origin (the play area's floor: its height there stays), and then held for the
    // session: the same each session within our level's accuracy, never a frame that drifts.
    // A new room (a room reset) has its own level, learned again: the trim decided against the old one
    // no longer holds (after a reset, the old room, imported from this very universe, had needed none, and the
    // universe's own tilt came back for the rest of the session). Decided again once the new level has
    // settled, gliding from where it is.
    const double now = snapshot.hostTime;
    if (snapshot.roomId != trimRoom_) {
      if (!trimRoom_.empty()) { trimDecided_ = false; firstTime_ = now; }
      trimRoom_ = snapshot.roomId;
    }
    if (firstTime_ < 0) firstTime_ = now;
    if (!trimDecided_ && (snapshot.levelSettled || now - firstTime_ > kTrimWaitSeconds)) {
      trimDecided_ = true;
      const Eigen::Vector3d up = rawFromWorld_.rotation() * Eigen::Vector3d::UnitY();
      trimTarget_ = Eigen::Quaterniond::FromTwoVectors(up, Eigen::Vector3d::UnitY()).toRotationMatrix();
      trimDegrees_ = std::acos(std::clamp(up.y(), -1.0, 1.0)) * 57.29577951308232;
      ++trimDecisions_;
    }
    const double dt = trimTime_ < 0 ? 0 : std::clamp(now - trimTime_, 0.0, 0.1);
    trimTime_ = now;
    const Eigen::AngleAxisd left(trimTarget_ * trim_.transpose());
    if (left.angle() > 1e-9)
      trim_ = Eigen::AngleAxisd(std::min(left.angle(), kTrimRad * dt), left.axis()).toRotationMatrix() * trim_;
    rawFromWorld_ = Eigen::Isometry3d(trim_) * rawFromWorld_;
    roomId_ = reference_.Active()->id;
    return true;
  }
  // Do not apply an alignment to an unrelated room, or to an invalidated anchor.
  anchor_ = 0; roomId_.clear(); rawFromWorld_.setIdentity();
  return false;
}
}
