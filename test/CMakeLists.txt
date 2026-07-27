ADD_EXECUTABLE(test_tokenize test_tokenize.c)
TARGET_LINK_LIBRARIES(test_tokenize libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME tokenize COMMAND test_tokenize)

ADD_EXECUTABLE(test_smaf_sequ test_smaf_sequ.c)
TARGET_LINK_LIBRARIES(test_smaf_sequ libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME smaf_sequ COMMAND test_smaf_sequ)

IF (WANT_MAFM)
    ADD_EXECUTABLE(test_ma7_voice test_ma7_voice.c)
    TARGET_INCLUDE_DIRECTORIES(test_ma7_voice PRIVATE ${CMAKE_SOURCE_DIR}/src)
    TARGET_LINK_LIBRARIES(test_ma7_voice libwildmidi-static ${M_LIBRARY})
    ADD_TEST(NAME ma7_voice COMMAND test_ma7_voice)

    ADD_EXECUTABLE(test_smaf_mtsp test_smaf_mtsp.c)
    TARGET_INCLUDE_DIRECTORIES(test_smaf_mtsp PRIVATE ${CMAKE_SOURCE_DIR}/include)
    TARGET_LINK_LIBRARIES(test_smaf_mtsp libwildmidi-static ${M_LIBRARY})
    ADD_TEST(NAME smaf_mtsp COMMAND test_smaf_mtsp)
ENDIF (WANT_MAFM)
