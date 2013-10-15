/* ----------------------------------------------------------------------------

 * GTSAM Copyright 2010, Georgia Tech Research Corporation,
 * Atlanta, Georgia 30332-0415
 * All Rights Reserved
 * Authors: Frank Dellaert, et al. (see THANKS for the full author list)

 * See LICENSE for the license information

 * -------------------------------------------------------------------------- */

/**
 * @file SmartProjectionFactorTesting.cpp
 * @brief Example usage of SmartProjectionFactor using real datasets
 * @date August, 2013
 * @author Luca Carlone
 */

// Use a map to store landmark/smart factor pairs
#include <gtsam/base/FastMap.h>

// Both relative poses and recovered trajectory poses will be stored as Pose3 objects
#include <gtsam/geometry/Pose3.h>
#include <gtsam/geometry/PinholeCamera.h>
#include <gtsam/geometry/Cal3Bundler.h>

// Each variable in the system (poses and landmarks) must be identified with a unique key.
// We can either use simple integer keys (1, 2, 3, ...) or symbols (X1, X2, L1).
// Here we will use Symbols
#include <gtsam/inference/Symbol.h>

// We want to use iSAM2 to solve the range-SLAM problem incrementally
#include <gtsam/nonlinear/ISAM2.h>

// iSAM2 requires as input a set set of new factors to be added stored in a factor graph,
// and initial guesses for any new variables used in the added factors
#include <gtsam/nonlinear/Values.h>

// We will use a non-linear solver to batch-initialize from the first 150 frames
#include <gtsam/nonlinear/LevenbergMarquardtOptimizer.h>
#include <gtsam/nonlinear/GaussNewtonOptimizer.h>

// In GTSAM, measurement functions are represented as 'factors'. Several common factors
// have been provided with the library for solving robotics SLAM problems.
#include <gtsam/slam/PriorFactor.h>
#include <gtsam_unstable/slam/SmartProjectionFactorsCreator.h>
#include <gtsam_unstable/slam/GenericProjectionFactorsCreator.h>

// Standard headers, added last, so we know headers above work on their own
#include <boost/foreach.hpp>
#include <boost/assign.hpp>
#include <boost/assign/std/vector.hpp>
#include <fstream>
#include <iostream>

using namespace std;
using namespace gtsam;
using namespace boost::assign;
namespace NM = gtsam::noiseModel;

using symbol_shorthand::X;
using symbol_shorthand::L;

typedef PriorFactor<Pose3> Pose3Prior;
typedef SmartProjectionFactorsCreator<Pose3, Point3, Cal3_S2> SmartFactorsCreator;
typedef GenericProjectionFactorsCreator<Pose3, Point3, Cal3_S2> ProjectionFactorsCreator;
typedef FastMap<Key, int> OrderingMap;

bool debug = false;

// Write key values to file
void writeValues(string directory_, const Values& values){
  string filename = directory_ + "out_camera_poses.txt";
  ofstream fout;
  fout.open(filename.c_str());
  fout.precision(20);

  // write out camera poses
  BOOST_FOREACH(Values::ConstFiltered<Pose3>::value_type key_value, values.filter<Pose3>()) {
    fout << Symbol(key_value.key).index();
    const gtsam::Matrix& matrix= key_value.value.matrix();
    for (size_t row=0; row < 4; ++row) {
      for (size_t col=0; col < 4; ++col) {
        fout << " " << matrix(row, col);
      }
    }
    fout << endl;
  }
  fout.close();

  if(values.filter<Point3>().size() > 0) {
    // write landmarks
    filename = directory_ + "landmarks.txt";
    fout.open(filename.c_str());

    BOOST_FOREACH(Values::ConstFiltered<Point3>::value_type key_value, values.filter<Point3>()) {
      fout << Symbol(key_value.key).index();
      fout << " " << key_value.value.x();
      fout << " " << key_value.value.y();
      fout << " " << key_value.value.z();
      fout << endl;
    }
    fout.close();

  }
}

void optimizeGraphLM(NonlinearFactorGraph &graph, gtsam::Values::shared_ptr graphValues, Values &result, boost::shared_ptr<Ordering> &ordering) {
  // Optimization parameters
  LevenbergMarquardtParams params;
  params.verbosityLM = LevenbergMarquardtParams::TRYLAMBDA;
  params.verbosity = NonlinearOptimizerParams::ERROR;
  params.lambdaInitial = 1;
  params.lambdaFactor = 10;
  // Profile a single iteration
//  params.maxIterations = 1;
  params.maxIterations = 100;
  std::cout << " LM max iterations: " << params.maxIterations << std::endl;
  // // params.relativeErrorTol = 1e-5;
  params.absoluteErrorTol = 1.0;
  params.verbosityLM = LevenbergMarquardtParams::TRYLAMBDA;
  params.verbosity = NonlinearOptimizerParams::ERROR;
  params.linearSolverType = SuccessiveLinearizationParams::MULTIFRONTAL_CHOLESKY;

  cout << "Graph size: " << graph.size() << endl;
  cout << "Number of variables: " << graphValues->size() << endl;
  std::cout << " OPTIMIZATION " << std::endl;

  std::cout << "\n\n=================================================\n\n";
  if (debug) {
    graph.print("thegraph");
  }
  std::cout << "\n\n=================================================\n\n";

  if (ordering && ordering->size() > 0) {
    if (debug) {
      std::cout << "Have an ordering\n" << std::endl;
      BOOST_FOREACH(const Key& key, *ordering) {
        std::cout << key << " ";
      }
      std::cout << std::endl;
    }

    params.ordering = *ordering;

    //for (int i = 0; i < 3; i++) {
      LevenbergMarquardtOptimizer optimizer(graph, *graphValues, params);
      gttic_(GenericProjectionFactorExample_kitti);
      result = optimizer.optimize();
      gttoc_(GenericProjectionFactorExample_kitti);
      tictoc_finishedIteration_();
    //}
  } else {
    std::cout << "Using COLAMD ordering\n" << std::endl;
    //boost::shared_ptr<Ordering> ordering2(new Ordering()); ordering = ordering2;

    //for (int i = 0; i < 3; i++) {
      LevenbergMarquardtOptimizer optimizer(graph, *graphValues, params);
      params.ordering = Ordering::COLAMD(graph);
      gttic_(SmartProjectionFactorExample_kitti);
      result = optimizer.optimize();
      gttoc_(SmartProjectionFactorExample_kitti);
      tictoc_finishedIteration_();
    //}

    //*ordering = params.ordering;
    if (params.ordering) {
        std::cout << "Graph size: " << graph.size() << " ORdering: " << params.ordering->size() << std::endl;
        ordering = boost::make_shared<Ordering>(*(new Ordering()));
        *ordering = *params.ordering;
    } else {
        std::cout << "WARNING: NULL ordering!" << std::endl;
    }
  }
}

void optimizeGraphGN(NonlinearFactorGraph &graph, gtsam::Values::shared_ptr graphValues, Values &result) {
  GaussNewtonParams params;
  //params.maxIterations = 1;
  params.verbosity = NonlinearOptimizerParams::DELTA;

  GaussNewtonOptimizer optimizer(graph, *graphValues, params);
  gttic_(SmartProjectionFactorExample_kitti);
  result = optimizer.optimize();
  gttoc_(SmartProjectionFactorExample_kitti);
  tictoc_finishedIteration_();

}

void optimizeGraphISAM2(NonlinearFactorGraph &graph, gtsam::Values::shared_ptr graphValues, Values &result) {
  ISAM2 isam;
  gttic_(SmartProjectionFactorExample_kitti);
  isam.update(graph, *graphValues);
  result = isam.calculateEstimate();
  gttoc_(SmartProjectionFactorExample_kitti);
  tictoc_finishedIteration_();
}

// main
int main(int argc, char** argv) {

  //  unsigned int maxNumLandmarks = 1e+7;
  //  unsigned int maxNumPoses = 1e+7;

  // Set to true to use SmartProjectionFactor. Otherwise GenericProjectionFactor will be used
  bool useSmartProjectionFactor = false;
  bool useLM = true; 

  double linThreshold = -1.0; // negative is disabled
  double rankTolerance = 1.0;

  bool incrementalFlag = false;
  int optSkip = 200; // we optimize the graph every optSkip poses

  std::cout << "PARAM SmartFactor: " << useSmartProjectionFactor << std::endl;
  std::cout << "PARAM LM: " << useLM << std::endl;
  std::cout << "PARAM linThreshold (negative is disabled): " << linThreshold << std::endl;

  // Get home directory and dataset
  string HOME = getenv("HOME");
  string input_dir = HOME + "/data/SfM/BAL/Ladybug/";
  string datasetName = "problem-1723-156502-pre.txt";

  static SharedNoiseModel pixel_sigma(noiseModel::Unit::Create(2));
  NonlinearFactorGraph graphSmart, graphProjection;

  gtsam::Values::shared_ptr graphSmartValues(new gtsam::Values());
  gtsam::Values::shared_ptr graphProjectionValues(new gtsam::Values());
  gtsam::Values::shared_ptr loadedValues(new gtsam::Values());

  // Read in kitti dataset
  ifstream fin;
  fin.open((input_dir+datasetName).c_str());
  if(!fin) {
    cerr << "Could not open dataset" << endl;
    exit(1);
  }

  // read all measurements
  cout << "Reading dataset... " << endl;
  unsigned int numLandmarks = 0, numPoses = 0;
  Key r, l;
  double u, v;
  double x, y, z, rotx, roty, rotz, f, k1, k2;
  std::vector<Key> landmarkKeys, cameraPoseKeys;
  Values result;
  bool optimized = false;
  boost::shared_ptr<Ordering> ordering(new Ordering());

  // std::vector< boost::shared_ptr<Cal3Bundler> > K_cameras; // TODO: uncomment
  Cal3_S2::shared_ptr K(new Cal3_S2(1, 1, 0, 0, 0));

  // boost::shared_ptr<Cal3Bundler> Kbund(new Cal3Bundler());// TODO: uncomment

  SmartFactorsCreator smartCreator(pixel_sigma, K, rankTolerance, linThreshold);
  ProjectionFactorsCreator projectionCreator(pixel_sigma, K);

  // main loop: reads measurements and adds factors (also performs optimization if desired)
  // r >> pose id
  // l >> landmark id
  // (u >> u) >> measurement
  unsigned int totNumLandmarks=0, totNumPoses=0, totNumMeasurements=0;
  fin >> totNumPoses >> totNumPoses >> totNumMeasurements;

  std::vector<double> vector_u;
  std::vector<double> vector_v;
  std::vector<int> vector_r;
  std::vector<int> vector_l;

  // read measurements
  for(unsigned int i = 0; i < totNumMeasurements; i++){
    fin >> r >> l >> u >> v;
    vector_u.push_back(u);
    vector_v.push_back(v);
    vector_r.push_back(r);
    vector_l.push_back(l);
  }

  // create values
  for(unsigned int i = 0; i < totNumPoses; i++){
    // R,t,f,k1 and k2.
    fin >> x >> y >> z >> rotx >> roty >> rotz >> f >> k1 >> k2;
    // boost::shared_ptr<Cal3Bundler> Kbundler(new Cal3Bundler(f, k1, k2, 0.0, 0.0)); // TODO: uncomment
    // K_cameras.push_back(Kbundler); // TODO: uncomment
    Vector3 rotVect(rotx,roty,rotz);
    loadedValues->insert(Symbol('x',i), Pose3(Rot3::Expmap(rotVect), Point3(x,y,z) ) );
  }

  // add landmarks in standard projection factors
  if(!useSmartProjectionFactor){
    for(unsigned int i = 0; i < totNumLandmarks; i++){
      fin >> x >> y >> z;
      loadedValues->insert(Symbol('l',i), Point3(x,y,z) );
    }
  }

  // 1: add values and factors to the graph
  // 1.1: add factors
  // SMART FACTORS ..
  for(size_t i = 0; i < vector_u.size(); i++){
    l = vector_l.at(i);
    r = vector_r.at(i);
    u = vector_u.at(i);
    v = vector_v.at(i);

    if (useSmartProjectionFactor) {

      smartCreator.add(L(l), X(r), Point2(u,v), graphSmart);
      numLandmarks = smartCreator.getNumLandmarks();

      // Add initial pose value if pose does not exist
      if (!graphSmartValues->exists<Pose3>(X(r)) && loadedValues->exists<Pose3>(X(r))) {
        graphSmartValues->insert(X(r), loadedValues->at<Pose3>(X(r)));
        numPoses++;
        optimized = false;
      }

    } else {
      // or STANDARD PROJECTION FACTORS
      projectionCreator.add(L(l), X(r), Point2(u,v), pixel_sigma, K, graphProjection);
      numLandmarks = projectionCreator.getNumLandmarks();
      optimized = false;
    }
  }

  if (!useSmartProjectionFactor) {
    projectionCreator.update(graphProjection, loadedValues, graphProjectionValues);
    ordering = projectionCreator.getOrdering();
  }

  if (useSmartProjectionFactor) {
    if (useLM)
      optimizeGraphLM(graphSmart, graphSmartValues, result, ordering);
    else
      optimizeGraphISAM2(graphSmart, graphSmartValues, result);
  } else {
    if (useLM)
      optimizeGraphLM(graphProjection, graphProjectionValues, result, ordering);
    else
      optimizeGraphISAM2(graphSmart, graphSmartValues, result);
  }
  // *graphSmartValues = result; // we use optimized solution as initial guess for the next one

  optimized = true;

  writeValues("./", result);

  // if (1||debug) fprintf(stderr,"%d: %d > %d, %d > %d\n", count, numLandmarks, maxNumLandmarks, numPoses, maxNumPoses);
  exit(0);
}
