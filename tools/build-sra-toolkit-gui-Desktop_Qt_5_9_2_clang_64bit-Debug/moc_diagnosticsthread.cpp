/****************************************************************************
** Meta object code from reading C++ file 'diagnosticsthread.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../sra-toolkit/diagnostics/diagnosticsthread.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'diagnosticsthread.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_DiagnosticsThread_t {
    QByteArrayData data[8];
    char stringdata0[70];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DiagnosticsThread_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DiagnosticsThread_t qt_meta_stringdata_DiagnosticsThread = {
    {
QT_MOC_LITERAL(0, 0, 17), // "DiagnosticsThread"
QT_MOC_LITERAL(1, 18, 8), // "finished"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 6), // "handle"
QT_MOC_LITERAL(4, 35, 16), // "DiagnosticsTest*"
QT_MOC_LITERAL(5, 52, 4), // "test"
QT_MOC_LITERAL(6, 57, 5), // "begin"
QT_MOC_LITERAL(7, 63, 6) // "cancel"

    },
    "DiagnosticsThread\0finished\0\0handle\0"
    "DiagnosticsTest*\0test\0begin\0cancel"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DiagnosticsThread[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       4,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   34,    2, 0x06 /* Public */,
       3,    1,   35,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       6,    0,   38,    2, 0x0a /* Public */,
       7,    0,   39,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    5,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void DiagnosticsThread::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        DiagnosticsThread *_t = static_cast<DiagnosticsThread *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->finished(); break;
        case 1: _t->handle((*reinterpret_cast< DiagnosticsTest*(*)>(_a[1]))); break;
        case 2: _t->begin(); break;
        case 3: _t->cancel(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (DiagnosticsThread::*_t)();
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DiagnosticsThread::finished)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (DiagnosticsThread::*_t)(DiagnosticsTest * );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&DiagnosticsThread::handle)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject DiagnosticsThread::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_DiagnosticsThread.data,
      qt_meta_data_DiagnosticsThread,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *DiagnosticsThread::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *DiagnosticsThread::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DiagnosticsThread.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int DiagnosticsThread::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void DiagnosticsThread::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void DiagnosticsThread::handle(DiagnosticsTest * _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
