/*************************************************************************
 * Copyright (C) [2019] by Cambricon, Inc. All rights reserved
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *************************************************************************/
#include <glog/logging.h>
#include <string>

#include "cnstream_eventbus.hpp"
#include "displayer.hpp"
#include "sdl_video_player.hpp"

namespace cnstream {

Displayer::Displayer(const std::string &name) : Module(name) {
  player_ = new SDLVideoPlayer;
  param_register_.SetModuleDesc("Displayer is a module for displaying the vedio.");
  param_register_.Register("window-width", "Width for displayer window.");
  param_register_.Register("window-height", "Height for displayer window.");
  param_register_.Register("refresh-rate", "Displayer refresh rate.");
  param_register_.Register("max-channels", "Max channel number.");
  param_register_.Register("full-screen", "Whether full screen.");
  param_register_.Register("show", "Whether show.");
}

Displayer::~Displayer() { delete player_; }

bool Displayer::Open(ModuleParamSet paramSet) {
  if (paramSet.find("window-width") == paramSet.end() || paramSet.find("window-height") == paramSet.end() ||
      paramSet.find("refresh-rate") == paramSet.end() || paramSet.find("max-channels") == paramSet.end() ||
      paramSet.find("show") == paramSet.end()) {
    LOG(ERROR) << "[Displayer] [window-width] [window-height] [refresh-rate] [max-channels] [show] should be set";
    return false;
  }
  bool full_screen = false;
  if (paramSet.find("full-screen") != paramSet.end()) {
    full_screen = paramSet["full-screen"] == "true" ? true : false;
  }
  show_ = paramSet["show"] == "true" ? true : false;
  int window_w = std::stoi(paramSet["window-width"]);
  int window_h = std::stoi(paramSet["window-height"]);
  int display_rate = std::stoi(paramSet["refresh-rate"]);
  int max_chns = std::stoi(paramSet["max-channels"]);
  if (window_w < 1 || window_h < 1 || display_rate < 1 || max_chns < 1) {
    LOG(ERROR) << "[Displayer] invalid parameters";
    return false;
  }

  if (show_) {
    player_->set_window_w(window_w);
    player_->set_window_h(window_h);
    player_->set_frame_rate(display_rate);
    if (!player_->Init(max_chns)) {
      return false;
    }
    if (full_screen) {
      player_->SetFullScreen();
    }
  }
  return true;
}

void Displayer::Close() {
  if (show_) {
    player_->Destroy();
  }
}

int Displayer::Process(CNFrameInfoPtr data) {
  if (show_) {
    UpdateData ud;
    ud.img = *data->frame.ImageBGR();
    ud.chn_idx = data->channel_idx;
    player_->FeedData(ud);
  }
  return 0;
}

void Displayer::GUILoop(const std::function<void()> &quit_callback) {
  if (show_) {
    player_->EventLoop(quit_callback);
  } else {
    LOG(ERROR) << "[Displayer] [show] not set to true.";
    if (quit_callback) {
      quit_callback();
    }
  }
}

bool Displayer::CheckParamSet(ModuleParamSet paramSet) {
  if (paramSet.find("window-width") == paramSet.end() || paramSet.find("window-height") == paramSet.end() ||
      paramSet.find("refresh-rate") == paramSet.end() || paramSet.find("max-channels") == paramSet.end() ||
      paramSet.find("show") == paramSet.end()) {
    LOG(ERROR) << "Displayer must specify [window-width], [window-height], [refresh-rate], [max-channels] [show].";
    return false;
  }

  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Displayer] Unknown param: " << it.first;
    }
  }

  std::string err_msg;
  if (!checker.IsNum({"window-width", "window-height", "refresh-rate", "max-channels"}, paramSet, err_msg, true)) {
    LOG(ERROR) << "[Displayer] " << err_msg;
    return false;
  }

  if (paramSet.find("full-screen") != paramSet.end()) {
    if (paramSet["full-screen"] != "true" && paramSet["full-screen"] != "false") {
      LOG(ERROR) << "[Displayer] [full-screen] should be true or false.";
      return false;
    }
  }
  if (paramSet.find("show") != paramSet.end()) {
    if (paramSet["show"] != "true" && paramSet["show"] != "false") {
      LOG(ERROR) << "[Displayer] [show] should be true or false.";
      return false;
    }
  }
  return true;
}

}  // namespace cnstream
