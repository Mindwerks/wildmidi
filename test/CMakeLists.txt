ADD_EXECUTABLE(test_tokenize test_tokenize.c)
TARGET_LINK_LIBRARIES(test_tokenize libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME tokenize COMMAND test_tokenize)

ADD_EXECUTABLE(test_smaf_sequ test_smaf_sequ.c)
TARGET_LINK_LIBRARIES(test_smaf_sequ libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME smaf_sequ COMMAND test_smaf_sequ)
