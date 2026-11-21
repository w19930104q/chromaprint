#include <gtest/gtest.h>
#include <algorithm>
#include <vector>
#include <string>
#include <fstream>
#include "test_utils.h"
#include "audio_processor.h"
#include "audio_buffer.h"
#include "utils.h"

using namespace chromaprint;

TEST(AudioProcessor, Accessors)
{
	std::vector<short> data = LoadAudioFile("data/test_mono_44100.raw");

	AudioBuffer buffer;
	AudioBuffer buffer2;
	AudioProcessor processor(44100, &buffer);

	EXPECT_EQ(44100, processor.target_sample_rate());
	EXPECT_EQ(&buffer, processor.consumer());

	processor.set_target_sample_rate(11025);
	EXPECT_EQ(11025, processor.target_sample_rate());

	processor.set_consumer(&buffer2);
	EXPECT_EQ(&buffer2, processor.consumer());
}

TEST(AudioProcessor, PassThrough)
{
	std::vector<short> data = LoadAudioFile("data/test_mono_44100.raw");

	AudioBuffer buffer;
	AudioProcessor processor(44100, &buffer);
	processor.Reset(44100, 1);
	processor.Consume(data.data(), data.size());
	processor.Flush();

	ASSERT_EQ(data.size(), buffer.data().size());
	for (size_t i = 0; i < data.size(); i++) {
		ASSERT_EQ(data[i], buffer.data()[i]) << "Signals differ at index " << i;
	}
}

TEST(AudioProcessor, StereoToMono)
{
	std::vector<short> data1 = LoadAudioFile("data/test_stereo_44100.raw");
	std::vector<short> data2 = LoadAudioFile("data/test_mono_44100.raw");

	AudioBuffer buffer;
	AudioProcessor processor(44100, &buffer);
	processor.Reset(44100, 2);
	processor.Consume(data1.data(), data1.size());
	processor.Flush();

	ASSERT_EQ(data2.size(), buffer.data().size());
	for (size_t i = 0; i < data2.size(); i++) {
		ASSERT_EQ(data2[i], buffer.data()[i]) << "Signals differ at index " << i;
	}
}

TEST(AudioProcessor, ResampleMono)
{
	std::vector<short> data1 = LoadAudioFile("data/test_mono_44100.raw");
	std::vector<short> data2 = LoadAudioFile("data/test_mono_11025.raw");

	AudioBuffer buffer;
	AudioProcessor processor(11025, &buffer);
	processor.Reset(44100, 1);
	processor.Consume(data1.data(), data1.size());
	processor.Flush();

	ASSERT_EQ(data2.size(), buffer.data().size());
	for (size_t i = 0; i < data2.size(); i++) {
		ASSERT_EQ(data2[i], buffer.data()[i]) << "Signals differ at index " << i;
	}
}

TEST(AudioProcessor, ResampleMonoNonInteger)
{
	std::vector<short> data1 = LoadAudioFile("data/test_mono_44100.raw");
	std::vector<short> data2 = LoadAudioFile("data/test_mono_8000.raw");

	AudioBuffer buffer;
	AudioProcessor processor(8000, &buffer);
	processor.Reset(44100, 1);
	processor.Consume(data1.data(), data1.size());
	processor.Flush();

	ASSERT_EQ(data2.size(), buffer.data().size());
	for (size_t i = 0; i < data2.size(); i++) {
		ASSERT_NEAR(data2[i], buffer.data()[i], 3) << "Signals differ at index " << i;
	}
}

TEST(AudioProcessor, StereoToMonoAndResample)
{
	std::vector<short> data1 = LoadAudioFile("data/test_stereo_44100.raw");
	std::vector<short> data2 = LoadAudioFile("data/test_mono_11025.raw");

	AudioBuffer buffer;
	AudioProcessor processor(11025, &buffer);
	processor.Reset(44100, 2);
	processor.Consume(data1.data(), data1.size());
	processor.Flush();

	ASSERT_EQ(data2.size(), buffer.data().size());
	for (size_t i = 0; i < data2.size(); i++) {
		ASSERT_EQ(data2[i], buffer.data()[i]) << "Signals differ at index " << i;
	}
}

TEST(AudioProcessor, ChunkedConsumeMatchesSingleConsume)
{
	std::vector<short> data = LoadAudioFile("data/test_stereo_44100.raw");

	AudioBuffer expected;
	AudioProcessor reference(11025, &expected);
	reference.Reset(44100, 2);
	reference.Consume(data.data(), data.size());
	reference.Flush();

	// Some of these are multiples of the channel count and some are not. The
	// ones that are not leave a partial frame to be carried into the next
	// call; the rest are there as a control. However the caller splits its
	// input, the result must be the same.
	const size_t chunk_sizes[] = { 1, 2, 3, 5, 7, 17, 511, 997, 4096 };

	for (size_t c = 0; c < NELEMS(chunk_sizes); c++) {
		const size_t chunk_size = chunk_sizes[c];
		SCOPED_TRACE("chunk size " + std::to_string(chunk_size));

		AudioBuffer buffer;
		AudioProcessor processor(11025, &buffer);
		processor.Reset(44100, 2);
		for (size_t offset = 0; offset < data.size(); offset += chunk_size) {
			processor.Consume(data.data() + offset,
			                  std::min(chunk_size, data.size() - offset));
		}
		processor.Flush();

		ASSERT_EQ(expected.data().size(), buffer.data().size());
		for (size_t i = 0; i < expected.data().size(); i++) {
			ASSERT_EQ(expected.data()[i], buffer.data()[i]) << "Signals differ at index " << i;
		}
	}
}

namespace {

// Counts every mono sample handed downstream, so a test can assert that no
// input sample is silently dropped regardless of how the caller chunks it.
class CountingConsumer : public AudioConsumer
{
public:
	void Consume(const int16_t *input, int length) override
	{
		(void)input;
		m_total += length;
		m_calls++;
	}

	long long total() const { return m_total; }
	int calls() const { return m_calls; }

private:
	long long m_total = 0;
	int m_calls = 0;
};

long long ConsumeInChunks(const std::vector<short> &data, int num_channels,
                          int target_sample_rate,
                          const std::vector<size_t> &chunk_sizes)
{
	CountingConsumer consumer;
	AudioProcessor processor(target_sample_rate, &consumer);
	processor.Reset(44100, num_channels);
	size_t offset = 0;
	for (size_t i = 0; offset < data.size(); i++) {
		size_t n = std::min(chunk_sizes[i], data.size() - offset);
		processor.Consume(data.data() + offset, static_cast<int>(n));
		offset += n;
	}
	processor.Flush();
	return consumer.total();
}

// The same feeding patterns required for the public fingerprinting API:
// one whole feed, seven chunks, a 1-sample feed and prime-sized feeds.
std::vector<std::vector<size_t> > ProcessorChunkingSchemes(size_t length)
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

TEST(AudioProcessor, ChunkedConsumeConservesSamples)
{
	std::vector<short> data = LoadAudioFile("data/test_stereo_44100.raw");
	const int num_channels = 2;

	// Without resampling, every complete input frame becomes one mono sample.
	// The whole-feed result defines the expected count; every other split
	// must deliver exactly the same number of samples.
	const auto schemes = ProcessorChunkingSchemes(data.size());
	const long long expected_passthrough =
		ConsumeInChunks(data, num_channels, 44100, schemes[0]);
	ASSERT_EQ(static_cast<long long>(data.size() / num_channels),
	          expected_passthrough);

	for (size_t i = 1; i < schemes.size(); i++) {
		SCOPED_TRACE("passthrough scheme " + std::to_string(i));
		EXPECT_EQ(expected_passthrough,
		          ConsumeInChunks(data, num_channels, 44100, schemes[i]));
	}

	// The resampling path keeps its own internal history, which makes it even
	// easier to lose or duplicate a sample on a non-aligned boundary. The
	// count must still be independent of the chunking.
	const long long expected_resampled =
		ConsumeInChunks(data, num_channels, 11025, schemes[0]);
	EXPECT_GT(expected_resampled, 0);
	for (size_t i = 1; i < schemes.size(); i++) {
		SCOPED_TRACE("resample scheme " + std::to_string(i));
		EXPECT_EQ(expected_resampled,
		          ConsumeInChunks(data, num_channels, 11025, schemes[i]));
	}
}

TEST(AudioProcessor, ChunkedConsumeConservesSamplesWithTrailingPartialFrame)
{
	std::vector<short> full = LoadAudioFile("data/test_stereo_44100.raw");
	// An odd length means the last call can never complete a stereo frame;
	// that single dangling sample is the only input that may stay unconsumed,
	// and it must not corrupt or swallow any frame before it.
	std::vector<short> data(full.begin(), full.end() - 1);
	ASSERT_EQ(1u, data.size() % 2);

	const auto schemes = ProcessorChunkingSchemes(data.size());
	const long long expected = static_cast<long long>(data.size() / 2);
	for (size_t i = 0; i < schemes.size(); i++) {
		SCOPED_TRACE("scheme " + std::to_string(i));
		EXPECT_EQ(expected, ConsumeInChunks(data, 2, 44100, schemes[i]));
	}
}
