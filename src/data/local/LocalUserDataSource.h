#pragma once

#include "domain/repository/UserRepository.h"
#include <QSqlDatabase>
#include <QByteArray>
#include <vector>
#include <optional>

/**
 * @brief 本地 SQLite 用户数据源
 */
class LocalUserDataSource : public UserRepository {
public:
    LocalUserDataSource();
    ~LocalUserDataSource() override;

    bool init() override;
    void close() override;

    void createUserAsync(const QString& username, const QString& pwd_hash, 
                         const QString& email, const QString& phone,
                         std::function<void(bool)> callback) override;
    
    void getUserByAccountAsync(const QString& account, 
                               std::function<void(std::optional<UserAuthDTO>)> callback) override;
                               
    void getUserByIdAsync(int uid, 
                          std::function<void(std::optional<User>)> callback) override;
                                       
    void updatePasswordAsync(int uid, const QString& newPwdHash, 
                             std::function<void(bool)> callback) override;

    void updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                 std::function<void(bool)> callback) override;
    void removeFaceEncodingAsync(int uid, std::function<void(bool)> callback) override;
    void hasUserFaceAsync(const QString& account, std::function<void(bool)> callback) override;
    void hasUserFaceAsync(int uid, std::function<void(bool)> callback) override;
    
    void saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                  std::function<void(bool)> callback) override;
                                  
    void getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback) override;

private:
    QSqlDatabase m_db;
    QByteArray m_secretBoxKey;
    QByteArray m_secretBoxNonce;

    void loadOrCreateCryptoKeys();
};
