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
    QByteArrayData data[14];
    char stringdata0[210];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SRAConfigView_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SRAConfigView_t qt_meta_stringdata_SRAConfigView = {
    {
QT_MOC_LITERAL(0, 0, 13), // "SRAConfigView"
QT_MOC_LITERAL(1, 14, 12), // "dirty_config"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 13), // "commit_config"
QT_MOC_LITERAL(4, 42, 13), // "reload_config"
QT_MOC_LITERAL(5, 56, 14), // "default_config"
QT_MOC_LITERAL(6, 71, 15), // "modified_config"
QT_MOC_LITERAL(7, 87, 15), // "edit_proxy_path"
QT_MOC_LITERAL(8, 103, 21), // "toggle_remote_enabled"
QT_MOC_LITERAL(9, 125, 7), // "toggled"
QT_MOC_LITERAL(10, 133, 20), // "toggle_local_caching"
QT_MOC_LITERAL(11, 154, 15), // "toggle_use_site"
QT_MOC_LITERAL(12, 170, 16), // "toggle_use_proxy"
QT_MOC_LITERAL(13, 187, 22) // "toggle_allow_all_certs"

    },
    "SRAConfigView\0dirty_config\0\0commit_config\0"
    "reload_config\0default_config\0"
    "modified_config\0edit_proxy_path\0"
    "toggle_remote_enabled\0toggled\0"
    "toggle_local_caching\0toggle_use_site\0"
    "toggle_use_proxy\0toggle_allow_all_certs"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SRAConfigView[] = {

 // content:
       7,       // revision
       0,       // classname
       0,    0, // classinfo
      11,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   69,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,   70,    2, 0x08 /* Private */,
       4,    0,   71,    2, 0x08 /* Private */,
       5,    0,   72,    2, 0x08 /* Private */,
       6,    0,   73,    2, 0x08 /* Private */,
       7,    0,   74,    2, 0x08 /* Private */,
       8,    1,   75,    2, 0x08 /* Private */,
      10,    1,   78,    2, 0x08 /* Private */,
      11,    1,   81,    2, 0x08 /* Private */,
      12,    1,   84,    2, 0x08 /* Private */,
      13,    1,   87,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,
    QMetaType::Void, QMetaType::Int,    9,

       0        // eod
};

void SRAConfigView::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SRAConfigView *_t = static_cast<SRAConfigView *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->dirty_config(); break;
        case 1: _t->commit_config(); break;
        case 2: _t->reload_config(); break;
        case 3: _t->default_config(); break;
        case 4: _t->modified_config(); break;
        case 5: _t->edit_proxy_path(); break;
        case 6: _t->toggle_remote_enabled((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 7: _t->toggle_local_caching((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: _t->toggle_use_site((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 9: _t->toggle_use_proxy((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 10: _t->toggle_allow_all_certs((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            typedef void (SRAConfigView::*_t)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&SRAConfigView::dirty_config)) {
                *result = 0;
                return;
            }
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
        if (_id < 11)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 11;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 11)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 11;
    }
    return _id;
}

// SIGNAL 0
void SRAConfigView::dirty_config()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
