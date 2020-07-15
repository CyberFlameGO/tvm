/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*!
 * \file auto_schedule/measure.h
 * \brief Distributed measurement infrastructure to measure the runtime costs of tensor programs.
 * These functions are responsible for building the tvm module, uploading it to remote devices,
 * recording the running time costs, and checking the correctness of the output.
 *
 * We separate the measurement into two steps: build and run.
 * A builder builds the executable binary files and a runner runs the binary files to get the
 * measurement results. The flow of data structures is
 *
 *                 `ProgramBuilder`                 `ProgramRunner`
 * `MeasureInput` -----------------> `BuildResult` ----------------> `MeasureResult`
 *
 * We implement these in python to utilize python's multiprocessing and error handling.
 */

#ifndef TVM_AUTO_SCHEDULE_MEASURE_H_
#define TVM_AUTO_SCHEDULE_MEASURE_H_

#include <string>
#include <unordered_map>
#include <utility>

#include "loop_state.h"
#include "search_task.h"

namespace tvm {
namespace auto_schedule {

class SearchPolicy;
class MeasureInput;
class MeasureResult;

/*! \brief The error code of one measurement */
enum class MeasureErrorNO : int {
  /*! \brief No error. */
  kNoError = 0,
  /*! \brief Errors happen when apply transform steps from init state. */
  kInstantiationError = 1,
  /*! \brief Errors happen when compiling code on host. (when build module) */
  kCompileHostError = 2,
  /*! \brief Errors happen when compiling code on device. (when load module) */
  kCompileDeviceError = 3,
  /*! \brief Errors happen when run program on device. */
  kRuntimeDeviceError = 4,
  /*! \brief Answer is wrong when compared to a reference output. */
  kWrongAnswerError = 5,
  /*! \brief Timeout during compilation. */
  kBuildTimeoutError = 6,
  /*! \brief Timeout during run. */
  kRunTimeoutError = 7,
  /*! \brief Unknown error. */
  kUnknonwError = 8,
};

// Inputs and results of one measurement

/*! \brief Store the input of a measurement */
class MeasureInputNode : public Object {
 public:
  /*! \brief The search task. */
  SearchTask task;
  /*! \brief The program state to be measured. */
  State state;

  void VisitAttrs(tvm::AttrVisitor* v) {
    v->Visit("task", &task);
    v->Visit("state", &state);
  }

  /*! \brief Do shallow copy. */
  MeasureInput copy() const;

  static constexpr const char* _type_key = "auto_schedule.MeasureInput";
  TVM_DECLARE_FINAL_OBJECT_INFO(MeasureInputNode, Object);
};

/*!
 * \brief Managed reference to MeasureInputNode.
 * \sa MeasureInputNode
 */
class MeasureInput : public ObjectRef {
 public:
  /*!
   * \brief The constructor.
   * \param task The SearchTeask of this measure.
   * \param state The State to be measured.
   */
  MeasureInput(SearchTask task, State state);

  TVM_DEFINE_OBJECT_REF_METHODS(MeasureInput, ObjectRef, MeasureInputNode);
};

/*! \brief Store the result of a build. */
class BuildResultNode : public Object {
 public:
  /*! \brief The filename of built binary file. */
  String filename;
  /*! \brief The arguments. */
  Array<te::Tensor> args;
  /*! \brief The error code. (0 means no error, see MeasureErrorNO) */
  int error_no;
  /*! \brief The error message if there is any error. */
  String error_msg;
  /*! \brief The time cost of build. */
  double time_cost;

  void VisitAttrs(tvm::AttrVisitor* v) {
    v->Visit("filename", &filename);
    v->Visit("args", &args);
    v->Visit("error_no", &error_no);
    v->Visit("error_msg", &error_msg);
    v->Visit("time_cost", &time_cost);
  }

  static constexpr const char* _type_key = "auto_schedule.BuildResult";
  TVM_DECLARE_FINAL_OBJECT_INFO(BuildResultNode, Object);
};

/*!
 * \brief Managed reference to BuildResultNode.
 * \sa BuildResultNode
 */
class BuildResult : public ObjectRef {
 public:
  /*!
   * \brief The constructor.
   * \param filename The filename of built binary file.
   * \param args The arguments.
   * \param error_no The error code.
   * \param error_msg The error message if there is any error.
   * \param time_cost The time cost of build.
   */
  BuildResult(String filename, Array<te::Tensor> args, int error_no, String error_msg,
              double time_cost);
  TVM_DEFINE_OBJECT_REF_METHODS(BuildResult, ObjectRef, BuildResultNode);
};

/*! \brief Store the results of a measurement. */
class MeasureResultNode : public Object {
 public:
  /*! \brief The time costs of execution. */
  Array<PrimExpr> costs;
  /*! \brief The error code. (0 means no error, see MeasureErrorNO) */
  int error_no;
  /*! \brief The error message if there is any error. */
  String error_msg;
  /*! \brief The time cost of build and run. */
  double all_cost;
  /*! \brief The time stamps of this measurement. */
  double timestamp;

  void VisitAttrs(tvm::AttrVisitor* v) {
    v->Visit("costs", &costs);
    v->Visit("error_no", &error_no);
    v->Visit("error_msg", &error_msg);
    v->Visit("all_cost", &all_cost);
    v->Visit("timestamp", &timestamp);
  }

  /*! \brief Do shallow copy. */
  MeasureResult copy() const;

  static constexpr const char* _type_key = "auto_schedule.MeasureResult";
  TVM_DECLARE_FINAL_OBJECT_INFO(MeasureResultNode, Object);
};

/*!
 * \brief Managed reference to MeasureResultNode.
 * \sa MeasureResultNode
 */
class MeasureResult : public ObjectRef {
 public:
  /*!
   * \brief The constructor.
   * \param costs The time costs of execution.
   * \param error_no The error code.
   * \param error_msg The error message if there is any error.
   * \param all_cost The time cost of build and run.
   * \param timestamp The time stamps of this measurement.
   */
  MeasureResult(Array<PrimExpr> costs, int error_no, String error_msg, double all_cost,
                double timestamp);

  TVM_DEFINE_OBJECT_REF_METHODS(MeasureResult, ObjectRef, MeasureResultNode);
};

/*! \brief Bass class of measurement callbacks */
class MeasureCallbackNode : public Object {
 public:
  /*!
   * \brief Callback function that will be called on measurement input/result pairs
   * after measurement.
   * \param policy The current search policy.
   * \param inputs An Array of MeasureInput.
   * \param results An Array of MeasureResult.
   */
  virtual void Callback(const SearchPolicy& policy, const Array<MeasureInput>& inputs,
                        const Array<MeasureResult>& results) = 0;
  static constexpr const char* _type_key = "auto_schedule.MeasureCallback";
  TVM_DECLARE_BASE_OBJECT_INFO(MeasureCallbackNode, Object);
};

/*!
 * \brief Managed reference to MeasureCallbackNode.
 * \sa MeasureCallbackNode
 */
class MeasureCallback : public ObjectRef {
 public:
  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(MeasureCallback, ObjectRef, MeasureCallbackNode);
};

// The base class of ProgramBuilders and ProgramRunners.

/*! \brief ProgramBuilder that builds the programs */
class ProgramBuilderNode : public Object {
 public:
  /*! \brief The number of tasks to run in parallel */
  int n_parallel;
  /*! \brief Timeout of a build */
  int timeout;

  /*!
   * \brief Build programs and return results.
   * \param inputs An Array of MeasureInput.
   * \param verbose Verbosity level. 0 for silent, 1 to output information during program
   * building.
   * \return An Array of MeasureResult.
   */
  virtual Array<BuildResult> Build(const Array<MeasureInput>& inputs, int verbose) = 0;

  static constexpr const char* _type_key = "auto_schedule.ProgramBuilder";
  TVM_DECLARE_BASE_OBJECT_INFO(ProgramBuilderNode, Object);
};

/*!
 * \brief Managed reference to ProgramBuilderNode.
 * \sa ProgramBuilderNode
 */
class ProgramBuilder : public ObjectRef {
 public:
  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(ProgramBuilder, ObjectRef, ProgramBuilderNode);
};

/*! \brief ProgramRunner that runs the built programs and measure the time cost. */
class ProgramRunnerNode : public Object {
 public:
  /*! \brief Timeout of a run. */
  int timeout;

  /*!
   * \brief Run measurement and return results.
   * \param inputs An Array of MeasureInput.
   * \param build_results An Array of BuildResult.
   * \param verbose Verbosity level. 0 for silent, 1 to output information during program
   * running.
   * \return An Array of MeasureResult.
   */
  virtual Array<MeasureResult> Run(const Array<MeasureInput>& inputs,
                                   const Array<BuildResult>& build_results, int verbose) = 0;

  static constexpr const char* _type_key = "auto_schedule.ProgramRunner";
  TVM_DECLARE_BASE_OBJECT_INFO(ProgramRunnerNode, Object);
};

/*!
 * \brief Managed reference to ProgramRunnerNode.
 * \sa ProgramRunnerNode
 */
class ProgramRunner : public ObjectRef {
 public:
  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(ProgramRunner, ObjectRef, ProgramRunnerNode);
};

// Implementation of various builders and runners

/*! \brief LocalBuilder use local CPU cores to build programs in parallel */
class LocalBuilderNode : public ProgramBuilderNode {
 public:
  /*! \brief Build function. */
  String build_func;

  Array<BuildResult> Build(const Array<MeasureInput>& inputs, int verbose) final;

  static constexpr const char* _type_key = "auto_schedule.LocalBuilder";
  TVM_DECLARE_FINAL_OBJECT_INFO(LocalBuilderNode, ProgramBuilderNode);
};

/*!
 * \brief Managed reference to LocalBuilderNode.
 * \sa LocalBuilderNode
 */
class LocalBuilder : public ProgramBuilder {
 public:
  /*!
   * \brief The constructor.
   * \param timeout The timeout limit (in second) for each build thread.
   * This will be used in a wrapper of the multiprocessing.Process.join().
   * \param n_parallel Number of threads used to build in parallel.
   * \param build_func The name of registered build function.
   */
  LocalBuilder(int timeout, int n_parallel, const String& build_func);

  TVM_DEFINE_OBJECT_REF_METHODS(LocalBuilder, ProgramBuilder, LocalBuilderNode);
};

/*! \brief LocalRunner that uses local CPU/GPU to measures the time cost of programs */
class LocalRunnerNode : public ProgramRunnerNode {
 public:
  /*! \brief Number of measure times. */
  int number;
  /*! \brief Number of repeat times in each measure. */
  int repeat;
  /*! \brief The minimum duration of one repeat in milliseconds. */
  int min_repeat_ms;
  /*! \brief The cool down interval between two measurements. */
  double cooldown_interval;

  Array<MeasureResult> Run(const Array<MeasureInput>& inputs,
                           const Array<BuildResult>& build_results, int verbose) final;

  static constexpr const char* _type_key = "auto_schedule.LocalRunner";
  TVM_DECLARE_FINAL_OBJECT_INFO(LocalRunnerNode, ProgramRunnerNode);
};

/*!
 * \brief Managed reference to LocalRunnerNode.
 * \sa LocalRunnerNode
 */
class LocalRunner : public ProgramRunner {
 public:
  /*!
   * \brief The constructor. See the corresponding class in python/tvm/auto_schedule/measure.py
   * for more detailed parameter explaination.
   * \param timeout The timeout limit (in second) for each run.
   * This is used in a wrapper of the multiprocessing.Process.join().
   * \param number Number of measure times.
   * \param repeat Number of repeat times in each measure.
   * \param min_repeat_ms The minimum duration of one repeat in milliseconds.
   * \param cooldown_interval The cool down interval between two measurements.
   */
  LocalRunner(int timeout, int number, int repeat, int min_repeat_ms, double cooldown_interval);

  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(LocalRunner, ProgramRunner, LocalRunnerNode);
};

/*!
 * \brief Measurer that measures the time costs of tvm programs
 * This class combines ProgramBuilder and ProgramRunner, and provides a simpler API */
class ProgramMeasurerNode : public Object {
 public:
  /*! \brief Measured programs counter. */
  int ct;
  /*! \brief Continuous error counter. */
  int error_ct;
  /*! \brief Workload key to best flops map. */
  std::unordered_map<std::string, double> best_flops;
  /*! \brief Workload key to best state map. */
  std::unordered_map<std::string, State> best_state;
  /*! \brief Workload key to best state's count index map. */
  std::unordered_map<std::string, int> best_ct;
  /*! \brief The ProgramBuilder to build each program. */
  ProgramBuilder builder;
  /*! \brief The ProgramRunner to measure each program. */
  ProgramRunner runner;
  /*! \brief MeasureCallback to be called after each measure batch. */
  Optional<Array<MeasureCallback>> callbacks;
  /*! \brief Verbosity level. 0 for silent, 1 to output information during program measuring. */
  int verbose;
  /*! \brief The number of max continuous error. */
  int max_continous_error;

  /*! \brief Reset book keeping variables */
  void Reset();

  /*!
   * \brief Do measurement.
   * \param task The current SearchTask.
   * \param policy The current SearchPolicy.
   * \param inputs The MeasureInputs.
   * \param results A pointer to a MeasureResult Array, this is used as output.
   * \param batch_size Number of programs to be measured in one batch.
   */
  void Measure(const SearchTask& task, const SearchPolicy& policy,
               const Array<MeasureInput>& inputs, Array<MeasureResult>* results,
               int batch_size = -1);
  /*!
   * \brief Do measurement silently.
   * This API will not print the measure results to screen.
   * \param task The current SearchTask.
   * \param inputs The MeasureInputs.
   * \param results A pointer to a MeasureResult Array, this is used as output.
   */
  void SilentMeasure(const SearchTask& task, const Array<MeasureInput>& inputs,
                     Array<MeasureResult>* results);

  /*! \brief The default max continuous error setting. */
  static const int DEFAULT_MAX_CONTINOUS_ERROR = 150;

  static constexpr const char* _type_key = "auto_schedule.ProgramMeasurer";
  TVM_DECLARE_FINAL_OBJECT_INFO(ProgramMeasurerNode, Object);
};

/*!
 * \brief Managed reference to ProgramMeasurerNode.
 * \sa ProgramMeasurerNode
 */
class ProgramMeasurer : public ObjectRef {
 public:
  /*!
   * \brief The constructor.
   * \param builder The ProgramBuilder to build each program.
   * \param runner The ProgramRunner to measure each program.
   * \param callbacks MeasureCallback to be called after each measure batch.
   * \param verbose Verbosity level. 0 for silent, 1 to output information during program
   * measuring.
   * \param max_continous_error The number of max continuous error.
   */
  ProgramMeasurer(ProgramBuilder builder, ProgramRunner runner,
                  Optional<Array<MeasureCallback>> callbacks, int verbose,
                  int max_continous_error = -1);

  TVM_DEFINE_MUTABLE_OBJECT_REF_METHODS(ProgramMeasurer, ObjectRef, ProgramMeasurerNode);
};

}  // namespace auto_schedule
}  // namespace tvm

#endif  // TVM_AUTO_SCHEDULE_MEASURE_H_