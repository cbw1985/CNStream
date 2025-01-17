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

#include <easyinfer/mlu_context.h>
#include <easyinfer/model_loader.h>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include "infer_engine.hpp"
#include "infer_trans_data_helper.hpp"
#include "postproc.hpp"
#include "preproc.hpp"

#include "inferencer.hpp"

namespace cnstream {

struct InferContext {
  std::shared_ptr<InferEngine> engine;
  std::shared_ptr<InferTransDataHelper> trans_data_helper;
  int drop_count = 0;
};  // struct InferContext

using InferContextSptr = std::shared_ptr<InferContext>;

class InferencerPrivate {
 public:
  explicit InferencerPrivate(Inferencer* q) : q_ptr_(q) {}
  std::shared_ptr<edk::ModelLoader> model_loader_;
  std::shared_ptr<Preproc> pre_proc_;
  std::shared_ptr<Postproc> post_proc_;
  int device_id_ = 0;
  int interval_ = 0;
  uint32_t bsize_ = 1;
  float batching_timeout_ = 3000.0;  // ms
  std::map<std::thread::id, InferContextSptr> ctxs_;
  std::mutex ctx_mtx_;

  void InferEngineErrorHnadleFunc(const std::string& err_msg) {
    LOG(FATAL) << err_msg;
    q_ptr_->PostEvent(EVENT_ERROR, err_msg);
  }

  InferContextSptr GetInferContext() {
    std::thread::id tid = std::this_thread::get_id();
    InferContextSptr ctx(nullptr);
    std::lock_guard<std::mutex> lk(ctx_mtx_);
    if (ctxs_.find(tid) != ctxs_.end()) {
      ctx = ctxs_[tid];
    } else {
      ctx = std::make_shared<InferContext>();
      ctx->engine = std::make_shared<InferEngine>(
          device_id_, model_loader_, pre_proc_, post_proc_, bsize_, batching_timeout_,
          std::bind(&InferencerPrivate::InferEngineErrorHnadleFunc, this, std::placeholders::_1));
      ctx->trans_data_helper = std::make_shared<InferTransDataHelper>(q_ptr_);
      ctxs_[tid] = ctx;
    }
    return ctx;
  }

 private:
  DECLARE_PUBLIC(q_ptr_, Inferencer);
};  // class InferencerPrivate

Inferencer::Inferencer(const std::string& name) : Module(name) {
  d_ptr_ = nullptr;
  hasTransmit_.store(1);  // transmit data by module itself
  param_register_.SetModuleDesc("Inferencer is a module for running offline model inference.");
  param_register_.Register("model_path", "The offline model path.");
  param_register_.Register("func_name", "The offline model func name.");
  param_register_.Register("postproc_name", "The postproc name.");
  param_register_.Register("peproc_name", "The preproc name.");
  param_register_.Register("device_id", "Device ID.");
  param_register_.Register("batching_timeout", "Batch timeout time.");
  param_register_.Register("data_order", "Data order.");
  param_register_.Register("batch_size", "The batch size.");
  param_register_.Register("infer_interval_", "Infer interval.");
}

Inferencer::~Inferencer() {}

bool Inferencer::Open(ModuleParamSet paramSet) {
  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
    LOG(WARNING) << "Inferencer must specify [model_path]、[func_name]、[postproc_name].";
    return false;
  }

  d_ptr_ = new InferencerPrivate(this);
  if (!d_ptr_) {
    return false;
  }

  std::string model_path = paramSet["model_path"];
  model_path = GetPathRelativeToTheJSONFile(model_path, paramSet);

  std::string func_name = paramSet["func_name"];
  std::string Data_Order;
  if (paramSet.find("data_order") != paramSet.end()) {
    Data_Order = paramSet["data_order"];
  }
  try {
    d_ptr_->model_loader_ = std::make_shared<edk::ModelLoader>(model_path, func_name);
    if (d_ptr_->model_loader_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] load model failed, model path: " << model_path;
      return false;
    }
    if (Data_Order == "NCHW") {
      for (uint32_t index = 0; index < d_ptr_->model_loader_->InputNum(); ++index) {
        edk::DataLayout layout;
        layout.dtype = edk::DataType::FLOAT32;
        layout.order = edk::DimOrder::NCHW;
        d_ptr_->model_loader_->SetCpuInputLayout(layout, index);
      }
      for (uint32_t index = 0; index < d_ptr_->model_loader_->OutputNum(); ++index) {
        edk::DataLayout layout;
        layout.dtype = edk::DataType::FLOAT32;
        layout.order = edk::DimOrder::NCHW;
        d_ptr_->model_loader_->SetCpuOutputLayout(layout, index);
      }
    }
    d_ptr_->model_loader_->InitLayout();
    std::string postproc_name = paramSet["postproc_name"];
    d_ptr_->post_proc_ = std::shared_ptr<cnstream::Postproc>(cnstream::Postproc::Create(postproc_name));
    if (d_ptr_->post_proc_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] Can not find Postproc implemention by name: " << postproc_name;
      return false;
    }
  } catch (edk::Exception& e) {
    LOG(ERROR) << "model path:" << model_path << ". " << e.what();
    return false;
  }

  d_ptr_->pre_proc_ = nullptr;
  auto preproc_name = paramSet.find("preproc_name");
  if (preproc_name != paramSet.end()) {
    d_ptr_->pre_proc_ = std::shared_ptr<Preproc>(Preproc::Create(preproc_name->second));
    if (d_ptr_->pre_proc_.get() == nullptr) {
      LOG(ERROR) << "[Inferencer] CPU preproc name not found: " << preproc_name->second;
      return false;
    }
    LOG(INFO) << "[Inferencer] With CPU preproc set";
  }

  d_ptr_->device_id_ = 0;
  if (paramSet.find("device_id") != paramSet.end()) {
    std::stringstream ss;
    int device_id;
    ss << paramSet["device_id"];
    ss >> device_id;
    d_ptr_->device_id_ = device_id;
  }

#ifdef CNS_MLU100
  if (paramSet.find("batch_size") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batch_size"];
    ss >> d_ptr_->bsize_;
  }
#elif CNS_MLU270
  d_ptr_->bsize_ = d_ptr_->model_loader_->InputShapes()[0].n;
#endif
  DLOG(INFO) << GetName() << " batch size:" << d_ptr_->bsize_;

  if (paramSet.find("infer_interval") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["infer_interval"];
    ss >> d_ptr_->interval_;
    LOG(INFO) << GetName() << " infer_interval:" << d_ptr_->interval_;
  }

  // batching timeout
  if (paramSet.find("batching_timeout") != paramSet.end()) {
    std::stringstream ss;
    ss << paramSet["batching_timeout"];
    ss >> d_ptr_->batching_timeout_;
    LOG(INFO) << GetName() << " batching timeout:" << d_ptr_->batching_timeout_;
  }

  if (container_ == nullptr) {
    LOG(INFO) << name_ << " has not been added into pipeline.";
  } else {
  }

  /* hold this code. when all threads that set the cnrt device id exit, cnrt may release the memory itself */
  edk::MluContext ctx;
  ctx.SetDeviceId(d_ptr_->device_id_);
  ctx.ConfigureForThisThread();

  return true;
}

void Inferencer::Close() {
  if (nullptr == d_ptr_) return;

  /*destroy infer contexts*/
  d_ptr_->ctx_mtx_.lock();
  d_ptr_->ctxs_.clear();
  d_ptr_->ctx_mtx_.unlock();

  delete d_ptr_;
  d_ptr_ = nullptr;
}

int Inferencer::Process(CNFrameInfoPtr data) {
  std::shared_ptr<InferContext> pctx = d_ptr_->GetInferContext();

  bool eos = data->frame.flags & CNFrameFlag::CN_FRAME_FLAG_EOS;
  bool drop_data = d_ptr_->interval_ > 0 && pctx->drop_count++ % d_ptr_->interval_ != 0;

  if (eos || drop_data) {
    if (drop_data) pctx->drop_count %= d_ptr_->interval_;
    std::shared_ptr<std::promise<void>> promise = std::make_shared<std::promise<void>>();
    promise->set_value();
    InferEngine::ResultWaitingCard card(promise);
    pctx->trans_data_helper->SubmitData(std::make_pair(data, card));
  } else {
    InferEngine::ResultWaitingCard card = pctx->engine->FeedData(data);
    pctx->trans_data_helper->SubmitData(std::make_pair(data, card));
  }

  return 1;
}

bool Inferencer::CheckParamSet(ModuleParamSet paramSet) {
  ParametersChecker checker;
  for (auto& it : paramSet) {
    if (!param_register_.IsRegisted(it.first)) {
      LOG(WARNING) << "[Inferencer] Unknown param: " << it.first;
    }
  }
  if (paramSet.find("model_path") == paramSet.end() || paramSet.find("func_name") == paramSet.end() ||
      paramSet.find("postproc_name") == paramSet.end()) {
    LOG(ERROR) << "Inferencer must specify [model_path], [func_name], [postproc_name].";
    return false;
  }
  if (!checker.CheckPath(paramSet["model_path"], paramSet)) {
    LOG(ERROR) << "[Inferencer] [model_path] : " << paramSet["model_path"] << " non-existence.";
    return false;
  }
  std::string err_msg;
  if (!checker.IsNum({"batching_timeout", "device_id"}, paramSet, err_msg)) {
    LOG(ERROR) << "[Inferencer] " << err_msg;
    return false;
  }
  return true;
}

}  // namespace cnstream
