// =============================================================================
// PasswordChallenge — implementation
// =============================================================================
#include "core/passwordchallenge.h"

#include <QCryptographicHash>

namespace fumbbl {

QString PasswordChallenge::response(const QString &challengeHex, const QByteArray &pwdMd5)
{
    // 1:1 port of PasswordChallenge.createResponse (Kalimar).
    if (challengeHex.isEmpty())
        return QString::fromLatin1(pwdMd5.toHex());

    const QByteArray challenge = QByteArray::fromHex(challengeHex.toUtf8());
    QByteArray opad;
    QByteArray ipad;
    opad.resize(pwdMd5.size());
    ipad.resize(pwdMd5.size());
    for (int i = 0; i < pwdMd5.size(); ++i) {
        opad[i] = char(pwdMd5[i] ^ 0x5c);
        ipad[i] = char(pwdMd5[i] ^ 0x36);
    }
    const QByteArray inner = QCryptographicHash::hash(ipad + challenge, QCryptographicHash::Md5);
    const QByteArray result = QCryptographicHash::hash(opad + inner, QCryptographicHash::Md5);
    return QString::fromLatin1(result.toHex());
}

QByteArray PasswordChallenge::encodePassword(const QString &password)
{
    return QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Md5);
}

} // namespace fumbbl
