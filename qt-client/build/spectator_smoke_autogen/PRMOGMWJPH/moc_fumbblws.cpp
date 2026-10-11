/****************************************************************************
** Meta object code from reading C++ file 'fumbblws.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/fumbblws.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'fumbblws.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN8FumbblWsE_t {};
} // unnamed namespace

template <> constexpr inline auto FumbblWs::qt_create_metaobjectdata<qt_meta_tag_ZN8FumbblWsE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "FumbblWs",
        "opened",
        "",
        "closed",
        "code",
        "reason",
        "reconnectScheduled",
        "attempt",
        "errorOccurred",
        "message",
        "sending",
        "netCommandId",
        "QJsonObject",
        "payload",
        "received",
        "serverVersionReceived",
        "userSettingsReceived",
        "joined",
        "left",
        "gameStateReceived",
        "game",
        "modelSyncReceived",
        "QJsonArray",
        "changes",
        "reports",
        "gameTimeReceived",
        "gameTimeMs",
        "turnTimeMs",
        "talkReceived",
        "soundReceived",
        "adminMessageReceived",
        "socketClosedByServer",
        "pongReceived",
        "timestamp"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'opened'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'closed'
        QtMocHelpers::SignalData<void(int, const QString &)>(3, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 4 }, { QMetaType::QString, 5 },
        }}),
        // Signal 'reconnectScheduled'
        QtMocHelpers::SignalData<void(int)>(6, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 7 },
        }}),
        // Signal 'errorOccurred'
        QtMocHelpers::SignalData<void(const QString &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 },
        }}),
        // Signal 'sending'
        QtMocHelpers::SignalData<void(const QString &, const QJsonObject &)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { 0x80000000 | 12, 13 },
        }}),
        // Signal 'received'
        QtMocHelpers::SignalData<void(const QString &, const QJsonObject &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 11 }, { 0x80000000 | 12, 13 },
        }}),
        // Signal 'serverVersionReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'userSettingsReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(16, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'joined'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(17, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'left'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'gameStateReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 20 },
        }}),
        // Signal 'modelSyncReceived'
        QtMocHelpers::SignalData<void(const QJsonArray &, const QJsonArray &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 22, 23 }, { 0x80000000 | 22, 24 },
        }}),
        // Signal 'gameTimeReceived'
        QtMocHelpers::SignalData<void(qint64, qint64)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 26 }, { QMetaType::LongLong, 27 },
        }}),
        // Signal 'talkReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'soundReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'adminMessageReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'socketClosedByServer'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 },
        }}),
        // Signal 'pongReceived'
        QtMocHelpers::SignalData<void(qint64)>(32, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 33 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<FumbblWs, qt_meta_tag_ZN8FumbblWsE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject FumbblWs::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8FumbblWsE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8FumbblWsE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN8FumbblWsE_t>.metaTypes,
    nullptr
} };

void FumbblWs::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<FumbblWs *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->opened(); break;
        case 1: _t->closed((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->reconnectScheduled((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->errorOccurred((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 4: _t->sending((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 5: _t->received((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 6: _t->serverVersionReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 7: _t->userSettingsReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 8: _t->joined((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 9: _t->left((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 10: _t->gameStateReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 11: _t->modelSyncReceived((*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[2]))); break;
        case 12: _t->gameTimeReceived((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2]))); break;
        case 13: _t->talkReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 14: _t->soundReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 15: _t->adminMessageReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 16: _t->socketClosedByServer((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 17: _t->pongReceived((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)()>(_a, &FumbblWs::opened, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(int , const QString & )>(_a, &FumbblWs::closed, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(int )>(_a, &FumbblWs::reconnectScheduled, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QString & )>(_a, &FumbblWs::errorOccurred, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QString & , const QJsonObject & )>(_a, &FumbblWs::sending, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QString & , const QJsonObject & )>(_a, &FumbblWs::received, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::serverVersionReceived, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::userSettingsReceived, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::joined, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::left, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::gameStateReceived, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonArray & , const QJsonArray & )>(_a, &FumbblWs::modelSyncReceived, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(qint64 , qint64 )>(_a, &FumbblWs::gameTimeReceived, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::talkReceived, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::soundReceived, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::adminMessageReceived, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(const QJsonObject & )>(_a, &FumbblWs::socketClosedByServer, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (FumbblWs::*)(qint64 )>(_a, &FumbblWs::pongReceived, 17))
            return;
    }
}

const QMetaObject *FumbblWs::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *FumbblWs::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN8FumbblWsE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int FumbblWs::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 18)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 18;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 18)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 18;
    }
    return _id;
}

// SIGNAL 0
void FumbblWs::opened()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void FumbblWs::closed(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void FumbblWs::reconnectScheduled(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void FumbblWs::errorOccurred(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void FumbblWs::sending(const QString & _t1, const QJsonObject & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1, _t2);
}

// SIGNAL 5
void FumbblWs::received(const QString & _t1, const QJsonObject & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void FumbblWs::serverVersionReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1);
}

// SIGNAL 7
void FumbblWs::userSettingsReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void FumbblWs::joined(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void FumbblWs::left(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1);
}

// SIGNAL 10
void FumbblWs::gameStateReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1);
}

// SIGNAL 11
void FumbblWs::modelSyncReceived(const QJsonArray & _t1, const QJsonArray & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}

// SIGNAL 12
void FumbblWs::gameTimeReceived(qint64 _t1, qint64 _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 12, nullptr, _t1, _t2);
}

// SIGNAL 13
void FumbblWs::talkReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 13, nullptr, _t1);
}

// SIGNAL 14
void FumbblWs::soundReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 14, nullptr, _t1);
}

// SIGNAL 15
void FumbblWs::adminMessageReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1);
}

// SIGNAL 16
void FumbblWs::socketClosedByServer(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 16, nullptr, _t1);
}

// SIGNAL 17
void FumbblWs::pongReceived(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 17, nullptr, _t1);
}
QT_WARNING_POP
