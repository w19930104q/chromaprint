#include <gtest/gtest.h>
#include <stdint.h>
#include <algorithm>
#include <vector>
#include <fstream>
#include "chromaprint.h"
#include "test_utils.h"
#include "utils/scope_exit.h"

namespace chromaprint {

TEST(API, TestFp) {
	std::vector<short> data = LoadAudioFile("data/test_stereo_44100.raw");

	ChromaprintContext *ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_TEST2);
	ASSERT_NE(nullptr, ctx);
	SCOPE_EXIT(chromaprint_free(ctx));

	ASSERT_EQ(1, chromaprint_get_num_channels(ctx));
	ASSERT_EQ(11025, chromaprint_get_sample_rate(ctx));

	ASSERT_EQ(1, chromaprint_start(ctx, 44100, 1));
	ASSERT_EQ(1, chromaprint_feed(ctx, data.data(), data.size()));

	char *fp;
	uint32_t fp_hash;

	ASSERT_EQ(1, chromaprint_finish(ctx));
	ASSERT_EQ(1, chromaprint_get_fingerprint(ctx, &fp));
	SCOPE_EXIT(chromaprint_dealloc(fp));
	ASSERT_EQ(1, chromaprint_get_fingerprint_hash(ctx, &fp_hash));

	EXPECT_EQ(std::string("AQAAC0kkZUqYREkUnFAXHk8uuMZl6EfO4zu-4ABKFGESWIIMEQE"), std::string(fp));
	ASSERT_EQ(3732003127, fp_hash);
}

TEST(API, Test2SilenceFp)
{
	short zeroes[1024];
	std::fill(zeroes, zeroes + 1024, 0);

	ChromaprintContext *ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_TEST2);
	ASSERT_NE(nullptr, ctx);
	SCOPE_EXIT(chromaprint_free(ctx));

	ASSERT_EQ(1, chromaprint_start(ctx, 44100, 1));
	for (int i = 0; i < 130; i++) {
		ASSERT_EQ(1, chromaprint_feed(ctx, zeroes, 1024));
	}

	char *fp;
	uint32_t fp_hash;

	ASSERT_EQ(1, chromaprint_finish(ctx));
	ASSERT_EQ(1, chromaprint_get_fingerprint(ctx, &fp));
	SCOPE_EXIT(chromaprint_dealloc(fp));
	ASSERT_EQ(1, chromaprint_get_fingerprint_hash(ctx, &fp_hash));

	ASSERT_EQ(18, strlen(fp));
	EXPECT_EQ(std::string("AQAAA0mUaEkSRZEGAA"), std::string(fp));
	ASSERT_EQ(627964279, fp_hash);
}

TEST(API, Test2SilenceRawFp)
{
	short zeroes[1024];
	std::fill(zeroes, zeroes + 1024, 0);

	ChromaprintContext *ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_TEST2);
	ASSERT_NE(nullptr, ctx);
	SCOPE_EXIT(chromaprint_free(ctx));

	ASSERT_EQ(1, chromaprint_start(ctx, 44100, 1));
	for (int i = 0; i < 130; i++) {
		ASSERT_EQ(1, chromaprint_feed(ctx, zeroes, 1024));
	}

	uint32_t *fp;
	int length;

	ASSERT_EQ(1, chromaprint_finish(ctx));
	ASSERT_EQ(1, chromaprint_get_raw_fingerprint(ctx, &fp, &length));
	SCOPE_EXIT(chromaprint_dealloc(fp));

	ASSERT_EQ(3, length);
	EXPECT_EQ(627964279, fp[0]);
	EXPECT_EQ(627964279, fp[1]);
	EXPECT_EQ(627964279, fp[2]);
}

TEST(API, TestEncodeFingerprint)
{
	uint32_t fingerprint[] = { 1, 0 };
	char expected[] = { 55, 0, 0, 2, 65, 0 };

	char *encoded;
	int encoded_size;
	ASSERT_EQ(1, chromaprint_encode_fingerprint(fingerprint, 2, 55, &encoded, &encoded_size, 0));
	SCOPE_EXIT(chromaprint_dealloc(encoded));

	ASSERT_EQ(6, encoded_size);
	for (int i = 0; i < encoded_size; i++) {
		ASSERT_EQ(expected[i], encoded[i]) << "Different at " << i;
	}
}

TEST(API, TestEncodeFingerprintBase64)
{
	uint32_t fingerprint[] = { 1, 0 };
	char expected[] = "NwAAAkEA";

	char *encoded;
	int encoded_size;
	ASSERT_EQ(1, chromaprint_encode_fingerprint(fingerprint, 2, 55, &encoded, &encoded_size, 1));
	SCOPE_EXIT(chromaprint_dealloc(encoded));

	ASSERT_EQ(8, encoded_size);
	ASSERT_STREQ(expected, encoded);
}

TEST(API, TestDecodeFingerprintBinary)
{
	char data[] = { 55, 0, 0, 2, 65, 0 };

	uint32_t *fingerprint;
	int size;
	int algorithm;
	ASSERT_EQ(1, chromaprint_decode_fingerprint(data, 6, &fingerprint, &size, &algorithm, 0));
	SCOPE_EXIT(chromaprint_dealloc(fingerprint));

	ASSERT_EQ(2, size);
	ASSERT_EQ(55, algorithm);
	ASSERT_EQ(1, fingerprint[0]);
	ASSERT_EQ(0, fingerprint[1]);
}

TEST(API, TestDecodeFingerprintText)
{
	std::string data = "AQAAEwkjrUmSJQpUHflR9mjSJMdZpcO_Imdw9dCO9Clu4_wQPvhCB01w6xAtXNcAp5RASgDBhDSCGGIAcwA";

	uint32_t *fingerprint;
	int size;
	int algorithm;
	ASSERT_EQ(1, chromaprint_decode_fingerprint(data.c_str(), data.size(), &fingerprint, &size, &algorithm, 1));
	SCOPE_EXIT(chromaprint_dealloc(fingerprint));

	ASSERT_EQ(19, size);
	ASSERT_EQ(1, algorithm);
	ASSERT_EQ(-587455133, fingerprint[0]);
	ASSERT_EQ(-591649759, fingerprint[1]);
}

TEST(API, TestDecodeFingerprintHeaderBinary)
{
	char data[] = { 55, 0, 0, 2, 65, 0 };

	int size;
	int algorithm;
	ASSERT_EQ(1, chromaprint_decode_fingerprint_header(data, 6, &size, &algorithm, 0));

	ASSERT_EQ(2, size);
	ASSERT_EQ(55, algorithm);
}

TEST(API, TestDecodeFingerprintHeaderText)
{
	std::string data = "AQAAEwkjrUmSJQpUHflR9mjSJMdZpcO_Imdw9dCO9Clu4_wQPvhCB01w6xAtXNcAp5RASgDBhDSCGGIAcwA";

	int size;
	int algorithm;
	ASSERT_EQ(1, chromaprint_decode_fingerprint_header(data.c_str(), data.size(), &size, &algorithm, 1));

	ASSERT_EQ(19, size);
	ASSERT_EQ(1, algorithm);
}


TEST(API, TestHashFingerprint)
{
	uint32_t fingerprint[] = { 19681, 22345, 312312, 453425 };
	uint32_t hash;

	ASSERT_EQ(0, chromaprint_hash_fingerprint(NULL, 4, &hash));
	ASSERT_EQ(0, chromaprint_hash_fingerprint(fingerprint, -1, &hash));
	ASSERT_EQ(0, chromaprint_hash_fingerprint(fingerprint, 4, NULL));

	ASSERT_EQ(1, chromaprint_hash_fingerprint(fingerprint, 4, &hash));
	ASSERT_EQ(17249, hash);
}

TEST(API, TestDecodeFingerprintOutOfRange)
{
	uint32_t *fp;
	int length, algorithm;

	const char *encoded = "AQABz9GSJEkkKYmS4Tp6xvjx-tBpXMfRpUP1w3yGHmUPcWTwhnhgiYNeeBcu5HhbxA2PZuEFnSd-9FECq-gJPbtQ_fiLfkf3wvnRl9AO__jxCj7-QjzyoFek4xcmPnA2-qjUDz9yTsR56PFRJp4GP8ePLj2qY-NRfkL2oXEx6cFThPCh7_iRVnzQbX9wHc0UT0I_5Duao1KVGOsPlpdQLcqFHxf5oqfRFVpySvjR_Ch7_NCO87h05ChL7Do65vDIFC2H8zhyKnhqXJ6GdsuLw4c276h-TGEcfAfjmWgeYzy6HT6u5UhvhCceZSp0PD5iZzka9Zh05NOEZmsgxZTx50ifa8hxpRv0E1vHKciFHk0cFY0moRKzabgbDb3gH_2RH8k3lGuEKc_x3_hRPehX5ODRfDX6JCtOIVyOMFyP-4XyHWkaF9fRt0VzD7sOJlUkHX2Q9Ed-9PnR8EpwijfCJJFi9PmhLySPhmOC-sIZiI8m_ML3INJ3pI6UknCNnkHzyNDEpchz400d9IN99PiSo9kyDf0xXTrS_HCC79D64Tx4o1YSIReeCKH84Q0ORT7CNMKPmQma62inD7xoodlEJNOJXEHzo3qCMEn4omWg_UPKqBN69EYz5kPyDrkeCb2OKd2i4RJ6PB3CQ5Wo5shTMUH7Hf6IHsd5NNcQnjFGo8oP__DJQz8OJk9s5BROotERSmSOjyaUv0LqsziV4zmaK5jIFLXAxEk0xIfeIcyFqiEVXL_hGLnQUOXRJ8SHSLnRr_jx5IU35CH0N0gfHtdQ_Wj0ZniT7ENZXPDRrijZIMxn7Dkqo0nIoG_QVDsO_mi6yOibDJqyHf6DSiX2fdAyVEyCXDyqhD2aRyf2H09EMJkchE-g60GYK3grNHHwHE9I9DrSMoJ2ZMd7_Ch7WEftI24DqTrCEteio9ePLxIVIj-uW1CjK6goVoYnhYZuuElwNDfyIT4e4lGWKWhEyUF5XKic47xQ0ZygO0XXhAgTMzP-FN_x4_lQiynCxBlxPgivIEyNL0e64zu-hCR-fMzBieLxIzy0MEZ-MNcc-Bqc53gYUfCPh0c6MmFI6DWaijk-K8g_PNXRhKjTD72UBSXzgT_-YIx27EsUJ6iThkJTZUd-aM_R03iaCk-CUwITPUcPVVfwLA8m0UbTrLgSHR3-Y8-DUsKVB6EkJWh-_HgPTv3AZs_R23CcBfWRM1B1hB1K63iipLg9XETcZ0I3jynO8PiF64YtH1klhDvxhThXzFex3kG1HPZu9Dmu47mh5UGtHKp1POjyUMWfDJo6of8Gtgvy5EDfo1bgnWiX6wCGhEEAKADJIUAbILJzTAgDBDBCAgQQMEQQJABBwgJgBBMAAGaEAMwAh5wAikDkhQBGCIGkAgQASghihAmlEBDEMCOUAYYJ4pRVwhAEAiXEYWAUMchQBpQVhkwhgDUMAQAoMkgxJRUwjgFBBRBCEGM0BgBAAIhQRBJkkALCCAUMQIAIIQBQRDGDBAECAAGY4MYQIhSgBCEkKAHEggG8cAIYJIgUwJAHpCHAMAaEQEYo6IgAADgEAUIIBAQcMEIgYBkECEABCEYKAYYcA4oowBBABAAvBJJKMGIAEAYwA5QADAAGCINMaAKQJcQYjCCzxBBACAcGMM4II8IQKIghgFgABTOEUAegMQQ4wYCBSjgAiAFCEGCAYwZIIAA0QChAhBROUAQAEo4YR4AQhhhliAAMEAQUBtooAg5xjCADgCGAOCQIQgQghigwQAikBBAEAA";

	chromaprint_decode_fingerprint(encoded, strlen(encoded), &fp, &length, &algorithm, 1);
}

TEST(API, TestDecodeFingerprintEmpty)
{
	uint32_t *fp;
	int length, algorithm;

	const char *encoded = "AQAAAA";

	auto ret = chromaprint_decode_fingerprint(encoded, strlen(encoded), &fp, &length, &algorithm, 1);
	ASSERT_EQ(1, ret);
	ASSERT_EQ(0, length);
	ASSERT_EQ(1, algorithm);
}

TEST(API, TestDecodeFingerprintNull)
{
	uint32_t *fp;
	int length, algorithm;

	const char *encoded = "null";

	auto ret = chromaprint_decode_fingerprint(encoded, strlen(encoded), &fp, &length, &algorithm, 1);
	ASSERT_EQ(0, ret);
	ASSERT_EQ(0, length);
	ASSERT_EQ(0, algorithm);
}

TEST(API, TestDecodeFingerprintCoustidFingerprint)
{
	uint32_t *fp;
	int length, algorithm;

	const char *encoded = "coustid Fingerprint";

	auto ret = chromaprint_decode_fingerprint(encoded, strlen(encoded), &fp, &length, &algorithm, 1);
	ASSERT_EQ(0, ret);
	ASSERT_EQ(0, length);
	ASSERT_EQ(0, algorithm);
}

TEST(API, TestEncodeFingerprintInvalidSize)
{
	uint32_t fingerprint[] = { 1, 0 };
	char *encoded;
	int encoded_size;

	ASSERT_EQ(0, chromaprint_encode_fingerprint(fingerprint, -1, 1, &encoded, &encoded_size, 0));
	// The header has room for a 24-bit size only.
	ASSERT_EQ(0, chromaprint_encode_fingerprint(fingerprint, 0x1000000, 1, &encoded, &encoded_size, 0));
	ASSERT_EQ(0, chromaprint_encode_fingerprint(NULL, 2, 1, &encoded, &encoded_size, 0));
}

TEST(API, TestDecodeFingerprintInvalidSize)
{
	uint32_t *fp;
	int length, algorithm;
	char data[] = { 0, 0, 0, 2, 65, 0 };

	ASSERT_EQ(0, chromaprint_decode_fingerprint(data, -1, &fp, &length, &algorithm, 0));
	ASSERT_EQ(0, length);
	ASSERT_EQ(0, chromaprint_decode_fingerprint(NULL, 6, &fp, &length, &algorithm, 0));
	ASSERT_EQ(0, length);

	ASSERT_EQ(0, chromaprint_decode_fingerprint_header(data, -1, &length, &algorithm, 0));
	ASSERT_EQ(0, length);
	ASSERT_EQ(0, chromaprint_decode_fingerprint_header(NULL, 6, &length, &algorithm, 0));
	ASSERT_EQ(0, length);
}

TEST(API, TestDecodeFingerprintBitPastPosition32)
{
	uint32_t *fp;
	int length, algorithm;
	// Six distances of 6 reach bit 36.
	char data[] = { 0, 0, 0, 1, char(182), 109, 3 };

	ASSERT_EQ(0, chromaprint_decode_fingerprint(data, NELEMS(data), &fp, &length, &algorithm, 0));
	ASSERT_EQ(nullptr, fp);
	ASSERT_EQ(0, length);
}

namespace {

// Run the whole fingerprinting pipeline on `data`, feeding it in the given
// chunk sizes (in int16 samples, not frames). Returns the raw fingerprint.
std::vector<uint32_t> FingerprintFromChunks(const std::vector<short> &data,
                                            int algorithm, int num_channels,
                                            const std::vector<size_t> &chunks)
{
	ChromaprintContext *ctx = chromaprint_new(algorithm);
	EXPECT_NE(nullptr, ctx);
	if (!ctx) {
		return std::vector<uint32_t>();
	}
	SCOPE_EXIT(chromaprint_free(ctx));

	EXPECT_EQ(1, chromaprint_start(ctx, 44100, num_channels));
	size_t offset = 0;
	for (size_t i = 0; offset < data.size(); i++) {
		size_t n = std::min(chunks[i], data.size() - offset);
		EXPECT_EQ(1, chromaprint_feed(ctx, data.data() + offset, static_cast<int>(n)));
		offset += n;
	}
	EXPECT_EQ(1, chromaprint_finish(ctx));

	uint32_t *raw = nullptr;
	int size = 0;
	EXPECT_EQ(1, chromaprint_get_raw_fingerprint(ctx, &raw, &size));
	SCOPE_EXIT(chromaprint_dealloc(raw));
	return std::vector<uint32_t>(raw, raw + size);
}

// The four required feeding patterns: a single whole feed, seven chunks,
// a one-sample sliding feed, and fixed prime-sized chunks. Prime sizes are
// deliberately coprime with the channel count so every call after the first
// leaves a partial frame that has to be cached and re-used next time.
std::vector<std::vector<size_t> > ChunkingSchemes(size_t length)
{
	const size_t primes[] = { 2, 3, 5, 7, 101, 997, 7919 };
	std::vector<std::vector<size_t> > schemes;
	schemes.push_back(std::vector<size_t>(1, length));
	schemes.emplace_back(7, length / 7);
	schemes.back()[6] += length % 7;
	schemes.emplace_back(length, static_cast<size_t>(1));
	for (size_t prime : primes) {
		schemes.emplace_back((length + prime - 1) / prime, prime);
	}
	return schemes;
}

} // namespace

TEST(API, ChunkedFeedDoesNotChangeFingerprint)
{
	std::vector<short> stereo = LoadAudioFile("data/test_stereo_44100.raw");

	// The 2 s fixture is too short to yield a fingerprint for every algorithm
	// (the classifier needs at least 16 FFT frames); repeat it so each stream
	// is long enough, including the true stereo downmix path where odd chunk
	// sizes split frames in the middle and exercise the leftover cache.
	std::vector<short> stream;
	stream.reserve(stereo.size() * 4);
	for (int repeat = 0; repeat < 4; repeat++) {
		stream.insert(stream.end(), stereo.begin(), stereo.end());
	}

	struct Case {
		const char *name;
		int num_channels;
	};
	const Case cases[] = {
		{ "mono interpretation", 1 },
		{ "stereo downmix", 2 },
	};

	for (const Case &c : cases) {
		for (int algorithm = CHROMAPRINT_ALGORITHM_TEST1;
		     algorithm <= CHROMAPRINT_ALGORITHM_TEST5; algorithm++) {
			const std::string trace = std::string(c.name) + " algorithm " +
			                          std::to_string(algorithm);
			SCOPED_TRACE(trace);

			const auto schemes = ChunkingSchemes(stream.size());
			const std::vector<uint32_t> reference =
				FingerprintFromChunks(stream, algorithm, c.num_channels, schemes[0]);
			ASSERT_FALSE(reference.empty());

			// Whole feed, seven chunks, 1-sample step and every prime scheme
			// must produce a byte-identical, item-by-item fingerprint.
			for (size_t i = 1; i < schemes.size(); i++) {
				SCOPED_TRACE(std::string("scheme ") + std::to_string(i));
				const std::vector<uint32_t> actual =
					FingerprintFromChunks(stream, algorithm, c.num_channels, schemes[i]);
				ASSERT_EQ(reference.size(), actual.size());
				for (size_t j = 0; j < reference.size(); j++) {
					EXPECT_EQ(reference[j], actual[j])
						<< "Fingerprints differ at item " << j;
				}
			}
		}
	}
}

}; // namespace chromaprint
