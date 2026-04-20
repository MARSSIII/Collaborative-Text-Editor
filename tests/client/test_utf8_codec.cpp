#include "client/utf8_codec.h"

#include <gtest/gtest.h>
#include <QByteArray>
#include <QString>

using namespace collab_client::utf8_codec;

TEST(Utf8Codec, AsciiOffsetsMatch) {
    QString s = QStringLiteral("Hello");
    EXPECT_EQ(utf16_to_utf8_offset(s, 0), 0u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 5), 5u);
    EXPECT_EQ(utf8_to_utf16_offset(s.toUtf8(), 5), 5);
}

TEST(Utf8Codec, CyrillicEachCharIsTwoBytes) {
    QString s = QString::fromUtf8("Привет");
    EXPECT_EQ(s.size(), 6);
    EXPECT_EQ(utf16_to_utf8_offset(s, 0), 0u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 1), 2u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 3), 6u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 6), 12u);

    auto utf8 = s.toUtf8();
    EXPECT_EQ(utf8.size(), 12);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 0), 0);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 2), 1);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 6), 3);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 12), 6);
}

TEST(Utf8Codec, MixedAsciiAndCyrillic) {
    QString s = QString::fromUtf8("Hi, Привет!");
    EXPECT_EQ(utf16_to_utf8_offset(s, 4), 4u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 10), 16u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 11), 17u);
}

TEST(Utf8Codec, EmojiSurrogatePair) {
    QString s = QString::fromUtf8("a😀b");
    EXPECT_EQ(s.size(), 4);
    EXPECT_EQ(utf16_to_utf8_offset(s, 0), 0u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 1), 1u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 3), 5u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 4), 6u);

    auto utf8 = s.toUtf8();
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 1), 1);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 5), 3);
    EXPECT_EQ(utf8_to_utf16_offset(utf8, 6), 4);
}

TEST(Utf8Codec, EmptyString) {
    QString s;
    EXPECT_EQ(utf16_to_utf8_offset(s, 0), 0u);
    EXPECT_EQ(utf8_to_utf16_offset(QByteArray(), 0), 0);
}

TEST(Utf8Codec, OffsetBeyondLengthClamps) {
    QString s = QStringLiteral("abc");
    EXPECT_EQ(utf16_to_utf8_offset(s, 100), 3u);
    EXPECT_EQ(utf8_to_utf16_offset(s.toUtf8(), 100), 3);
}

TEST(Utf8Codec, NegativeUtf16OffsetTreatedAsZero) {
    QString s = QStringLiteral("abc");
    EXPECT_EQ(utf16_to_utf8_offset(s, -5), 0u);
}
