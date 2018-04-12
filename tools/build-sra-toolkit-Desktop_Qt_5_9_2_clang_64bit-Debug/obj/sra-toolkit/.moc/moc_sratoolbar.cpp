/****************************************************************************
** Meta object code from reading C++ file 'sratoolbar.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.9.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../sra-toolkit/sratoolbar.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'sratoolbar.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.9.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_SRAToolBar_t {
    QByteArrayData data[7];
    char stringdata0[57];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_SRAToolBar_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_SRAToolBar_t qt_meta_stringdata_SRAToolBar = {
    {
QT_MOC_LITERAL(0, 0, 10), // "SRAToolBar"
QT_MOC_LITERAL(1, 11, 8), // "expanded"
QT_MOC_LITERAL(2, 20, 0), // ""
QT_MOC_LITERAL(3, 21, 12), // "toolSwitched"
QT_MOC_LITERAL(4, 34, 6), // "expand"
QT_MOC_LITERAL(5, 41, 10), // "switchTool"
QT_MOC_LITERAL(6, 52, 4) // "tool"

    },
    "SRAToolBar\0expanded\0\0toolSwitched\0"
    "expand\0switchTool\0tool"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_SRAToolBar[] = {

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
       1,    1,   34,    2, 0x06 /* Public */,
       3,    1,   37,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       4,    0,   40,    2, 0x0a /* Public */,
       5,    1,   41,    2, 0x0a /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Bool,    2,
    QMetaType::Void, QMetaType::Int,    2,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    6,

       0        // eod
};

void SRAToolBar::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        SRAToolBar *_t = static_cast<SRAToolBar *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->expanded((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 1: _t->toolSwitched((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 2: _t->expand(); break;
        case 3: _t->switchTool((*reinterpret_cast< int(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        void **func = reinterpret_cast<void **>(_a[1]);
        {
            typedef void (SRAToolBar::*_t)(bool );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SRAToolBar::expanded)) {
                *result = 0;
                return;
            }
        }
        {
            typedef void (SRAToolBar::*_t)(int );
            if (*reinterpret_cast<_t *>(func) == static_cast<_t>(&SRAToolBar::toolSwitched)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject SRAToolBar::staticMetaObject = {
    { &QWidget::staticMetaObject, qt_meta_stringdata_SRAToolBar.data,
      qt_meta_data_SRAToolBar,  qt_static_metacall, nullptr, nullptr}
};


const QMetaObject *SRAToolBar::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SRAToolBar::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_SRAToolBar.stringdata0))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int SRAToolBar::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
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
void SRAToolBar::expanded(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void SRAToolBar::toolSwitched(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
