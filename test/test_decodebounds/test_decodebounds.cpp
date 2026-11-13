#include <unity.h>
#include <initializer_list>
#include "../arduino_to_native.h"
#include "labeldef.h"
#include "converters.h"

void test_real_short_register_zero_excludes_crc_and_missing_mpu_fields()
{
    // ERGA06EVH, 13 November 2026: 10 payload bytes, then checksum A9. Offsets 10/11 are absent.
    unsigned char frame[] = {0x40,0x00,0x0C,0x04,0x01,0x00,0x01,0x01,0x01,0x00,0x00,0x01,0x01,0xA9};
    LabelDef labels[] = {{0,9,152,1,-1,"last valid"},{0,10,152,1,-1,"MPU xx"},{0,11,152,1,-1,"MPU yy"}};
    Converter c; c.quiet = true; c.setLabels(labels, 3);
    TEST_ASSERT_TRUE(c.readRegistryValues(frame, 'I', sizeof(frame)));
    TEST_ASSERT_TRUE(labels[0].valid);
    TEST_ASSERT_EQUAL_STRING("1", labels[0].asString);
    TEST_ASSERT_FALSE(labels[1].valid);
    TEST_ASSERT_FALSE(labels[2].valid);
    TEST_ASSERT_EQUAL_STRING("", labels[1].asString);
}

void test_fields_crossing_end_are_invalidated_and_recover()
{
    Converter c; c.quiet = true;
    LabelDef label(0x20, 0, 105, 2, 1, "coil");
    unsigned char bytes[] = {0x97,0xFF};
    TEST_ASSERT_TRUE(c.convertBounded(&label, bytes, 2));
    TEST_ASSERT_EQUAL_STRING("-10.5", label.asString);
    TEST_ASSERT_FALSE(c.convertBounded(&label, bytes, 1));
    TEST_ASSERT_FALSE(label.valid);
    TEST_ASSERT_EQUAL_STRING("", label.asString);
    TEST_ASSERT_TRUE(c.convertBounded(&label, bytes, 2));
    TEST_ASSERT_TRUE(label.valid);
}

void test_converter_minimum_span_overrides_bad_one_byte_definition()
{
    Converter c; c.quiet = true;
    unsigned char one[] = {0};
    for (int conv : {114,119,314})
    {
        LabelDef label(0x20, 0, conv, 1, 1, "bad definition");
        TEST_ASSERT_FALSE(c.convertBounded(&label, one, sizeof(one)));
    }
    LabelDef word(0x20, 0, 105, 0, 1, "zero declared numeric");
    TEST_ASSERT_FALSE(c.convertBounded(&word, one, sizeof(one)));
    LabelDef outside(0x20, -1, 152, 1, 1, "negative offset");
    TEST_ASSERT_FALSE(c.convertBounded(&outside, one, sizeof(one)));
}

void test_frame_validation_rejects_truncation_length_header_and_crc()
{
    Converter c; c.quiet = true;
    unsigned char frame[] = {0x40,0x20,0x04,0x97,0xFF,0x05};
    LabelDef label(0x20, 0, 105, 2, 1, "coil"); c.setLabels(&label, 1);
    TEST_ASSERT_TRUE(c.readRegistryValues(frame, 'I', sizeof(frame)));
    TEST_ASSERT_FALSE(c.readRegistryValues(frame, 'I', sizeof(frame) - 1));
    frame[5] ^= 1;
    TEST_ASSERT_FALSE(c.readRegistryValues(frame, 'I', sizeof(frame)));
    frame[2] = 0;
    TEST_ASSERT_FALSE(c.readRegistryValues(frame, 'I', sizeof(frame)));
    frame[0] = 0x15;
    TEST_ASSERT_FALSE(c.readRegistryValues(frame, 'I', sizeof(frame)));
}

void test_protocol_s_excludes_checksum()
{
    unsigned char frame[] = {0x56,0x01,0x02,0xA6};
    LabelDef labels[] = {{0x56,1,152,1,-1,"valid"},{0x56,2,152,1,-1,"past end"}};
    Converter c; c.quiet = true; c.setLabels(labels, 2);
    TEST_ASSERT_TRUE(c.readRegistryValues(frame, 'S', sizeof(frame)));
    TEST_ASSERT_TRUE(labels[0].valid);
    TEST_ASSERT_FALSE(labels[1].valid);
}

void test_string_field_needs_no_terminator_in_received_bytes()
{
    unsigned char bytes[] = {'A','B','C','D'};
    LabelDef label(0x20,0,100,4,-1,"text");
    Converter c; c.quiet = true;
    TEST_ASSERT_TRUE(c.convertBounded(&label, bytes, sizeof(bytes)));
    TEST_ASSERT_EQUAL_STRING("ABC", label.asString);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_real_short_register_zero_excludes_crc_and_missing_mpu_fields);
    RUN_TEST(test_fields_crossing_end_are_invalidated_and_recover);
    RUN_TEST(test_converter_minimum_span_overrides_bad_one_byte_definition);
    RUN_TEST(test_frame_validation_rejects_truncation_length_header_and_crc);
    RUN_TEST(test_protocol_s_excludes_checksum);
    RUN_TEST(test_string_field_needs_no_terminator_in_received_bytes);
    return UNITY_END();
}
