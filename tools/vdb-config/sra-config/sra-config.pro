#-------------------------------------------------
#
# Project created by QtCreator 2017-03-22T11:17:53
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = sra-config
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any feature of Qt which as been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
LIBS += -L/Users/rodarme1/ncbi-outdir/ncbi-vdb/mac/clang/x86_64/dbg/lib -lncbi-vdb

INCLUDEPATH +=  ../../../../ncbi-vdb/interfaces \
                ../../../../ncbi-vdb/interfaces/os/mac \
                ../../../../ncbi-vdb/interfaces/os/unix \
                ../../../../ncbi-vdb/interfaces/cc/gcc \
                ../../../../ncbi-vdb/interfaces/cc/gcc/x86_64

SOURCES += main.cpp\
        sraconfig.cpp \
    ../configure.cpp \
    ../vdb-config-model.cpp \
    ../util.cpp \
    ../sra-tools-gui/libs/ktoolbaritem.cpp


HEADERS  += sraconfig.h \
    ../sra-tools-gui/interfaces/ktoolbaritem.h

RESOURCES += \
    resources.qrc

