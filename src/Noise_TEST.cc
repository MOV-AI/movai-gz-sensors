/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include <gtest/gtest.h>

#include <numeric>

#include <ignition/common/Console.hh>
#include <ignition/math/Rand.hh>

#include <sdf/Noise.hh>


#include "gz/sensors/Noise.hh"
#include "gz/sensors/GaussianNoiseModel.hh"

using namespace gz;

const unsigned int g_applyCount = 100;

// We will use 5 sigma (4e-5 chance of failure)
const double g_sigma = 5.0;

////////////////////////////////////////////////////////////////
// Helper function that constructs sdf strings for noise element
sdf::ElementPtr NoiseSdf(const std::string &_type, double _mean,
  double _stddev, double _biasMean, double _biasStddev, double _precision)
{
  std::ostringstream noiseStream;
  noiseStream << "<sdf version='1.6'>"
              << "  <noise type='" << _type << "'>"
              << "    <mean>" << _mean << "</mean>"
              << "    <stddev>" << _stddev << "</stddev>"
              << "    <bias_mean>" << _biasMean << "</bias_mean>"
              << "    <bias_stddev>" << _biasStddev << "</bias_stddev>"
              << "    <precision>" << _precision << "</precision>"
              << "  </noise>"
              << "</sdf>";

  sdf::ElementPtr sdf(new sdf::Element);
  sdf::initFile("noise.sdf", sdf);
  sdf::readString(noiseStream.str(), sdf);

  return sdf;
}

/// \brief Test sensor noise
class NoiseTest : public ::testing::Test
{
  // Documentation inherited
  protected: void SetUp() override
  {
    common::Console::SetVerbosity(4);
  }
};

//////////////////////////////////////////////////
// Test constructor
TEST(NoiseTest, Constructor)
{
  // Construct and nothing else
  {
    sensors::Noise noise(sensors::NoiseType::NONE);
  }

  // Construct and initialize
  {
    sensors::Noise noise(sensors::NoiseType::NONE);

    sdf::Noise noiseDom;
    noiseDom.Load(NoiseSdf("none", 0, 0, 0, 0, 0));
    noise.Load(noiseDom);
  }
}

//////////////////////////////////////////////////
// Test noise types
TEST(NoiseTest, Types)
{
  sdf::Noise noiseDom;

  // NONE type
  {
    sensors::NoisePtr noise =
        sensors::NoiseFactory::NewNoiseModel(NoiseSdf("none", 0, 0, 0, 0, 0));
    EXPECT_EQ(noise->Type(), sensors::NoiseType::NONE);

    noise = sensors::NoiseFactory::NewNoiseModel(noiseDom);
    EXPECT_EQ(noise->Type(), sensors::NoiseType::NONE);
  }

  // GAUSSIAN type
  {
    sensors::NoisePtr noise =
        sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian", 0, 0, 0, 0, 0));
    EXPECT_EQ(noise->Type(), sensors::NoiseType::GAUSSIAN);

    noiseDom.SetType(sdf::NoiseType::GAUSSIAN);
    noise = sensors::NoiseFactory::NewNoiseModel(noiseDom);
    EXPECT_EQ(noise->Type(), sensors::NoiseType::GAUSSIAN);
  }

  // GAUSSIAN_QUANTIZED type
  {
    sensors::NoisePtr noise =
        sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian_quantized", 0, 0, 0, 0, 0));
    EXPECT_EQ(noise->Type(), sensors::NoiseType::GAUSSIAN);

    noiseDom.SetType(sdf::NoiseType::GAUSSIAN_QUANTIZED);
    noise = sensors::NoiseFactory::NewNoiseModel(noiseDom);
    EXPECT_EQ(noise->Type(), sensors::NoiseType::GAUSSIAN);
  }
}

//////////////////////////////////////////////////
// Helper function for testing no noise
void NoNoise(sensors::NoisePtr _noise, unsigned int _count)
{
  // Expect no change in input value
  for (unsigned int i = 0; i < _count; ++i)
  {
    double x = math::Rand::DblUniform(-1e6, 1e6);
    EXPECT_NEAR(x, _noise->Apply(x), 1e-6);
  }
}

//////////////////////////////////////////////////
// Helper function for testing Gaussian noise
void GaussianNoise(sensors::NoisePtr _noise, unsigned int _count)
{
  sensors::GaussianNoiseModelPtr noiseModel =
      std::dynamic_pointer_cast<sensors::GaussianNoiseModel>(_noise);

  ASSERT_TRUE(noiseModel != nullptr);

  // Use constant input and repeatedly add noise to it.
  double x = 42.0;

  std::vector<double> values;
  for (unsigned int i = 0; i < _count; ++i)
  {
    double y = _noise->Apply(x);
    values.push_back(y);
  }

  // std::accumulate code from http://stackoverflow.com/questions/7616511
  double sum_values = std::accumulate(values.begin(), values.end(), 0.0);
  double mean_values = sum_values / values.size();
  std::vector<double> diff(values.size());
  std::transform(values.begin(), values.end(), diff.begin(),
      [mean_values](double value) { return value - mean_values; });
  double sq_sum = std::inner_product(
      diff.begin(), diff.end(), diff.begin(), 0.0);
  double stdev_values = std::sqrt(sq_sum / values.size());
  double variance_values = stdev_values*stdev_values;

  // The sample mean should be near_ x+mean, with standard deviation of
  // stddev / sqrt(_count)
  // https://onlinecourses.science.psu.edu/stat414/node/167
  // We will use 5 sigma (4e-5 chance of failure)
  double mean = noiseModel->Mean() + noiseModel->Bias();
  double stddev = noiseModel->StdDev();
  double sampleStdDev = g_sigma*stddev / sqrt(_count);
  EXPECT_NEAR(mean_values, x+mean, sampleStdDev);

  // The sample variance has the following variance:
  // 2 stddev^4 / (_count - 1)
  // en.wikipedia.org/wiki/Variance#Distribution_of_the_sample_variance
  // Again use 5 sigma
  double variance = stddev*stddev;
  double sampleVariance2 = 2 * variance*variance / (_count - 1);
  EXPECT_NEAR(variance_values, variance, g_sigma*sqrt(sampleVariance2));
}

//////////////////////////////////////////////////
// Test noise application
TEST(NoiseTest, ApplyNone)
{
  sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
      NoiseSdf("none", 0, 0, 0, 0, 0));

  NoNoise(noise, g_applyCount);
}

TEST(NoiseTest, ApplyGaussian)
{
  double mean, stddev, biasMean, biasStddev;

  // GAUSSIAN with zero means and standard deviations
  // should be the same as NONE
  mean = 0.0;
  stddev = 0.0;
  biasMean = 0.0;
  biasStddev = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian", mean, stddev, biasMean, biasStddev, 0));
    NoNoise(noise, g_applyCount);
  }

  // GAUSSIAN with non-zero means and standard deviations, but no bias
  mean = 10.0;
  stddev = 5.0;
  biasMean = 0.0;
  biasStddev = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian", mean, stddev, biasMean, biasStddev, 0));
    sensors::GaussianNoiseModelPtr gaussianNoise =
      std::dynamic_pointer_cast<sensors::GaussianNoiseModel>(noise);
    EXPECT_NEAR(gaussianNoise->Bias(), 0.0, 1e-6);
    GaussianNoise(noise, g_applyCount);
  }

  // GAUSSIAN with non-zero mean, exact bias, and standard deviations
  mean = 10.0;
  stddev = 5.0;
  biasMean = 100.0;
  biasStddev = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian", mean, stddev, biasMean, biasStddev, 0));
    GaussianNoise(noise, g_applyCount);
  }

  // Test bias generation
  mean = 0.0;
  stddev = 0.0;
  biasMean = 0.0;
  biasStddev = 5.0;
  {
    std::vector<double> values;
    for (unsigned int i = 0; i < g_applyCount; ++i)
    {
      sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
          NoiseSdf("gaussian", mean, stddev, biasMean, biasStddev, 0));
      sensors::GaussianNoiseModelPtr gaussianNoise =
        std::dynamic_pointer_cast<sensors::GaussianNoiseModel>(noise);
      values.push_back(gaussianNoise->Bias());
    }

    // std::accumulate code from http://stackoverflow.com/questions/7616511
    double sum_values = std::accumulate(values.begin(), values.end(), 0.0);
    double mean_values = sum_values / values.size();
    std::vector<double> diff(values.size());
    std::transform(values.begin(), values.end(), diff.begin(),
        [mean_values](double value) { return value - mean_values; });
    double sq_sum = std::inner_product(
        diff.begin(), diff.end(), diff.begin(), 0.0);
    double stdev_values = std::sqrt(sq_sum / values.size());
    double variance_values = stdev_values*stdev_values;

    // See comments in GaussianNoise function to explain these calculations.
    double sampleStdDev = g_sigma*biasStddev / sqrt(g_applyCount);
    EXPECT_NEAR(mean_values, 0.0, sampleStdDev);

    double variance = biasStddev*biasStddev;
    double sampleVariance2 = 2 * variance*variance / (g_applyCount - 1);
    EXPECT_NEAR(variance_values, variance, g_sigma*sqrt(sampleVariance2));
  }
}

TEST(NoiseTest, ApplyGaussianQuantized)
{
  double mean, stddev, biasMean, biasStddev, precision;

  // GAUSSIAN_QUANTIZED with zero means and standard deviations
  // should be the same as NONE
  mean = 0.0;
  stddev = 0.0;
  biasMean = 0.0;
  biasStddev = 0.0;
  precision = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian_quantized", mean, stddev, biasMean,
        biasStddev, precision));

    NoNoise(noise, g_applyCount);
  }

  // GAUSSIAN_QUANTIZED with non-zero means and standard deviations,
  // but no bias or precision
  mean = 10.0;
  stddev = 5.0;
  biasMean = 0.0;
  biasStddev = 0.0;
  precision = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian_quantized", mean, stddev, biasMean,
        biasStddev, precision));
    sensors::GaussianNoiseModelPtr gaussianNoise =
      std::dynamic_pointer_cast<sensors::GaussianNoiseModel>(noise);
    EXPECT_NEAR(gaussianNoise->Bias(), 0.0, 1e-6);

    GaussianNoise(noise, g_applyCount);
  }

  // GAUSSIAN with non-zero mean, exact bias, and standard deviations
  // no precision specified
  mean = 10.0;
  stddev = 5.0;
  biasMean = 100.0;
  biasStddev = 0.0;
  precision = 0.0;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian_quantized", mean, stddev, biasMean,
        biasStddev, precision));

    GaussianNoise(noise, g_applyCount);
  }

  // Test bias generation
  mean = 0.0;
  stddev = 0.0;
  biasMean = 0.0;
  biasStddev = 5.0;
  precision = 0.0;
  {
    std::vector<double> values;
    for (unsigned int i = 0; i < g_applyCount; ++i)
    {
      sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
          NoiseSdf("gaussian_quantized", mean, stddev, biasMean,
          biasStddev, precision));
      sensors::GaussianNoiseModelPtr gaussianNoise =
        std::dynamic_pointer_cast<sensors::GaussianNoiseModel>(noise);
      values.push_back(gaussianNoise->Bias());
    }

    // std::accumulate code from http://stackoverflow.com/questions/7616511
    double sum_values = std::accumulate(values.begin(), values.end(), 0.0);
    double mean_values = sum_values / values.size();
    std::vector<double> diff(values.size());
    std::transform(values.begin(), values.end(), diff.begin(),
        [mean_values](double value) { return value - mean_values; });
    double sq_sum = std::inner_product(
        diff.begin(), diff.end(), diff.begin(), 0.0);
    double stdev_values = std::sqrt(sq_sum / values.size());
    double variance_values = stdev_values*stdev_values;

    // See comments in GaussianNoise function to explain these calculations.
    double sampleStdDev = g_sigma*biasStddev / sqrt(g_applyCount);
    EXPECT_NEAR(mean_values, 0.0, sampleStdDev);

    double variance = biasStddev*biasStddev;
    double sampleVariance2 = 2 * variance*variance / (g_applyCount - 1);
    EXPECT_NEAR(variance_values, variance, g_sigma*sqrt(sampleVariance2));
  }

  // Test precision
  mean = 0.0;
  stddev = 0.0;
  biasMean = 0.0;
  biasStddev = 0.0;
  precision = 0.3;
  {
    sensors::NoisePtr noise = sensors::NoiseFactory::NewNoiseModel(
        NoiseSdf("gaussian_quantized", mean, stddev, biasMean,
        biasStddev, precision));

    EXPECT_NEAR(noise->Apply(0.32), 0.3, 1e-6);
    EXPECT_NEAR(noise->Apply(0.31), 0.3, 1e-6);
    EXPECT_NEAR(noise->Apply(0.30), 0.3, 1e-6);
    EXPECT_NEAR(noise->Apply(0.29), 0.3, 1e-6);
    EXPECT_NEAR(noise->Apply(0.28), 0.3, 1e-6);

    EXPECT_NEAR(noise->Apply(-12.92), -12.9, 1e-6);
    EXPECT_NEAR(noise->Apply(-12.91), -12.9, 1e-6);
    EXPECT_NEAR(noise->Apply(-12.90), -12.9, 1e-6);
    EXPECT_NEAR(noise->Apply(-12.89), -12.9, 1e-6);
    EXPECT_NEAR(noise->Apply(-12.88), -12.9, 1e-6);
  }
}

//////////////////////////////////////////////////
// Callback function for applying custom noise
double OnApplyCustomNoise(double _in, double /*_dt*/)
{
  return _in*2;
}

TEST(NoiseTest, OnApplyNoise)
{
  // Verify that the custom callback function is called if noise type is
  // set to CUSTOM
  sensors::NoisePtr noise(new sensors::Noise(sensors::NoiseType::CUSTOM));
  ASSERT_TRUE(noise != nullptr);
  EXPECT_TRUE(noise->Type() == sensors::NoiseType::CUSTOM);

  noise->SetCustomNoiseCallback(
    std::bind(&OnApplyCustomNoise,
      std::placeholders::_1, std::placeholders::_2));

  for (double i = 0; i < 100; i += 1)
  {
    double value = noise->Apply(i);
    EXPECT_DOUBLE_EQ(value, i*2);
  }
}

/////////////////////////////////////////////////
TEST(NoiseTest, NoiseFailures)
{
  sensors::Noise noise(sensors::NoiseType::CUSTOM);

  EXPECT_DOUBLE_EQ(9, noise.Apply(9, 0.1));
  EXPECT_DOUBLE_EQ(9, noise.ApplyImpl(9, 0.1));
  std::ostringstream out;
  noise.Print(out);
  EXPECT_EQ("Noise with type[1] does not have an overloaded Print function. "
   "No more information is available.", out.str());

  sensors::Noise noiseGaussian(sensors::NoiseType::GAUSSIAN);

  sensors::NoisePtr noiseFactory =
     sensors::NoiseFactory::NewNoiseModel(
     NoiseSdf("gaussian", 0, 0, 0, 0, 0), "camera");

  sdf::Noise sdfNoise;
  sdfNoise.SetType(static_cast<sdf::NoiseType>(99));
  sensors::NoisePtr noiseFactory2 =
     sensors::NoiseFactory::NewNoiseModel(
     sdfNoise, "camera");
}

/////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
