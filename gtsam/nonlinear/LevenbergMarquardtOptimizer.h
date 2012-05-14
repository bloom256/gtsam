/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file    LevenbergMarquardtOptimizer.h
 * @brief   
 * @author  Richard Roberts
 * @created Feb 26, 2012
 */

#pragma once

#include <gtsam/nonlinear/SuccessiveLinearizationOptimizer.h>

namespace gtsam {

class LevenbergMarquardtOptimizer;

/** Parameters for Levenberg-Marquardt optimization.  Note that this parameters
 * class inherits from NonlinearOptimizerParams, which specifies the parameters
 * common to all nonlinear optimization algorithms.  This class also contains
 * all of those parameters.
 */
class LevenbergMarquardtParams : public SuccessiveLinearizationParams {
public:
  /** See LevenbergMarquardtParams::lmVerbosity */
  enum LMVerbosity {
    SILENT,
    LAMBDA,
    TRYLAMBDA,
    TRYCONFIG,
    TRYDELTA,
    DAMPED
  };

  double lambdaInitial; ///< The initial Levenberg-Marquardt damping term (default: 1e-5)
  double lambdaFactor; ///< The amount by which to multiply or divide lambda when adjusting lambda (default: 10.0)
  double lambdaUpperBound; ///< The maximum lambda to try before assuming the optimization has failed (default: 1e5)
  LMVerbosity lmVerbosity; ///< The verbosity level for Levenberg-Marquardt (default: SILENT), see also NonlinearOptimizerParams::verbosity

  LevenbergMarquardtParams() :
    lambdaInitial(1e-5), lambdaFactor(10.0), lambdaUpperBound(1e5), lmVerbosity(SILENT) {}

  virtual ~LevenbergMarquardtParams() {}

  virtual void print(const std::string& str = "") const {
    SuccessiveLinearizationParams::print(str);
    std::cout << "              lambdaInitial: " << lambdaInitial << "\n";
    std::cout << "               lambdaFactor: " << lambdaFactor << "\n";
    std::cout << "           lambdaUpperBound: " << lambdaUpperBound << "\n";
    std::cout.flush();
  }
};

/**
 * State for LevenbergMarquardtOptimizer
 */
class LevenbergMarquardtState : public NonlinearOptimizerState {
public:

  double lambda;

  LevenbergMarquardtState() {}

  virtual ~LevenbergMarquardtState() {}

protected:
  LevenbergMarquardtState(const NonlinearFactorGraph& graph, const Values& initialValues, const LevenbergMarquardtParams& params, unsigned int iterations = 0) :
    NonlinearOptimizerState(graph, initialValues, iterations), lambda(params.lambdaInitial) {}

  friend class LevenbergMarquardtOptimizer;
};

/**
 * This class performs Levenberg-Marquardt nonlinear optimization
 */
class LevenbergMarquardtOptimizer : public NonlinearOptimizer {

public:

  typedef boost::shared_ptr<LevenbergMarquardtOptimizer> shared_ptr;

  /// @name Standard interface
  /// @{

  /** Standard constructor, requires a nonlinear factor graph, initial
   * variable assignments, and optimization parameters.  For convenience this
   * version takes plain objects instead of shared pointers, but internally
   * copies the objects.
   * @param graph The nonlinear factor graph to optimize
   * @param initialValues The initial variable assignments
   * @param params The optimization parameters
   */
  LevenbergMarquardtOptimizer(const NonlinearFactorGraph& graph, const Values& initialValues,
      const LevenbergMarquardtParams& params = LevenbergMarquardtParams()) :
        NonlinearOptimizer(graph), params_(ensureHasOrdering(params, graph, initialValues)),
        state_(graph, initialValues, params_), dimensions_(initialValues.dims(*params_.ordering)) {}

  /** Standard constructor, requires a nonlinear factor graph, initial
   * variable assignments, and optimization parameters.  For convenience this
   * version takes plain objects instead of shared pointers, but internally
   * copies the objects.
   * @param graph The nonlinear factor graph to optimize
   * @param initialValues The initial variable assignments
   * @param params The optimization parameters
   */
  LevenbergMarquardtOptimizer(const NonlinearFactorGraph& graph, const Values& initialValues, const Ordering& ordering) :
        NonlinearOptimizer(graph), dimensions_(initialValues.dims(ordering)) {
    params_.ordering = ordering;
    state_ = LevenbergMarquardtState(graph, initialValues, params_); }

  /** Access the current damping value */
  double lambda() const { return state_.lambda; }

  /// @}

  /// @name Advanced interface
  /// @{

  /** Virtual destructor */
  virtual ~LevenbergMarquardtOptimizer() {}

  /** Perform a single iteration, returning a new NonlinearOptimizer class
   * containing the updated variable assignments, which may be retrieved with
   * values().
   */
  virtual void iterate();

  /** Access the parameters */
  const LevenbergMarquardtParams& params() const { return params_; }

  /** Access the last state */
  const LevenbergMarquardtState& state() const { return state_; }

  /// @}

protected:

  LevenbergMarquardtParams params_;
  LevenbergMarquardtState state_;
  std::vector<size_t> dimensions_;

  /** Access the parameters (base class version) */
  virtual const NonlinearOptimizerParams& _params() const { return params_; }

  /** Access the state (base class version) */
  virtual const NonlinearOptimizerState& _state() const { return state_; }

  /** Internal function for computing a COLAMD ordering if no ordering is specified */
  LevenbergMarquardtParams ensureHasOrdering(LevenbergMarquardtParams params, const NonlinearFactorGraph& graph, const Values& values) const {
    if(!params.ordering)
      params.ordering = *graph.orderingCOLAMD(values);
    return params;
  }
};

}
