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

#ifndef MODULES_INFERENCE_SRC_BATCHING_STAGE_HPP_
#define MODULES_INFERENCE_SRC_BATCHING_STAGE_HPP_

#include <memory>

namespace edk {
class ModelLoader;
}  // namespace edk

namespace cnstream {

class CNFrameInfo;
class InferTask;
class IOResValue;
class IOResource;
class MluInputResource;
class CpuInputResource;
class RCOpResource;
class Preproc;

class BatchingStage {
 public:
  BatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize) : model_(model), batchsize_(batchsize) {}
  virtual ~BatchingStage() {}
  virtual std::shared_ptr<InferTask> Batching(std::shared_ptr<CNFrameInfo> finfo) = 0;

 protected:
  std::shared_ptr<edk::ModelLoader> model_;
  uint32_t batchsize_ = 0;
};  // class BatchingStage

class IOBatchingStage : public BatchingStage {
 public:
  IOBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize, std::shared_ptr<IOResource> output_res)
      : BatchingStage(model, batchsize), output_res_(output_res) {}
  virtual ~IOBatchingStage() {}
  std::shared_ptr<InferTask> Batching(std::shared_ptr<CNFrameInfo> finfo) override;

 protected:
  virtual void ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx, const IOResValue& value) = 0;

 private:
  using BatchingStage::batchsize_;
  uint32_t batch_idx_ = 0;
  std::shared_ptr<IOResource> output_res_ = NULL;
};  // class IOBatchingStage

class CpuPreprocessingBatchingStage : public IOBatchingStage {
 public:
  CpuPreprocessingBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                                std::shared_ptr<Preproc> preprocessor, std::shared_ptr<CpuInputResource> cpu_input_res);
  ~CpuPreprocessingBatchingStage();

 private:
  void ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx, const IOResValue& value) override;
  std::shared_ptr<Preproc> preprocessor_;
};  // class CpuPreprocessingBatchingStage

class YUVSplitBatchingStage : public IOBatchingStage {
 public:
  YUVSplitBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                        std::shared_ptr<MluInputResource> mlu_input_res);
  ~YUVSplitBatchingStage();

 private:
  void ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx, const IOResValue& value) override;
};  // class YUVSplitBatchingStage

class YUVPackedBatchingStage : public IOBatchingStage {
 public:
  YUVPackedBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                         std::shared_ptr<MluInputResource> mlu_input_res);
  ~YUVPackedBatchingStage();

 private:
  void ProcessOneFrame(std::shared_ptr<CNFrameInfo> finfo, uint32_t batch_idx, const IOResValue& value) override;
};  // class YUVPackedBatchingStage

class ResizeConvertBatchingStage : public BatchingStage {
 public:
  ResizeConvertBatchingStage(std::shared_ptr<edk::ModelLoader> model, uint32_t batchsize,
                             std::shared_ptr<RCOpResource> rcop_res);
  ~ResizeConvertBatchingStage();

  std::shared_ptr<InferTask> Batching(std::shared_ptr<CNFrameInfo> finfo);

 private:
  std::shared_ptr<RCOpResource> rcop_res_;
};  // class ResizeConvertBatchingStage

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_BATCHING_STAGE_HPP_
