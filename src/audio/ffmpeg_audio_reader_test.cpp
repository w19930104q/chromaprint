#include "audio/ffmpeg_audio_reader.h"
#include <gtest/gtest.h>

#include <config.h>

namespace chromaprint {

TEST(FFmpegAudioReaderTest, ReadRaw) {
	FFmpegAudioReader reader;

	ASSERT_TRUE(reader.SetInputFormat("s16le"));
	ASSERT_TRUE(reader.SetInputChannels(2));
	ASSERT_TRUE(reader.SetInputSampleRate(44100));

	ASSERT_TRUE(reader.Open(TESTS_DIR "/data/test_stereo_44100.raw")) << reader.GetError();
	ASSERT_TRUE(reader.IsOpen());

	ASSERT_EQ(2, reader.GetChannels());
	ASSERT_EQ(44100, reader.GetSampleRate());

	const int16_t *data;
	size_t size;
	while (reader.Read(&data, &size)) {

	}

/*	ASSERT_TRUE(reader.ReadAll([&](const int16_t *data, size_t size) {

	}));*/
}

TEST(FFmpegAudioReaderTest, ReadChannelLayoutChange) {
	FFmpegAudioReader reader;

	reader.SetOutputChannels(2);
	reader.SetOutputSampleRate(11025);

	// 5.1 AC-3 stream with a few stereo frames in the middle
	ASSERT_TRUE(reader.Open(TESTS_DIR "/data/test_51_stereo_switch.ac3")) << reader.GetError();
	ASSERT_TRUE(reader.IsOpen());

	ASSERT_EQ(2, reader.GetChannels());
	ASSERT_EQ(11025, reader.GetSampleRate());

	const int16_t *data;
	size_t size;
	size_t total_size = 0;
	while (reader.Read(&data, &size)) {
		total_size += size;
	}
	ASSERT_TRUE(reader.IsFinished()) << reader.GetError();

	// 15 frames of 1536 samples at 44100 Hz
	ASSERT_NEAR(15 * 1536 * 11025 / 44100, total_size, 100);
}

TEST(FFmpegAudioReaderTest, ReadPlanarMoreThanEightChannels) {
	FFmpegAudioReader reader;

	// Planar 10-channel stream. AVFrame::data only holds 8 planes, so the
	// reader has to pass extended_data to the converter, otherwise channels
	// 8 and 9 come out as NULL pointers and the conversion crashes.
	ASSERT_TRUE(reader.Open(TESTS_DIR "/data/test_10ch_planar.nut")) << reader.GetError();
	ASSERT_TRUE(reader.IsOpen());

	ASSERT_EQ(10, reader.GetChannels());
	ASSERT_EQ(44100, reader.GetSampleRate());

	const int16_t *data;
	size_t size;
	size_t total_size = 0;
	// Per-channel count of nonzero samples, especially channels 8 and 9,
	// which are only reachable through AVFrame::extended_data.
	size_t nonzero[10] = {};
	while (reader.Read(&data, &size)) {
		for (size_t frame = 0; frame < size; ++frame) {
			for (int channel = 0; channel < 10; ++channel) {
				if (data[frame * 10 + channel] != 0) {
					++nonzero[channel];
				}
			}
		}
		total_size += size;
	}
	ASSERT_TRUE(reader.IsFinished()) << reader.GetError();

	// 0.1 s at 44100 Hz
	ASSERT_NEAR(4410, total_size, 10);

	// Every channel carries a distinct sine wave, so all of them -- including
	// planes 8 and 9 -- must contain a large number of nonzero samples.
	for (int channel = 0; channel < 10; ++channel) {
		EXPECT_GT(nonzero[channel], total_size / 2) << "channel " << channel;
	}
}

TEST(FFmpegAudioReaderTest, ReadPlanarMoreThanEightChannelsDownmix) {
	FFmpegAudioReader reader;

	// Same 10-channel planar stream, but converted through swresample to a
	// stereo output so the extended_data pointers really go through the
	// converter rather than a plain passthrough.
	reader.SetOutputChannels(2);
	reader.SetOutputSampleRate(11025);

	ASSERT_TRUE(reader.Open(TESTS_DIR "/data/test_10ch_planar.nut")) << reader.GetError();
	ASSERT_TRUE(reader.IsOpen());

	ASSERT_EQ(2, reader.GetChannels());
	ASSERT_EQ(11025, reader.GetSampleRate());

	const int16_t *data;
	size_t size;
	size_t total_size = 0;
	while (reader.Read(&data, &size)) {
		total_size += size;
	}
	ASSERT_TRUE(reader.IsFinished()) << reader.GetError();

	ASSERT_NEAR(4410 * 11025 / 44100, total_size, 10);
}

}; // namespace chromaprint
