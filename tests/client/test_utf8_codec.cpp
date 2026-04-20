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
    // "Привет" — 6 Cyrillic letters, each 1 QChar (UTF-16) / 2 bytes (UTF-8).
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
    // H i ,   П р и в е т !
    // positions (UTF-16): 0 1 2 3 4 5 6 7 8 9 10
    // UTF-8 bytes:        1 1 1 1 2 2 2 2 2 2 1
    EXPECT_EQ(utf16_to_utf8_offset(s, 4), 4u);   // after "Hi, "
    EXPECT_EQ(utf16_to_utf8_offset(s, 10), 16u); // before '!'
    EXPECT_EQ(utf16_to_utf8_offset(s, 11), 17u); // after '!'
}

TEST(Utf8Codec, EmojiSurrogatePair) {
    // U+1F600 (grinning face) = 4 UTF-8 bytes, 2 UTF-16 code units.
    QString s = QString::fromUtf8("a😀b");
    EXPECT_EQ(s.size(), 4);
    EXPECT_EQ(utf16_to_utf8_offset(s, 0), 0u);
    EXPECT_EQ(utf16_to_utf8_offset(s, 1), 1u);       // after 'a'
    EXPECT_EQ(utf16_to_utf8_offset(s, 3), 5u);       // after emoji
    EXPECT_EQ(utf16_to_utf8_offset(s, 4), 6u);       // after 'b'

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
