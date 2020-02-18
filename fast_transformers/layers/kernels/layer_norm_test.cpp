#define CATCH_CONFIG_MAIN

#include "fast_transformers/layers/kernels/layer_norm.h"

#include <chrono>

#include "catch2/catch.hpp"
#include "fast_transformers/core/aligned_scratchpad.h"
#include "fast_transformers/core/enforce.h"
#include "fast_transformers/layers/kernels/test_helper.h"
#include "loguru.hpp"

#ifdef FT_WITH_CUDA
#include "fast_transformers/core/cuda_device_context.h"
#endif

namespace fast_transformers {
namespace layers {
namespace kernels {

#ifdef FT_WITH_CUDA
TEST_CASE("layer_norm CPU and GPU correctness") {
  int64_t hidden_size = 12 * 64;

  std::vector<int64_t> batch_size_list{1, 20};
  std::vector<int64_t> seq_length_list{8, 16, 32, 48, 64, 128};
  for (auto batch_size : batch_size_list)
    for (auto seq_length : seq_length_list) {
      fast_transformers::core::Tensor gpu_gamma(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLGPU, 0));

      fast_transformers::core::Tensor cpu_gamma(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor gpu_beta(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLGPU, 0));

      fast_transformers::core::Tensor cpu_beta(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor gpu_out(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLGPU, 0));

      fast_transformers::core::Tensor cpu_out(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLCPU, 0));

      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_gamma,
                                                                 gpu_gamma);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_beta,
                                                                 gpu_beta);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_out,
                                                                 gpu_out);

      LOG_S(INFO) << "batch_size: " << batch_size
                  << " seq_length: " << seq_length;
      {
        LayerNorm<float>(cpu_gamma, cpu_beta, &cpu_out);
        LayerNorm<float>(gpu_gamma, gpu_beta, &gpu_out);
      }
      REQUIRE(
          ::fast_transformers::test::CompareCPUGPU<float>(cpu_out, gpu_out));
    }  // for
}

TEST_CASE("add_bias_layer_norm CPU and GPU correctness") {
  int64_t hidden_size = 12 * 64;

  std::vector<int64_t> batch_size_list{1, 20};
  std::vector<int64_t> seq_length_list{8, 16, 32, 48, 64, 128};
  for (auto batch_size : batch_size_list)
    for (auto seq_length : seq_length_list) {
      fast_transformers::core::Tensor gpu_input(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLGPU, 0));
      fast_transformers::core::Tensor cpu_input(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLCPU, 0));

      fast_transformers::core::Tensor gpu_bias(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLGPU, 0));
      fast_transformers::core::Tensor cpu_bias(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor gpu_gamma(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLGPU, 0));
      fast_transformers::core::Tensor cpu_gamma(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor gpu_beta(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLGPU, 0));
      fast_transformers::core::Tensor cpu_beta(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor gpu_out(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLGPU, 0));
      fast_transformers::core::Tensor cpu_out(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLCPU, 0));

      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_input,
                                                                 gpu_input);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_bias,
                                                                 gpu_bias);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_gamma,
                                                                 gpu_gamma);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_beta,
                                                                 gpu_beta);
      ::fast_transformers::test::FillDataForCPUGPUTensors<float>(cpu_out,
                                                                 gpu_out);

      std::cout << "batch_size: " << batch_size
                << " seq_length: " << seq_length;
      {
        AddBiasLayerNorm<float>(cpu_input, cpu_bias, cpu_gamma, cpu_beta,
                                &cpu_out);
        AddBiasLayerNorm<float>(gpu_input, gpu_bias, gpu_gamma, gpu_beta,
                                &gpu_out);
      }
      REQUIRE(
          ::fast_transformers::test::CompareCPUGPU<float>(cpu_out, gpu_out));

      // WARM UP
      for (int i = 0; i < 5; ++i) {
        AddBiasLayerNorm<float>(gpu_input, gpu_bias, gpu_gamma, gpu_beta,
                                &gpu_out);
      }

      cudaEvent_t start_event, stop_event;
      cudaEventCreate(&start_event);
      cudaEventCreate(&stop_event);
      auto& cuda_ctx =
          fast_transformers::core::CUDADeviceContext::GetInstance();
      auto stream = cuda_ctx.stream();
      cudaEventRecord(start_event, stream);

      int step = 150;
      for (int i = 0; i < step; ++i) {
        AddBiasLayerNorm<float>(gpu_input, gpu_bias, gpu_gamma, gpu_beta,
                                &gpu_out);
      }

      cudaEventRecord(stop_event, stream);
      cudaEventSynchronize(stop_event);
      float elapse;
      cudaEventElapsedTime(&elapse, start_event, stop_event);
      elapse /= step;
      elapse /= 1000;  // ms

      std::cout << "AddBiasLayerNorm gpu cost, "
                << batch_size * seq_length * hidden_size * sizeof(float) / 1e9 /
                       elapse
                << "GB/s, time consumed, " << elapse << std::endl;
    }  // for
}
#endif

TEST_CASE("add_bias_layer_norm CPU benchmark") {
  int64_t hidden_size = 12 * 64;

  std::vector<int64_t> batch_size_list{1, 20};
  std::vector<int64_t> seq_length_list{8, 16, 32, 48, 64, 128};
  for (auto batch_size : batch_size_list)
    for (auto seq_length : seq_length_list) {
      fast_transformers::core::Tensor cpu_input(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLCPU, 0));

      fast_transformers::core::Tensor cpu_bias(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor cpu_gamma(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor cpu_beta(
          fast_transformers::core::NewDLPackTensorT<float>({hidden_size},
                                                           kDLCPU, 0));

      fast_transformers::core::Tensor cpu_out(
          fast_transformers::core::NewDLPackTensorT<float>(
              {batch_size, seq_length, hidden_size}, kDLCPU, 0));

      test::RandomFillHost(cpu_input.mutableData<float>(), cpu_input.numel());
      test::RandomFillHost(cpu_bias.mutableData<float>(), cpu_bias.numel());
      test::RandomFillHost(cpu_gamma.mutableData<float>(), cpu_gamma.numel());
      test::RandomFillHost(cpu_beta.mutableData<float>(), cpu_beta.numel());
      test::RandomFillHost(cpu_out.mutableData<float>(), cpu_out.numel());

      std::cout << "batch_size: " << batch_size << " seq_length: " << seq_length
                << " ";

      int step = 150;
      auto start = std::chrono::system_clock::now();
      for (int i = 0; i < step; ++i) {
        AddBiasLayerNorm<float>(cpu_input, cpu_bias, cpu_gamma, cpu_beta,
                                &cpu_out);
      }
      auto end = std::chrono::system_clock::system_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::microseconds>(end - start);
      auto elapse = double(duration.count()) *
                    std::chrono::microseconds::period::num /
                    std::chrono::microseconds::period::den / step;
      std::cout << "CPU AddBiasLayerNorm cost,"
                << batch_size * seq_length * hidden_size * sizeof(float) / 1e9 /
                       elapse
                << ", GB/s" << std::endl;
    }  // for
}

}  // namespace kernels
}  // namespace layers
}  // namespace fast_transformers