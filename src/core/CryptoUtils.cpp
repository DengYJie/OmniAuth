#include "CryptoUtils.h"
#include <sodium.h>
#include <QDebug>

bool CryptoUtils::init() {
    if (sodium_init() < 0) {
        qCritical() << "libsodium initialization failed!";
        return false;
    }
    return true;
}

QString CryptoUtils::hashPassword(const QString& password) {
    QByteArray pwdBytes = password.toUtf8();
    char hashed_pwd[crypto_pwhash_STRBYTES];

    if (crypto_pwhash_str(
            hashed_pwd,
            pwdBytes.constData(),
            pwdBytes.length(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        qWarning() << "Out of memory while hashing password";
        return QString();
    }
    return QString::fromLatin1(hashed_pwd);
}

bool CryptoUtils::verifyPassword(const QString& hashStr, const QString& password) {
    QByteArray pwdBytes = password.toUtf8();
    QByteArray hashBytes = hashStr.toLatin1();

    return crypto_pwhash_str_verify(
               hashBytes.constData(),
               pwdBytes.constData(),
               pwdBytes.length()) == 0;
}

QByteArray CryptoUtils::encryptData(const QByteArray& plaintext, const QByteArray& key, const QByteArray& nonce) {
    if (key.length() != crypto_secretbox_KEYBYTES || nonce.length() != crypto_secretbox_NONCEBYTES) {
        qWarning() << "Invalid key or nonce size for encryption";
        return QByteArray();
    }

    QByteArray ciphertext;
    ciphertext.resize(plaintext.length() + crypto_secretbox_MACBYTES);

    crypto_secretbox_easy(
        reinterpret_cast<unsigned char*>(ciphertext.data()),
        reinterpret_cast<const unsigned char*>(plaintext.constData()),
        plaintext.length(),
        reinterpret_cast<const unsigned char*>(nonce.constData()),
        reinterpret_cast<const unsigned char*>(key.constData())
    );

    return ciphertext;
}

QByteArray CryptoUtils::decryptData(const QByteArray& ciphertext, const QByteArray& key, const QByteArray& nonce) {
    if (key.length() != crypto_secretbox_KEYBYTES || nonce.length() != crypto_secretbox_NONCEBYTES) {
        qWarning() << "Invalid key or nonce size for decryption";
        return QByteArray();
    }

    if (ciphertext.length() < crypto_secretbox_MACBYTES) {
        qWarning() << "Ciphertext too short";
        return QByteArray();
    }

    QByteArray plaintext;
    plaintext.resize(ciphertext.length() - crypto_secretbox_MACBYTES);

    if (crypto_secretbox_open_easy(
            reinterpret_cast<unsigned char*>(plaintext.data()),
            reinterpret_cast<const unsigned char*>(ciphertext.constData()),
            ciphertext.length(),
            reinterpret_cast<const unsigned char*>(nonce.constData()),
            reinterpret_cast<const unsigned char*>(key.constData())) != 0) {
        qWarning() << "Message forged or decryption failed!";
        return QByteArray();
    }

    return plaintext;
}

QByteArray CryptoUtils::generateSecretBoxKey() {
    QByteArray key;
    key.resize(crypto_secretbox_KEYBYTES);
    randombytes_buf(key.data(), crypto_secretbox_KEYBYTES);
    return key;
}

QByteArray CryptoUtils::generateSecretBoxNonce() {
    QByteArray nonce;
    nonce.resize(crypto_secretbox_NONCEBYTES);
    randombytes_buf(nonce.data(), crypto_secretbox_NONCEBYTES);
    return nonce;
}
