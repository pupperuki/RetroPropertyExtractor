CONFIG   += console

TEMPLATE = app

INCLUDEPATH += $$PWD\LibCommon\Source

win32: {
        QMAKE_CXXFLAGS += /WX \  # Treat warnings as errors
                /wd4267 \        # Disable C4267: conversion from 'size_t' to 'type', possible loss of data
                /wd4100 \        # Disable C4100: unreferenced formal parameter
                /wd4101 \        # Disable C4101: unreferenced local variable
                /wd4189          # Disable C4189: local variable is initialized but not referenced

        QMAKE_CXXFLAGS_WARN_ON -= -w34100 -w34189 # Override C4100 and C4189 being set to w3 in Qt's default .qmake.conf file
}

CONFIG (debug, debug|release) {
    # Debug Libs
    LIBS += -L$$PWD\LibCommon\Build -lLibCommond

    # Debug Target Dependencies
    win32 {
        PRE_TARGETDEPS += $$PWD\LibCommon/Build/LibCommond.lib
    }
}

CONFIG (release, debug|release) {
    # Release Libs
    LIBS += -L$$PWD\LibCommon\Build -lLibCommon

    # Release Target Dependencies
    win32 {
        PRE_TARGETDEPS += $$PWD\LibCommon\Build\LibCommon.lib
    }
}

INCLUDEPATH += $$PWD\LibCommon\Externals\tinyxml2

SOURCES += main.cpp \
    CByteCodeReader.cpp \
    Masks.cpp \
    CDKLoader.cpp \
    IPropertyLoader.cpp \
    SProperty.cpp \
    CDynamicLinker.cpp \
    CTemplateWriter.cpp \
    CCorruptionLoader.cpp \
    CEchoesTrilogyLoader.cpp \
    CEchoesLoader.cpp \
    CCorruptionProtoLoader.cpp \
    CEchoesDemoLoader.cpp
HEADERS += \
    CByteCodeReader.h \
    Masks.h \
    OpCodes.h \
    CStack.h \
    CDKLoader.h \
    SProperty.h \
    IPropertyLoader.h \
    CDynamicLinker.h \
    CTemplateWriter.h \
    CCorruptionLoader.h \
    CEchoesTrilogyLoader.h \
    CEchoesLoader.h \
    CCorruptionProtoLoader.h \
    CEchoesDemoLoader.h

SOURCES += $$PWD\LibCommon\Externals\tinyxml2\tinyxml2.cpp
