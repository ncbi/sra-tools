#-------------------------------------------------
#
# Project created by QtCreator 2017-05-24T11:29:35
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = sra-toolkit
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
LIBS += -L/Users/rodarme1/ncbi-outdir/ncbi-vdb/mac/clang/x86_64/dbg/lib -lncbi-vdb -ldiagnose

INCLUDEPATH +=  ../../../ncbi-vdb/interfaces \
                ../../../ncbi-vdb/interfaces/os/mac \
                ../../../ncbi-vdb/interfaces/os/unix \
                ../../../ncbi-vdb/interfaces/cc/gcc \
                ../../../ncbi-vdb/interfaces/cc/gcc/x86_64

SOURCES += main.cpp\
        sratoolkit.cpp \
        sratoolkitpreferences.cpp \
        sraworkspace.cpp \
        sratoolbar.cpp \
        sratoolview.cpp \
        config/sraconfigmodel.cpp \
        config/sraconfigview.cpp \
        ../vdb-config/configure.cpp \
        ../vdb-config/vdb-config-model.cpp \
        ../vdb-config/util.cpp \
        diagnostics/diagnosticstest.cpp \
        diagnostics/diagnosticsthread.cpp \
        diagnostics/sradiagnosticsview.cpp


HEADERS  += sratoolkit.h \
        sratoolkittemplate.h \
        sratoolkitglobals.h \
        sratoolkitpreferences.h \
        sraworkspace.h \
        sratoolbar.h \
        sratoolview.h \
        config/sraconfigmodel.h \
        config/sraconfigview.h \
        diagnostics/diagnosticstest.h \
        diagnostics/diagnosticsthread.h \
        diagnostics/sradiagnosticsview.h


RESOURCES += \
        resources.qrc

DISTFILES += \
    style.qss
