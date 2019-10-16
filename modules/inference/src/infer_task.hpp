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

#ifndef MODULES_INFERENCE_SRC_INFER_TASK_HPP_
#define MODULES_INFERENCE_SRC_INFER_TASK_HPP_

#include <glog/logging.h>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace cnstream {

class InferTask;
using InferTaskSptr = std::shared_ptr<InferTask>;

class InferTask {
 public:
  std::string task_msg = "task";  // for debug.

  explicit InferTask(const std::function<int()>& task_func) {
    func_ = task_func;
    statem_ = promise_.get_future();
  }

  ~InferTask() {}

  void BindFrontTask(const InferTaskSptr& ftask) {
    if (ftask.get()) pre_task_statem_.push_back(ftask->statem_);
  }

  void BindFrontTasks(const std::vector<InferTaskSptr>& ftasks) {
    for (const auto& task : ftasks) {
      BindFrontTask(task);
    }
  }

  int Execute() {
    promise_.set_value(func_());
    func_ = NULL;  // unbind resources.
    return statem_.get();
  }

  void WaitForTaskComplete() { statem_.wait(); }

  void WaitForFrontTasksComplete() {
    for (const auto& task_statem : pre_task_statem_) {
      task_statem.wait();
    }
  }

 private:
  std::shared_ptr<std::packaged_task<int()>> tfunc_;
  std::promise<int> promise_;
  std::function<int()> func_;
  std::shared_future<int> statem_;
  std::vector<std::shared_future<int>> pre_task_statem_;
};  // class InferTask

}  // namespace cnstream

#endif  // MODULES_INFERENCE_SRC_INFER_TASK_HPP_