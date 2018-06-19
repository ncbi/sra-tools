/****************************************************************************
** Meta object code from reading C++ file 'diagnosticsview.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../sra-toolkit/diagnostics/sradiagnosticsview.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'diagnosticsview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_DiagnosticsView_t {
    QByteArrayData data[7];
    char stringdata0[92];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_DiagnosticsView_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_DiagnosticsView_t qt_meta_stringdata_DiagnosticsView = {
    {
QT_MOC_LITERAL(0, 0, 15), // "DiagnosticsView"
QT_MOC_LITERAL(1, 16, 15), // "handle_callback"
QT_MOC_LITERAL(2, 32, 0), // ""
QT_MOC_LITERAL(3, 33, 16), // "DiagnosticsTest*"
QT_MOC_LITERAL(4, 50, 4), // "test"
QT_MOC_LITERAL(5, 55, 17), // "start_diagnostics"
QT_MOC_LITERAL(6, 73, 18) // "cancel_diagnostics"

    },
    "DiagnosticsView\0handle_callback\0\0"
    "DiagnosticsTest*\0test\0start_diagnostics\0"
    "cancel_diagnostics"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_DiagnosticsView[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       3,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   29,    2, 0x0a /* Public */,
       5,    0,   32,    2, 0x08 /* Private */,
       6,    0,   33,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void SRADiagnosticsView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SRADiagnosticsView *_t = static_cast<SRADiagnosticsView *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->handle_callback((*reinterpret_cast< DiagnosticsTest*(*)>(_a[1]))); break;
        case 1: _t->start_diagnostics(); break;
        case 2: _t->cancel_diagnostics(); break;
        default: ;
        }
    }
}

const QMetaObject SRADiagnosticsView::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_DiagnosticsView.data,
      qt_meta_data_DiagnosticsView,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *SRADiagnosticsView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SRADiagnosticsView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_DiagnosticsView.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SRADiagnosticsView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 3)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 3;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 3)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 3;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
