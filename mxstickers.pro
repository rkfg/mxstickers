#-------------------------------------------------
#
# Project created by QtCreator 2019-05-03T13:35:01
#
#-------------------------------------------------

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mxstickers
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

CONFIG += c++14

SOURCES += \
        src/main.cpp \
        src/mainwindow.cpp \
    src/preferences.cpp \
    src/accesstokenpage.cpp \
    src/roomseditorpage.cpp \
    src/dbmanager.cpp \
    src/tageditor.cpp \
    src/previewfiledialog.cpp \
    src/archivemanager.cpp \
    src/zip.c \
    src/qzip.cpp \
    src/matrixapi.cpp

HEADERS += \
        src/mainwindow.h \
    src/preferences.h \
    src/accesstokenpage.h \
    src/roomseditorpage.h \
    src/itemutil.h \
    src/dbmanager.h \
    src/tageditor.h \
    src/previewfiledialog.h \
    src/archivemanager.h \
    src/qzip.h \
    src/matrixapi.h
    src/miniz.h
    src/zip.h

FORMS += \
        ui/mainwindow.ui \
    ui/preferences.ui \
    ui/accesstokenpage.ui \
    ui/roomseditorpage.ui \
    ui/tageditor.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res/resources.qrc
