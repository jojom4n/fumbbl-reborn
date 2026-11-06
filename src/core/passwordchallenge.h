// =============================================================================
// PasswordChallenge — official FUMBBL login challenge/response
//
// 1:1 port of com.fumbbl.ffb.PasswordChallenge (author "Kalimar"):
//
//     PWD  = MD5(clear-text password)        // 16 binary bytes, the HMAC key
//     R    = MD5( (PWD ^ 0x5c…) + MD5( (PWD ^ 0x36…) + CHL ) )
//     response = hex(R)                      // CHL = hex challenge from the
//                                            // server, decoded to bytes
//
// With an EMPTY challenge the response is simply hex(PWD) (deterministic
// vector used by the tests). The response goes into the "password" field of
// clientJoin.
//
// The client only ever needs to keep MD5(password) — the plaintext is
// never stored (the OS keychain holds {coach, passwordMd5}).
// =============================================================================
#pragma once

#include <QByteArray>
#include <QString>

namespace fumbbl {

class PasswordChallenge
{
public:
    // pwdMd5: 16-byte binary MD5 of the clear-text password.
    // challengeHex: hex challenge from serverPasswordChallenge; an empty
    // challenge yields hex(pwdMd5).
    static QString response(const QString &challengeHex, const QByteArray &pwdMd5);

    // MD5 of the clear-text password (UTF-8) — the value to persist.
    static QByteArray encodePassword(const QString &password);
};

} // namespace fumbbl
