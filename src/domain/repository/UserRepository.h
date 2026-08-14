#pragma once

#include <QString>
#include <QByteArray>
#include <optional>
#include <vector>
#include <functional>
#include "domain/entity/User.h"
#include "domain/model/UserAuthDTO.h"

/**
 * @brief 领域仓库接口：用户存储契约
 */
class UserRepository {
public:
    virtual ~UserRepository() = default;

    virtual bool init() = 0;
    virtual void close() = 0;

    // --- 用户基础信息 ---
    virtual void createUserAsync(const QString& username, const QString& pwd_hash, 
                                 const QString& email, const QString& phone,
                                 std::function<void(bool)> callback) = 0;
    
    // account 可以是 username, email, 或 phone
    virtual void getUserByAccountAsync(const QString& account, 
                                       std::function<void(std::optional<UserAuthDTO>)> callback) = 0;
                                       
    virtual void getUserByIdAsync(int uid, 
                                  std::function<void(std::optional<User>)> callback) = 0;
                                       
    virtual void updatePasswordAsync(int uid, const QString& newPwdHash, 
                                     std::function<void(bool)> callback) = 0;

    // --- 人脸凭证相关 ---
    virtual void updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                         std::function<void(bool)> callback) = 0;
    virtual void removeFaceEncodingAsync(int uid, std::function<void(bool)> callback) = 0;
    
    // account 可以是邮箱或手机号
    virtual void hasUserFaceAsync(const QString& account, std::function<void(bool)> callback) = 0;
    virtual void hasUserFaceAsync(int uid, std::function<void(bool)> callback) = 0;
    
    virtual void saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                          std::function<void(bool)> callback) = 0;
                                          
    // 纯 IO 接口，拉取全量人脸特征 (uid -> feature)
    virtual void getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback) = 0;
};
