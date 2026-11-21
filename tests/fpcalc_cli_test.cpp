// Copyright (C) 2016  Lukas Lalinsky
// Distributed under the MIT license, see the LICENSE file for details.

// End-to-end regression tests for the fpcalc command-line tool. These exercise
// the audio reading / limiting loop in src/cmd/fpcalc.cpp by running the real
// binary, so they are only built when BUILD_TOOLS is enabled.

#include <gtest/gtest.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sstream>
#ifndef _WIN32
#include <sys/wait.h>
#endif

#ifndef FPCALC_PATH
#error "FPCALC_PATH must be defined as the path to the fpcalc executable"
#endif

#ifndef TESTS_DIR
#error "TESTS_DIR must be defined"
#endif

namespace chromaprint {
namespace {

struct CliResult {
	int exit_code;
	std::string output;
};

// Run fpcalc with the given arguments and capture stdout plus the exit code.
// stderr is redirected to stdout so failures are visible in the captured text.
CliResult RunFpcalc(const std::string &args) {
	const std::string command =
		std::string(FPCALC_PATH) + " " + args + " 2>&1";
	FILE *pipe = popen((command + "; echo \"rc=$?\"").c_str(), "r");
	EXPECT_NE(nullptr, pipe);
	CliResult result{ -1, "" };
	if (!pipe) {
		return result;
	}
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
		result.output += buffer;
	}
	const int status = pclose(pipe);
#ifdef _WIN32
	result.exit_code = status;
#else
	result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
	// Strip the trailing "rc=N" marker and remember the reported code.
	const auto marker = result.output.rfind("\nrc=");
	if (marker != std::string::npos) {
		result.exit_code = atoi(result.output.c_str() + marker + 4);
		result.output.erase(marker);
	}
	return result;
}

bool Contains(const std::string &haystack, const std::string &needle) {
	return haystack.find(needle) != std::string::npos;
}

const char *kTenSecondsRaw = TESTS_DIR "data/fpcalc_10s_44100_mono.raw";
const char *kShortRaw = TESTS_DIR "data/fpcalc_500samples_8000_mono.raw";
const char *kEmptyRaw = TESTS_DIR "data/fpcalc_empty.raw";

const char *kRawArgs = "-f s16le -rate 44100 -channels 1";
const char *kShortRawArgs = "-f s16le -rate 8000 -channels 1";

// The -length limit must actually restrict the processed audio and the
// reported duration must match the requested value, not the full file length.
TEST(FpcalcCliTest, LengthLimitRestrictsDuration) {
	const auto baseline = RunFpcalc(std::string("-json ") + kRawArgs + " " + kTenSecondsRaw);
	ASSERT_EQ(0, baseline.exit_code) << baseline.output;
	EXPECT_TRUE(Contains(baseline.output, "\"duration\": 10.00")) << baseline.output;

	const auto limited = RunFpcalc(
		std::string("-json -length 3 ") + kRawArgs + " " + kTenSecondsRaw);
	ASSERT_EQ(0, limited.exit_code) << limited.output;
	EXPECT_TRUE(Contains(limited.output, "\"duration\": 3.00")) << limited.output;
	// A 3 second fingerprint must be shorter than the full 10 second one.
	EXPECT_LT(limited.output.size(), baseline.output.size());
}

// A fractional limit is honoured and reported exactly.
TEST(FpcalcCliTest, FractionalLengthLimit) {
	const auto limited = RunFpcalc(
		std::string("-json -length 2.5 ") + kRawArgs + " " + kTenSecondsRaw);
	ASSERT_EQ(0, limited.exit_code) << limited.output;
	EXPECT_TRUE(Contains(limited.output, "\"duration\": 2.50")) << limited.output;
}

// A limit longer than the file reports the real (file) duration.
TEST(FpcalcCliTest, LengthLimitLongerThanFile) {
	const auto limited = RunFpcalc(
		std::string("-json -length 30 ") + kRawArgs + " " + kTenSecondsRaw);
	ASSERT_EQ(0, limited.exit_code) << limited.output;
	EXPECT_TRUE(Contains(limited.output, "\"duration\": 10.00")) << limited.output;
}

// A few hundred samples cannot produce a fingerprint, but fpcalc must not
// crash and must emit an empty fingerprint instead of failing fatally.
TEST(FpcalcCliTest, VeryShortAudioProducesEmptyFingerprint) {
	const auto result = RunFpcalc(std::string("-json ") + kShortRawArgs + " " + kShortRaw);
	ASSERT_EQ(0, result.exit_code) << result.output;
	EXPECT_TRUE(Contains(result.output, "\"fingerprint\": \"\"")) << result.output;
}

// An empty (zero-byte) stream is handled gracefully with an empty fingerprint.
TEST(FpcalcCliTest, EmptyAudioProducesEmptyFingerprint) {
	const auto result = RunFpcalc(std::string("-json ") + kShortRawArgs + " " + kEmptyRaw);
	ASSERT_EQ(0, result.exit_code) << result.output;
	EXPECT_TRUE(Contains(result.output, "\"duration\": 0.00")) << result.output;
	EXPECT_TRUE(Contains(result.output, "\"fingerprint\": \"\"")) << result.output;
}

// Text output for empty audio stays well-formed.
TEST(FpcalcCliTest, EmptyAudioTextOutput) {
	const auto result = RunFpcalc(std::string(kShortRawArgs) + " " + kEmptyRaw);
	ASSERT_EQ(0, result.exit_code) << result.output;
	EXPECT_TRUE(Contains(result.output, "DURATION=0")) << result.output;
	EXPECT_TRUE(Contains(result.output, "FINGERPRINT=")) << result.output;
}

}; // namespace
}; // namespace chromaprint
