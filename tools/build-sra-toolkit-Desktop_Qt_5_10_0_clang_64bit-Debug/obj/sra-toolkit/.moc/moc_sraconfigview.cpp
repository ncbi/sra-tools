/****************************************************************************
** Meta object code from reading C++ file 'sraconfigview.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.10.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../sra-toolkit/config/sraconfigview.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sraconfigview.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.10.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SRAConfigView_t {
    QByteArrayData data[8];
    char stringdata0[122];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SRAConfigView_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SRAConfigView_t qt_meta_stringdata_SRAConfigView = {
    {
QT_MOC_LITERAL(0, 0, 13), // "SRAConfigView"
QT_MOC_LITERAL(1, 14, 21), // "toggle_remote_enabled"
QT_MOC_LITERAL(2, 36, 0), // ""
QT_MOC_LITERAL(3, 37, 7), // "toggled"
QT_MOC_LITERAL(4, 45, 20), // "toggle_local_caching"
QT_MOC_LITERAL(5, 66, 15), // "toggle_use_site"
QT_MOC_LITERAL(6, 82, 16), // "toggle_use_proxy"
QT_MOC_LITERAL(7, 99, 22) // "toggle_allow_all_certs"

    },
    "SRAConfigView\0toggle_remote_enabled\0"
    "\0toggled\0toggle_local_caching\0"
    "toggle_use_site\0toggle_use_proxy\0"
    "toggle_allow_all_certs"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SRAConfigView[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
       5,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    1,   39,    2, 0x08 /* Private */,
       4,    1,   42,    2, 0x08 /* Private */,
       5,    1,   45,    2, 0x08 /* Private */,
       6,    1,   48,    2, 0x08 /* Private */,
       7,    1,   51,    2, 0x08 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,

       0        // eod
};

void SRAConfigView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SRAConfigView *_t = static_cast<SRAConfigView *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->toggle_remote_enabled((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 1: _t->toggle_local_caching((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->toggle_use_site((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 3: _t->toggle_use_proxy((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 4: _t->toggle_allow_all_certs((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject SRAConfigView::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SRAConfigView.data,
      qt_meta_data_SRAConfigView,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *SRAConfigView::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SRAConfigView::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SRAConfigView.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SRAConfigView::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 5;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
