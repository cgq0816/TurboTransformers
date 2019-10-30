#include "fast_transformers/layers/bert_embedding.h"
#include "fast_transformers/layers/kernels/layer_norm.h"
namespace fast_transformers {
namespace layers {

template <bool Add>
static void LookupEmbedding(core::Tensor &out_tensor,
                            const core::Tensor &embedding_table,
                            const core::Tensor &ids_tensor) {
  auto *embedding = embedding_table.data<float>();
  auto *ids = ids_tensor.data<int64_t>();
  auto *out = out_tensor.mutableData<float>();
  auto num_ids = ids_tensor.numel();
  auto hidden_size = embedding_table.shape(1);
  auto vocab_size = embedding_table.shape(0);
  for (int64_t i = 0; i < num_ids; ++i) {
    int64_t id = ids[i];
    FT_ENFORCE_LT(id, vocab_size, "embedding id out of index");
    auto dst = out + i * hidden_size;
    auto src = embedding + id * hidden_size;
    if (Add) {
#pragma omp simd
      for (int64_t j = 0; j < hidden_size; ++j) {
        dst[j] += src[j];
      }
    } else {
      std::copy(src, src + hidden_size, dst);
    }
  }
}

core::Tensor BERTEmbedding::
operator()(const core::Tensor &input_ids, const core::Tensor &position_ids,
           const core::Tensor &token_type_ids) const {
  if (VLOG_IS_ON(3)) {
    std::ostringstream os;
    os << ">>>>>>>>>>>> input_ids <<<<<<<<<<<<" << std::endl;
    input_ids.Print<int64_t>(os);
    os << ">>>>>>>>>>>> position_ids <<<<<<<<<<<<" << std::endl;
    position_ids.Print<int64_t>(os);
    os << ">>>>>>>>>>>> token_type_ids <<<<<<<<<<<<" << std::endl;
    token_type_ids.Print<int64_t>(os);
    VLOG(3) << os.str();
  }
  FT_ENFORCE_EQ(
      input_ids.n_dim(), 2,
      "The input ids should be a matrix with shape [BatchSize, SeqLen].");
  auto batch_size = input_ids.shape(0);
  auto seq_length = input_ids.shape(1);
  // TODO 1. switch DeviceType::CPU 2. how should I set stride?
  auto hidden_size = word_embedings_.shape(1);
  core::Tensor output_tensor(
      core::NewDLPackTensorT<float>({batch_size, seq_length, hidden_size}));

  LookupEmbedding</*Add=*/false>(output_tensor, word_embedings_, input_ids);
  LookupEmbedding</*Add=*/true>(output_tensor, token_type_embeddings_,
                                token_type_ids);
  LookupEmbedding</*Add=*/true>(output_tensor, position_embeddings_,
                                position_ids);

  kernels::LayerNorm<float>(output_tensor, layer_norm_weights_,
                            layer_norm_bias_);

  return output_tensor;
}
void BERTEmbedding::EnforceShapeAndType() const {

  VLOG(3) << ">>>>> init BERTEmbedding <<<<<<<<";
  FT_ENFORCE_EQ(word_embedings_.device_type(), kDLCPU, "Only CPU supportted");

  VLOG(3) << ">>>>> Assert OK <<<<<<<<";

  if (VLOG_IS_ON(3)) {
    std::ostringstream os;
    os << ">>>>> word_embedings_ <<<<<<<<" << std::endl;
    word_embedings_.Print<float>(os);
    os << ">>>>> position_embeddings_ <<<<<<<<" << std::endl;
    position_embeddings_.Print<float>(os);
    os << ">>>>> token_type_embeddings_ <<<<<<<<" << std::endl;
    token_type_embeddings_.Print<float>(os);
    os << ">>>>> layer_norm_weights_ <<<<<<<<" << std::endl;
    layer_norm_weights_.Print<float>(os);
    os << ">>>>> layer_norm_bias_ <<<<<<<<" << std::endl;
    layer_norm_bias_.Print<float>(os);
    VLOG(3) << os.str();
  }
}
} // namespace layers
} // namespace fast_transformers