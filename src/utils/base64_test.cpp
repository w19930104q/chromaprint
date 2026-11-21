#include <gtest/gtest.h>
#include "base64.h"
#include "test_utils.h"

using namespace chromaprint;

TEST(Base64, Base64Encode)
{
	ASSERT_EQ("eA", Base64Encode("x"));
	ASSERT_EQ("eHg", Base64Encode("xx"));
	ASSERT_EQ("eHh4", Base64Encode("xxx"));
	ASSERT_EQ("eHh4eA", Base64Encode("xxxx"));
	ASSERT_EQ("eHh4eHg", Base64Encode("xxxxx"));
	ASSERT_EQ("eHh4eHh4", Base64Encode("xxxxxx"));
	ASSERT_EQ("_-4", Base64Encode("\xff\xee"));
}

TEST(Base64, Base64Decode)
{
	ASSERT_EQ("x", Base64Decode("eA"));
	ASSERT_EQ("xx", Base64Decode("eHg"));
	ASSERT_EQ("xxx", Base64Decode("eHh4"));
	ASSERT_EQ("xxxx", Base64Decode("eHh4eA"));
	ASSERT_EQ("xxxxx", Base64Decode("eHh4eHg"));
	ASSERT_EQ("xxxxxx", Base64Decode("eHh4eHh4"));
	ASSERT_EQ("\xff\xee", Base64Decode("_-4"));
}

TEST(Base64, Base64DecodeInvalid)
{
	// A single dangling symbol (length % 4 == 1) does not encode a whole
	// byte. It must be rejected rather than silently dropped, which previously
	// made invalid fingerprints decode as different (shorter) data.
	ASSERT_TRUE(IsValidBase64EncodedSize(0));
	ASSERT_TRUE(IsValidBase64EncodedSize(2));
	ASSERT_TRUE(IsValidBase64EncodedSize(3));
	ASSERT_TRUE(IsValidBase64EncodedSize(4));
	ASSERT_FALSE(IsValidBase64EncodedSize(1));
	ASSERT_FALSE(IsValidBase64EncodedSize(5));
	ASSERT_FALSE(IsValidBase64EncodedSize(9));

	ASSERT_FALSE(IsValidBase64(std::string("w")));
	ASSERT_FALSE(IsValidBase64(std::string("AQAAA")));
	ASSERT_FALSE(IsValidBase64(std::string("AQABx")));

	// Characters outside the URL-safe alphabet (including standard padding)
	// are invalid.
	ASSERT_FALSE(IsValidBase64(std::string("eA==")));
	ASSERT_FALSE(IsValidBase64(std::string("eH+4")));
	ASSERT_FALSE(IsValidBase64(std::string("eH/4")));
	ASSERT_FALSE(IsValidBase64(std::string("eA!")));
	ASSERT_TRUE(IsValidBase64(std::string("_-4")));

	// Invalid input decodes to an empty string instead of truncated data.
	ASSERT_EQ("", Base64Decode("w"));
	ASSERT_EQ("", Base64Decode("AQAAA"));
	ASSERT_EQ("", Base64Decode("eA=="));
}

TEST(Base64, Base64RoundTrip)
{
	// Bit-exact encode -> decode round trip for every input length, including
	// the 1- and 2-byte remainder cases and all byte values.
	std::string data;
	for (int value = 0; value < 256; value++) {
		data.push_back(static_cast<char>(value));
	}
	for (size_t len = 0; len <= data.size(); len++) {
		const std::string original = data.substr(0, len);
		const std::string encoded = Base64Encode(original);
		// Every string produced by the encoder must be self-validating.
		ASSERT_TRUE(IsValidBase64(encoded)) << "len=" << len;
		ASSERT_EQ(original, Base64Decode(encoded)) << "len=" << len;
	}
}

TEST(Base64, Base64EncodeLong)
{
	int data[] = {
		1, 0, 1, 207, 17, 181, 36, 18, 19, 37, 65, 15, 31, 197, 149, 161, 63, 33, 22,
		60, 141, 27, 202, 35, 184, 47, 254, 227, 135, 135, 11, 58, 139, 208, 65, 127,
		52, 167, 241, 31, 99, 182, 25, 159, 96, 70, 71, 160, 251, 168, 75, 132, 185,
		112, 230, 193, 133, 252, 42, 126, 66, 91, 121, 60, 135, 79, 24, 185, 210, 28,
		199, 133, 255, 240, 113, 101, 67, 199, 23, 225, 181, 160, 121, 140, 67, 123,
		161, 229, 184, 137, 30, 205, 135, 119, 70, 94, 252, 71, 120, 150
	};
	std::string original(data, data + NELEMS(data));

	char encoded[] = "AQABzxG1JBITJUEPH8WVoT8hFjyNG8ojuC_-44eHCzqL0EF_NKfxH2O2GZ9gRkeg-6hLhLlw5sGF_Cp-Qlt5PIdPGLnSHMeF__BxZUPHF-G1oHmMQ3uh5biJHs2Hd0Ze_Ed4lg";
	ASSERT_EQ(encoded, Base64Encode(original));
}

TEST(Base64, Base64DecodeLong)
{
	int data[] = {
		1, 0, 1, 207, 17, 181, 36, 18, 19, 37, 65, 15, 31, 197, 149, 161, 63, 33, 22,
		60, 141, 27, 202, 35, 184, 47, 254, 227, 135, 135, 11, 58, 139, 208, 65, 127,
		52, 167, 241, 31, 99, 182, 25, 159, 96, 70, 71, 160, 251, 168, 75, 132, 185,
		112, 230, 193, 133, 252, 42, 126, 66, 91, 121, 60, 135, 79, 24, 185, 210, 28,
		199, 133, 255, 240, 113, 101, 67, 199, 23, 225, 181, 160, 121, 140, 67, 123,
		161, 229, 184, 137, 30, 205, 135, 119, 70, 94, 252, 71, 120, 150
	};
	std::string original(data, data + NELEMS(data));

	char encoded[] = "AQABzxG1JBITJUEPH8WVoT8hFjyNG8ojuC_-44eHCzqL0EF_NKfxH2O2GZ9gRkeg-6hLhLlw5sGF_Cp-Qlt5PIdPGLnSHMeF__BxZUPHF-G1oHmMQ3uh5biJHs2Hd0Ze_Ed4lg";
	ASSERT_EQ(original, Base64Decode(encoded));
}
