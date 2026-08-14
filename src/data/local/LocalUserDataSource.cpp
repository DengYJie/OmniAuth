#include "LocalUserDataSource.h"
#include "core/CryptoUtils.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>
#include <QSqlRecord>

LocalUserDataSource::LocalUserDataSource() {}

LocalUserDataSource::~LocalUserDataSource() {
    close();
}

void LocalUserDataSource::loadOrCreateCryptoKeys() {
    QSettings settings("OmniAuth", "CryptoConfig");
    if (settings.contains("secretbox_key") && settings.contains("secretbox_nonce")) {
        m_secretBoxKey = settings.value("secretbox_key").toByteArray();
        m_secretBoxNonce = settings.value("secretbox_nonce").toByteArray();
    } else {
        m_secretBoxKey = CryptoUtils::generateSecretBoxKey();
        m_secretBoxNonce = CryptoUtils::generateSecretBoxNonce();
        settings.setValue("secretbox_key", m_secretBoxKey);
        settings.setValue("secretbox_nonce", m_secretBoxNonce);
    }
}

bool LocalUserDataSource::init() {
    loadOrCreateCryptoKeys();

    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        m_db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        m_db = QSqlDatabase::addDatabase("QSQLITE");
    }

    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir;
    if (!dir.exists(dataDir)) {
        dir.mkpath(dataDir);
    }
    
    QString dbPath = dir.filePath("omniauth_core.db");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    QSqlQuery query(m_db);
    QString createTableQuery = R"(
        CREATE TABLE IF NOT EXISTS sys_users (
            uid INTEGER PRIMARY KEY AUTOINCREMENT,
            username VARCHAR(64) NOT NULL,
            pwd_hash VARCHAR(128) NOT NULL,
            salt VARCHAR(32) NOT NULL,
            email VARCHAR(128),
            phone VARCHAR(32),
            enc_phone VARCHAR(128),
            has_face INTEGER DEFAULT 0,
            face_encodings BLOB,
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createTableQuery)) {
        qWarning() << "Failed to create sys_users table:" << query.lastError().text();
        return false;
    }

    QSqlQuery checkCol(m_db);
    if (checkCol.exec("PRAGMA table_info(sys_users)")) {
        bool hasEmailCol = false;
        bool hasPhoneCol = false;
        bool hasEncPhoneCol = false;
        bool hasFaceCol = false;
        while (checkCol.next()) {
            QString colName = checkCol.value("name").toString();
            if (colName == "email") hasEmailCol = true;
            if (colName == "phone") hasPhoneCol = true;
            if (colName == "enc_phone") hasEncPhoneCol = true;
            if (colName == "has_face") hasFaceCol = true;
        }
        if (!hasEmailCol) query.exec("ALTER TABLE sys_users ADD COLUMN email VARCHAR(128)");
        if (!hasPhoneCol) query.exec("ALTER TABLE sys_users ADD COLUMN phone VARCHAR(32)");
        if (!hasEncPhoneCol) query.exec("ALTER TABLE sys_users ADD COLUMN enc_phone VARCHAR(128)");
        if (!hasFaceCol) query.exec("ALTER TABLE sys_users ADD COLUMN has_face INTEGER DEFAULT 0");
    }

    qDebug() << "Database initialized successfully at:" << dbPath;
    return true;
}

void LocalUserDataSource::close() {
    if (m_db.isOpen()) {
        m_db.close();
    }
}

void LocalUserDataSource::createUserAsync(const QString& username, const QString& pwd_hash, 
                                          const QString& email, const QString& phone,
                                          std::function<void(bool)> callback) {
    QString salt = ""; // Simplified for now, since hash happens in Service layer or is passed in
    
    QString maskedPhone = phone;
    QString encPhoneStr = "";
    if (!phone.isEmpty()) {
        if (phone.length() == 11) {
            maskedPhone = phone.left(3) + "****" + phone.right(4);
        }
        QByteArray encPhoneRaw = CryptoUtils::encryptData(phone.toUtf8(), m_secretBoxKey, m_secretBoxNonce);
        encPhoneStr = QString::fromUtf8(encPhoneRaw.toBase64());
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        INSERT INTO sys_users (username, pwd_hash, salt, email, phone, enc_phone, has_face)
        VALUES (:username, :pwd_hash, :salt, :email, :phone, :enc_phone, 0)
    )");
    query.bindValue(":username", username);
    query.bindValue(":pwd_hash", pwd_hash);
    query.bindValue(":salt", salt);
    query.bindValue(":email", email.isEmpty() ? QVariant() : email);
    query.bindValue(":phone", maskedPhone.isEmpty() ? QVariant() : maskedPhone);
    query.bindValue(":enc_phone", encPhoneStr.isEmpty() ? QVariant() : encPhoneStr);

    bool success = query.exec();
    if (!success) {
        qWarning() << "Failed to insert user:" << query.lastError().text();
    }
    callback(success);
}

static UserAuthDTO parseUserAuthRecord(const QSqlQuery& query) {
    UserAuthDTO dto;
    dto.user.setUid(query.value("uid").toInt());
    dto.user.setUsername(query.value("username").toString());
    dto.user.setEmail(query.value("email").toString());
    dto.user.setPhone(query.value("phone").toString());
    dto.user.setHasFace(query.value("has_face").toInt() != 0);
    dto.user.setCreatedAt(query.value("created_at").toDateTime());

    dto.pwdHash = query.value("pwd_hash").toString();
    dto.salt = query.value("salt").toString();
    dto.encPhone = query.value("enc_phone").toString();
    return dto;
}

void LocalUserDataSource::getUserByAccountAsync(const QString& account, 
                                                std::function<void(std::optional<UserAuthDTO>)> callback) {
    QSqlQuery query(m_db);
    // 优先匹配 username, email
    // phone 如果只匹配脱敏是不行的，如果是全明文查询可能查不到。这里先查明文 phone、email、username。
    query.prepare("SELECT uid, username, pwd_hash, salt, email, phone, enc_phone, has_face, created_at FROM sys_users WHERE username = :acc OR email = :acc OR phone = :acc");
    query.bindValue(":acc", account);

    if (query.exec() && query.next()) {
        callback(parseUserAuthRecord(query));
        return;
    }
    
    // 如果找不到，尝试匹配加密手机号
    QByteArray encAccRaw = CryptoUtils::encryptData(account.toUtf8(), m_secretBoxKey, m_secretBoxNonce);
    QString encAccStr = QString::fromUtf8(encAccRaw.toBase64());
    
    query.prepare("SELECT uid, username, pwd_hash, salt, email, phone, enc_phone, has_face, created_at FROM sys_users WHERE enc_phone = :enc_acc");
    query.bindValue(":enc_acc", encAccStr);
    
    if (query.exec() && query.next()) {
        callback(parseUserAuthRecord(query));
    } else {
        callback(std::nullopt);
    }
}

void LocalUserDataSource::getUserByIdAsync(int uid, 
                                           std::function<void(std::optional<User>)> callback) {
    QSqlQuery query(m_db);
    query.prepare("SELECT uid, username, pwd_hash, salt, email, phone, enc_phone, has_face, created_at FROM sys_users WHERE uid = :uid");
    query.bindValue(":uid", uid);

    if (query.exec() && query.next()) {
        callback(parseUserAuthRecord(query).user);
    } else {
        callback(std::nullopt);
    }
}

void LocalUserDataSource::updatePasswordAsync(int uid, const QString& newPwdHash, 
                                              std::function<void(bool)> callback) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE sys_users SET pwd_hash = :pwd_hash WHERE uid = :uid");
    query.bindValue(":pwd_hash", newPwdHash);
    query.bindValue(":uid", uid);
    callback(query.exec());
}

void LocalUserDataSource::updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                                  std::function<void(bool)> callback) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE sys_users SET face_encodings = :face, has_face = 1 WHERE uid = :uid");
    query.bindValue(":face", encryptedFaceData);
    query.bindValue(":uid", uid);

    bool success = query.exec();
    if (!success) {
        qWarning() << "Failed to update face encoding:" << query.lastError().text();
    }
    callback(success);
}

void LocalUserDataSource::removeFaceEncodingAsync(int uid, std::function<void(bool)> callback) {
    QSqlQuery query(m_db);
    query.prepare("UPDATE sys_users SET face_encodings = NULL, has_face = 0 WHERE uid = :uid");
    query.bindValue(":uid", uid);
    bool success = query.exec();
    callback(success);
}

void LocalUserDataSource::hasUserFaceAsync(const QString& account, std::function<void(bool)> callback) {
    getUserByAccountAsync(account, [this, callback](std::optional<UserAuthDTO> dto) {
        if (dto) {
            hasUserFaceAsync(dto->user.uid(), callback);
        } else {
            callback(false);
        }
    });
}

void LocalUserDataSource::hasUserFaceAsync(int uid, std::function<void(bool)> callback) {
    QSqlQuery query(m_db);
    query.prepare("SELECT has_face, face_encodings FROM sys_users WHERE uid = :uid");
    query.bindValue(":uid", uid);
    if (query.exec() && query.next()) {
        int hasFaceVal = query.value("has_face").toInt();
        QByteArray enc = query.value("face_encodings").toByteArray();
        callback(hasFaceVal != 0 || !enc.isEmpty());
    } else {
        callback(false);
    }
}

void LocalUserDataSource::saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                                   std::function<void(bool)> callback) {
    if (feature.empty()) {
        callback(false);
        return;
    }
    QByteArray rawData(reinterpret_cast<const char*>(feature.data()), feature.size() * sizeof(float));
    QByteArray encryptedFace = CryptoUtils::encryptData(rawData, m_secretBoxKey, m_secretBoxNonce);

    updateFaceEncodingAsync(uid, encryptedFace, callback);
}

void LocalUserDataSource::getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback) {
    std::vector<std::pair<int, std::vector<float>>> features;
    QSqlQuery query("SELECT uid, face_encodings FROM sys_users WHERE face_encodings IS NOT NULL", m_db);
    while (query.next()) {
        int uid = query.value("uid").toInt();
        QByteArray encryptedData = query.value("face_encodings").toByteArray();
        if (encryptedData.isEmpty()) continue;

        QByteArray decryptedData = CryptoUtils::decryptData(encryptedData, m_secretBoxKey, m_secretBoxNonce);
        if (decryptedData.isEmpty() || decryptedData.size() % sizeof(float) != 0) continue;

        const float* dbFeaturePtr = reinterpret_cast<const float*>(decryptedData.constData());
        size_t dbLen = decryptedData.size() / sizeof(float);
        
        features.emplace_back(uid, std::vector<float>(dbFeaturePtr, dbFeaturePtr + dbLen));
    }
    callback(std::move(features));
}
