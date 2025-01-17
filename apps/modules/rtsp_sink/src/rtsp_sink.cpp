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

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "cnstream_eventbus.hpp"
#include "rtsp_sink.hpp"

RtspSink::RtspSink(const std::string &name) : Module(name) {
  param_register_.SetModuleDesc("RtspSink is a module to deliver stream by RTSP protocol.");
  param_register_.Register("http-port", "Http port.");
  param_register_.Register("udp-port", "UDP port.");
  param_register_.Register("encoder-type", "Encode type.");
  param_register_.Register("frame-rate", "Frame rate.");
  param_register_.Register("cols", "Video cols.");
  param_register_.Register("rows", "Video rows.");
}

RtspSinkContext *RtspSink::GetRtspSinkContext(CNFrameInfoPtr data) {
  RtspSinkContext *ctx = nullptr;
  if (is_mosaic_style_) {
    auto search = ctxs_.find(0);
    if (search != ctxs_.end()) {
      ctx = search->second;
    } else {
      ctx = new RtspSinkContext;
      ctx->stream_ = new RTSPSinkJoinStream;

      if (!ctx->stream_->Open(data->frame.width, data->frame.height, format_, frame_rate_ /* 30000.0f / 1001 */,
                              udp_port_, http_port_, rows_, cols_,
                              enc_type == "mlu" ? RTSPSinkJoinStream::MLU : RTSPSinkJoinStream::FFMPEG)) {
        LOG(ERROR) << "[RTSPSink] Invalid parameter";
      }
      ctxs_[data->channel_idx] = ctx;
    }
  } else {
    auto search = ctxs_.find(data->channel_idx);
    if (search != ctxs_.end()) {
      // context exists
      ctx = search->second;
    } else {
      ctx = new RtspSinkContext;
      ctx->stream_ = new RTSPSinkJoinStream;

      if (!ctx->stream_->Open(data->frame.width, data->frame.height, format_, frame_rate_ /* 30000.0f / 1001 */,
                              udp_port_ + data->channel_idx, http_port_,
                              enc_type == "mlu" ? RTSPSinkJoinStream::MLU : RTSPSinkJoinStream::FFMPEG)) {
        LOG(ERROR) << "[RTSPSink] Invalid parameter";
      }
      ctxs_[data->channel_idx] = ctx;
    }
  }
  return ctx;
}

RtspSink::~RtspSink() { Close(); }

bool RtspSink::Open(ModuleParamSet paramSet) {
  if (paramSet.find("http-port") == paramSet.end() || paramSet.find("udp-port") == paramSet.end() ||
      paramSet.find("encoder-type") == paramSet.end()) {
    return false;
  }

  http_port_ = std::stoi(paramSet["http-port"]);
  udp_port_ = std::stoi(paramSet["udp-port"]);
  enc_type = paramSet["encoder-type"];
  if (paramSet.find("frame-rate") == paramSet.end()) {
    frame_rate_ = 0;
  } else {
    frame_rate_ = std::stof(paramSet["frame-rate"]);
    if (frame_rate_ < 0) frame_rate_ = 0;
  }

  if (paramSet.find("cols") != paramSet.end() && paramSet.find("rows") != paramSet.end()) {
    cols_ = std::stoi(paramSet["cols"]);
    rows_ = std::stoi(paramSet["rows"]);
    is_mosaic_style_ = true;
    LOG(INFO) << "mosaic windows cols: " << cols_ << " ,rows: " << rows_;
  }
  format_ = RTSPSinkJoinStream::BGR24;
  return true;
}

void RtspSink::Close() {
  if (ctxs_.empty()) {
    return;
  }
  for (auto &pair : ctxs_) {
    pair.second->stream_->Close();
    delete pair.second->stream_;
    delete pair.second;
  }
  ctxs_.clear();
}

int RtspSink::Process(CNFrameInfoPtr data) {
  RtspSinkContext *ctx = GetRtspSinkContext(data);
  cv::Mat image = *data->frame.ImageBGR();
  if (format_ == RTSPSinkJoinStream::YUV420P) cv::cvtColor(image, image, cv::COLOR_BGR2YUV_I420);
  if (is_mosaic_style_) {
    ctx->stream_->Update(image, data->frame.timestamp, data->channel_idx);
  } else {
    ctx->stream_->Update(image, data->frame.timestamp);
  }
  return 0;
}

bool RtspSink::CheckParamSet(ModuleParamSet paramSet) {
  ParametersChecker checker;
  for (auto &it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[RtspSink] Unknown param: " << it.first;
    }
  }

  if (paramSet.find("http-port") == paramSet.end() || paramSet.find("udp-port") == paramSet.end() ||
      paramSet.find("encoder-type") == paramSet.end()) {
    LOG(ERROR) << "RtspSink must specify [http-port], [udp-port], [encoder-type].";
    return false;
  }

  std::string err_msg;
  if (!checker.IsNum({"http-port", "udp-port", "frame-rate", "cols", "rows"}, paramSet, err_msg, true)) {
    LOG(ERROR) << "[RtspSink] " << err_msg;
    return false;
  }

  if (paramSet["encoder-type"] != "mlu" && paramSet["encoder-type"] != "ffmpeg") {
    LOG(ERROR) << "[RtspSink] Not support encoder type: " << paramSet["encoder-type"];
    return false;
  }
  return true;
}
