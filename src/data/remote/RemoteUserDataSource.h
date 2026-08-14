#pragma once

#include "domain/repository/UserRepository.h"
#include <QDebug>

/**
 * @brief 远程用户数据源 (Stub)
 */
class RemoteUserDataSource : public UserRepository {
public:
    RemoteUserDataSource() = default;
    ~RemoteUserDataSource() override = default;

    bool init() override {
        qDebug() << "[RemoteUserDataSource] Initialized (Stub)";
        return true;
    }
    
    void close() override {}

    void createUserAsync(const QString& username, const QString& pwd_hash, 
                         const QString& email, const QString& phone,
                         std::function<void(bool)> callback) override {
        callback(false);
    }
    
    void getUserByAccountAsync(const QString& account, 
                               std::function<void(std::optional<UserAuthDTO>)> callback) override {
        callback(std::nullopt);
    }
    
    void getUserByIdAsync(int uid, 
                          std::function<void(std::optional<User>)> callback) override {
        callback(std::nullopt);
    }
                                       
    void updatePasswordAsync(int uid, const QString& newPwdHash, 
                             std::function<void(bool)> callback) override {
        callback(false);
    }

    void updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                 std::function<void(bool)> callback) override {
        callback(false);
    }
    
    void removeFaceEncodingAsync(int uid, std::function<void(bool)> callback) override {
        callback(false);
    }
    
    void hasUserFaceAsync(const QString& account, std::function<void(bool)> callback) override {
        callback(false);
    }

    void hasUserFaceAsync(int uid, std::function<void(bool)> callback) override {
        callback(false);
    }

    void saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                  std::function<void(bool)> callback) override {
        callback(false);
    }
    
    void getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback) override {
        callback({});
    }
};
