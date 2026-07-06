ADD_EXECUTABLE(test_tokenize test_tokenize.c)
TARGET_LINK_LIBRARIES(test_tokenize libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME tokenize COMMAND test_tokenize)
