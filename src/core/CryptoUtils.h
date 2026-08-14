#pragma once

#include <QByteArray>
#include <QString>

class CryptoUtils {
public:
    /**
     * @brief 初始化 libsodium 库。必须在调用任何其他加解密函数前执行。
     * @return 成功返回 true，否则返回 false。
     */
    static bool init();

    /**
     * @brief 使用 Argon2id 算法计算密码哈希。内部会自动生成随机 Salt 并将其编码在返回结果中。
     * @param password 明文密码
     * @return 包含算法参数、盐值和最终哈希的组合字符串（长约 97 字节）。
     */
    static QString hashPassword(const QString& password);

    /**
     * @brief 验证给定的明文密码与 Argon2id 哈希字符串是否匹配。
     */
    static bool verifyPassword(const QString& hashStr, const QString& password);

    /**
     * @brief 使用 XSalsa20-Poly1305 (crypto_secretbox_easy) 算法加密数据。
     */
    static QByteArray encryptData(const QByteArray& plaintext, const QByteArray& key, const QByteArray& nonce);

    /**
     * @brief 使用 XSalsa20-Poly1305 (crypto_secretbox_open_easy) 算法解密数据。
     */
    static QByteArray decryptData(const QByteArray& ciphertext, const QByteArray& key, const QByteArray& nonce);
    
    /**
     * @brief 生成适用于 secretbox 的高强度随机密钥 (crypto_secretbox_KEYBYTES)。
     */
    static QByteArray generateSecretBoxKey();

    /**
     * @brief 生成适用于 secretbox 的高强度随机 Nonce (crypto_secretbox_NONCEBYTES)。
     */
    static QByteArray generateSecretBoxNonce();
};
