AUX_SOURCE_DIRECTORY(. DIR_STRING_SRC_LIST)

ADD_LIBRARY(string SHARED ${DIR_STRING_SRC_LIST})

SET_TARGET_PROPERTIES(string PROPERTIES VERSION 1.0.0 SOVERSION 1)

INSTALL(TARGETS string DESTINATION lib)
INSTALL(DIRECTORY ./ DESTINATION include/string FILES_MATCHING PATTERN *.h)
