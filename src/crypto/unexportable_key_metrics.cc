// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crypto/unexportable_key_metrics.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/timer/elapsed_timer.h"
#include "crypto/unexportable_key.h"

namespace crypto {

namespace {

enum class TPMOperation {
  kMessageSigning,
  kMessageVerify,
  kWrappedKeyCreation,
  kNewKeyCreation,
};

std::string GetHistogramSuffixForOperation(TPMOperation operation) {
  switch (operation) {
    case TPMOperation::kMessageSigning:
      return "MessageSigning";
    case TPMOperation::kMessageVerify:
      return "MessageVerify";
    case TPMOperation::kNewKeyCreation:
      return "NewKeyCreation";
    case TPMOperation::kWrappedKeyCreation:
      return "WrappedKeyCreation";
  }
  return "";
}

std::string GetHistogramSuffixForAlgo(internal::TPMSupport algo) {
  switch (algo) {
    case internal::TPMSupport::kECDSA:
      return "ECDSA";
    case internal::TPMSupport::kRSA:
      return "RSA";
    case internal::TPMSupport::kNone:
      return "";
  }
  return "";
}

void ReportUmaLatency(TPMOperation operation,
                      internal::TPMSupport algo,
                      base::TimeDelta latency) {
  std::string histogram_name = "Crypto.TPMDuration." +
                               GetHistogramSuffixForOperation(operation) +
                               GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramMediumTimes(histogram_name, latency);
}

void ReportUmaOperationSuccess(TPMOperation operation,
                               internal::TPMSupport algo,
                               bool status) {
  std::string histogram_name = "Crypto.TPMOperation." +
                               GetHistogramSuffixForOperation(operation) +
                               GetHistogramSuffixForAlgo(algo);
  base::UmaHistogramBoolean(histogram_name, status);
}

void ReportUmaTpmOperation(TPMOperation operation,
                           internal::TPMSupport algo,
                           base::TimeDelta latency,
                           bool status) {
  ReportUmaOperationSuccess(operation, algo, status);
  if (status && operation != TPMOperation::kMessageVerify) {
    // Only report latency for successful operations
    // No latency reported for verification that is done outside of TPM
    ReportUmaLatency(operation, algo, latency);
  }
}

void MeasureTpmOperationsInternal() {
  internal::TPMSupport supported_algo = internal::TPMSupport::kNone;
  std::unique_ptr<UnexportableKeyProvider> provider =
      GetUnexportableKeyProvider();
  if (!provider) {
    return;
  }

  const SignatureVerifier::SignatureAlgorithm kAllAlgorithms[] = {
      SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256,
      SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256,
  };

  auto algo = provider->SelectAlgorithm(kAllAlgorithms);
  if (algo) {
    switch (*algo) {
      case SignatureVerifier::SignatureAlgorithm::ECDSA_SHA256:
        supported_algo = internal::TPMSupport::kECDSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA256:
        supported_algo = internal::TPMSupport::kRSA;
        break;
      case SignatureVerifier::SignatureAlgorithm::RSA_PKCS1_SHA1:
      case SignatureVerifier::SignatureAlgorithm::RSA_PSS_SHA256:
        // Not supported for this metric.
        break;
    }
  }

  // Report if TPM is supported and best algo
  base::UmaHistogramEnumeration("Crypto.TPMSupport2", supported_algo);
  if (supported_algo == internal::TPMSupport::kNone) {
    return;
  }

  base::ElapsedTimer key_creation_timer;
  std::unique_ptr<UnexportableSigningKey> current_key =
      provider->GenerateSigningKeySlowly(kAllAlgorithms);
  ReportUmaTpmOperation(TPMOperation::kNewKeyCreation, supported_algo,
                        key_creation_timer.Elapsed(), current_key != nullptr);
  if (!current_key) {
    return;
  }

  base::ElapsedTimer wrapped_key_creation_timer;
  std::unique_ptr<UnexportableSigningKey> wrapped_key =
      provider->FromWrappedSigningKeySlowly(current_key->GetWrappedKey());
  ReportUmaTpmOperation(TPMOperation::kWrappedKeyCreation, supported_algo,
                        wrapped_key_creation_timer.Elapsed(),
                        wrapped_key != nullptr);

  const uint8_t msg[] = {1, 2, 3, 4};
  base::ElapsedTimer message_signing_timer;
  absl::optional<std::vector<uint8_t>> signed_bytes =
      current_key->SignSlowly(msg);
  ReportUmaTpmOperation(TPMOperation::kMessageSigning, supported_algo,
                        message_signing_timer.Elapsed(),
                        signed_bytes.has_value());
  if (!signed_bytes.has_value()) {
    return;
  }

  crypto::SignatureVerifier verifier;
  bool verify_init =
      verifier.VerifyInit(current_key->Algorithm(), signed_bytes.value(),
                          current_key->GetSubjectPublicKeyInfo());
  if (verify_init) {
    verifier.VerifyUpdate(msg);
    bool verify_final = verifier.VerifyFinal();
    ReportUmaOperationSuccess(TPMOperation::kMessageVerify, supported_algo,
                              verify_final);
  } else {
    ReportUmaOperationSuccess(TPMOperation::kMessageVerify, supported_algo,
                              verify_init);
  }
}

}  // namespace

namespace internal {

void MeasureTpmOperationsInternalForTesting() {
  MeasureTpmOperationsInternal();
}

}  // namespace internal

void MaybeMeasureTpmOperations() {
  const base::Feature kTpmLatencyMetrics{"TpmLatencyMetrics",
                                         base::FEATURE_ENABLED_BY_DEFAULT};
  if (base::FeatureList::IsEnabled(kTpmLatencyMetrics)) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&MeasureTpmOperationsInternal));
  }
}

}  // namespace crypto