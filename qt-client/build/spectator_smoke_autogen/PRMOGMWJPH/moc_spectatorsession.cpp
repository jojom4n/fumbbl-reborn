/****************************************************************************
** Meta object code from reading C++ file 'spectatorsession.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/spectatorsession.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'spectatorsession.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN16SpectatorSessionE_t {};
} // unnamed namespace

template <> constexpr inline auto SpectatorSession::qt_create_metaobjectdata<qt_meta_tag_ZN16SpectatorSessionE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SpectatorSession",
        "progress",
        "",
        "message",
        "fatal",
        "serverVersionReceived",
        "QJsonObject",
        "payload",
        "joined",
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
        "pongReceived",
        "timestamp",
        "protocolSending",
        "netCommandId",
        "protocolReceived",
        "wsClosed",
        "code",
        "reason",
        "onAccessToken",
        "token",
        "onSessionToken",
        "onAuthFailed",
        "step",
        "error"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'progress'
        QtMocHelpers::SignalData<void(const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'fatal'
        QtMocHelpers::SignalData<void(const QString &)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 },
        }}),
        // Signal 'serverVersionReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'joined'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'gameStateReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 10 },
        }}),
        // Signal 'modelSyncReceived'
        QtMocHelpers::SignalData<void(const QJsonArray &, const QJsonArray &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 12, 13 }, { 0x80000000 | 12, 14 },
        }}),
        // Signal 'gameTimeReceived'
        QtMocHelpers::SignalData<void(qint64, qint64)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 16 }, { QMetaType::LongLong, 17 },
        }}),
        // Signal 'talkReceived'
        QtMocHelpers::SignalData<void(const QJsonObject &)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'pongReceived'
        QtMocHelpers::SignalData<void(qint64)>(19, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::LongLong, 20 },
        }}),
        // Signal 'protocolSending'
        QtMocHelpers::SignalData<void(const QString &, const QJsonObject &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { 0x80000000 | 6, 7 },
        }}),
        // Signal 'protocolReceived'
        QtMocHelpers::SignalData<void(const QString &, const QJsonObject &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { 0x80000000 | 6, 7 },
        }}),
        // Signal 'wsClosed'
        QtMocHelpers::SignalData<void(int, const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 25 }, { QMetaType::QString, 26 },
        }}),
        // Slot 'onAccessToken'
        QtMocHelpers::SlotData<void(const QString &)>(27, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Slot 'onSessionToken'
        QtMocHelpers::SlotData<void(const QString &)>(29, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 28 },
        }}),
        // Slot 'onAuthFailed'
        QtMocHelpers::SlotData<void(const QString &, const QString &)>(30, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::QString, 31 }, { QMetaType::QString, 32 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SpectatorSession, qt_meta_tag_ZN16SpectatorSessionE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SpectatorSession::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SpectatorSessionE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SpectatorSessionE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN16SpectatorSessionE_t>.metaTypes,
    nullptr
} };

void SpectatorSession::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SpectatorSession *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->progress((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 1: _t->fatal((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 2: _t->serverVersionReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 3: _t->joined((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 4: _t->gameStateReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 5: _t->modelSyncReceived((*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonArray>>(_a[2]))); break;
        case 6: _t->gameTimeReceived((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<qint64>>(_a[2]))); break;
        case 7: _t->talkReceived((*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[1]))); break;
        case 8: _t->pongReceived((*reinterpret_cast<std::add_pointer_t<qint64>>(_a[1]))); break;
        case 9: _t->protocolSending((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 10: _t->protocolReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QJsonObject>>(_a[2]))); break;
        case 11: _t->wsClosed((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 12: _t->onAccessToken((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 13: _t->onSessionToken((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 14: _t->onAuthFailed((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QString & )>(_a, &SpectatorSession::progress, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QString & )>(_a, &SpectatorSession::fatal, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QJsonObject & )>(_a, &SpectatorSession::serverVersionReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QJsonObject & )>(_a, &SpectatorSession::joined, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QJsonObject & )>(_a, &SpectatorSession::gameStateReceived, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QJsonArray & , const QJsonArray & )>(_a, &SpectatorSession::modelSyncReceived, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(qint64 , qint64 )>(_a, &SpectatorSession::gameTimeReceived, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QJsonObject & )>(_a, &SpectatorSession::talkReceived, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(qint64 )>(_a, &SpectatorSession::pongReceived, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QString & , const QJsonObject & )>(_a, &SpectatorSession::protocolSending, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(const QString & , const QJsonObject & )>(_a, &SpectatorSession::protocolReceived, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (SpectatorSession::*)(int , const QString & )>(_a, &SpectatorSession::wsClosed, 11))
            return;
    }
}

const QMetaObject *SpectatorSession::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SpectatorSession::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN16SpectatorSessionE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SpectatorSession::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}

// SIGNAL 0
void SpectatorSession::progress(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void SpectatorSession::fatal(const QString & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SpectatorSession::serverVersionReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1);
}

// SIGNAL 3
void SpectatorSession::joined(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void SpectatorSession::gameStateReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void SpectatorSession::modelSyncReceived(const QJsonArray & _t1, const QJsonArray & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2);
}

// SIGNAL 6
void SpectatorSession::gameTimeReceived(qint64 _t1, qint64 _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void SpectatorSession::talkReceived(const QJsonObject & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void SpectatorSession::pongReceived(qint64 _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 8, nullptr, _t1);
}

// SIGNAL 9
void SpectatorSession::protocolSending(const QString & _t1, const QJsonObject & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}

// SIGNAL 10
void SpectatorSession::protocolReceived(const QString & _t1, const QJsonObject & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 10, nullptr, _t1, _t2);
}

// SIGNAL 11
void SpectatorSession::wsClosed(int _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 11, nullptr, _t1, _t2);
}
QT_WARNING_POP
