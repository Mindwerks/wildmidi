ADD_EXECUTABLE(test_tokenize test_tokenize.c)
TARGET_LINK_LIBRARIES(test_tokenize libwildmidi-static ${M_LIBRARY})
ADD_TEST(NAME tokenize COMMAND test_tokenize)

IF (WANT_SF2)
    ADD_EXECUTABLE(test_sf2 test_sf2.c)
    TARGET_LINK_LIBRARIES(test_sf2 libwildmidi-static ${M_LIBRARY})
    # small soundfont + midi vendored under extern/TinySoundFont/examples
    SET(TSF_EXAMPLES "${PROJECT_SOURCE_DIR}/extern/TinySoundFont/examples")
    ADD_TEST(NAME sf2_direct COMMAND test_sf2
        "${TSF_EXAMPLES}/florestan-subset.sf2" "${TSF_EXAMPLES}/venture.mid")
    FILE(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sf2test.cfg"
        "soundfont \"${TSF_EXAMPLES}/florestan-subset.sf2\"\n")
    ADD_TEST(NAME sf2_cfg COMMAND test_sf2
        "${CMAKE_CURRENT_BINARY_DIR}/sf2test.cfg" "${TSF_EXAMPLES}/venture.mid")
ENDIF()
