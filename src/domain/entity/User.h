#pragma once

#include <QString>
#include <QDateTime>

/**
 * @brief 代表系统用户的领域层实体 (Domain Entity)
 * 
 * 本类是代表用户的核心业务对象。
 * 它完全剥离了所有的鉴权细节数据（如密码哈希、盐值、或加密后的人脸特征），
 * 旨在确保业务领域逻辑与底层数据持久化层之间拥有清晰、严谨的架构边界。
 */
class User {
public:
    User() = default;
    User(int uid, const QString& username, const QString& email, 
         const QString& phone, bool hasFace, const QDateTime& createdAt)
        : m_uid(uid), m_username(username), m_email(email), 
          m_phone(phone), m_hasFace(hasFace), m_createdAt(createdAt) {}

    [[nodiscard]] int uid() const { return m_uid; }
    void setUid(int uid) { m_uid = uid; }

    [[nodiscard]] QString username() const { return m_username; }
    void setUsername(const QString& username) { m_username = username; }

    [[nodiscard]] QString email() const { return m_email; }
    void setEmail(const QString& email) { m_email = email; }

    // 注意：这里的 phone 应当仅存储用于展示的脱敏格式（如 138****1234）
    [[nodiscard]] QString phone() const { return m_phone; }
    void setPhone(const QString& phone) { m_phone = phone; }

    [[nodiscard]] bool hasFace() const { return m_hasFace; }
    void setHasFace(bool hasFace) { m_hasFace = hasFace; }

    [[nodiscard]] QDateTime createdAt() const { return m_createdAt; }
    void setCreatedAt(const QDateTime& createdAt) { m_createdAt = createdAt; }

    [[nodiscard]] bool isValid() const {
        return m_uid != -1 && !m_username.isEmpty();
    }

private:
    int m_uid = -1;
    QString m_username;
    QString m_email;
    QString m_phone;
    bool m_hasFace = false;
    QDateTime m_createdAt;
};
