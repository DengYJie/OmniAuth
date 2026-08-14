#pragma once

#include "domain/repository/UserRepository.h"
#include <memory>

class UserRepositoryImpl : public UserRepository {
public:
    UserRepositoryImpl(std::unique_ptr<UserRepository> localSource,
                   std::unique_ptr<UserRepository> remoteSource);
    
    void setUseRemote(bool useRemote) { m_useRemote = useRemote; }

    bool init() override;
    void close() override;

    void createUserAsync(const QString& username, const QString& pwd_hash, 
                         const QString& email, const QString& phone,
                         std::function<void(bool)> callback);
    
    void getUserByAccountAsync(const QString& account, 
                               std::function<void(std::optional<UserAuthDTO>)> callback);
                               
    void getUserByIdAsync(int uid, 
                          std::function<void(std::optional<User>)> callback);
                                       
    void updatePasswordAsync(int uid, const QString& newPwdHash, 
                             std::function<void(bool)> callback);

    void updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                 std::function<void(bool)> callback);
    void removeFaceEncodingAsync(int uid, std::function<void(bool)> callback);
    // account 可以是邮箱或手机号
    void hasUserFaceAsync(const QString& account, std::function<void(bool)> callback);
    void hasUserFaceAsync(int uid, std::function<void(bool)> callback);
    
    void saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                  std::function<void(bool)> callback);
    void getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback);

private:
    std::unique_ptr<UserRepository> m_localSource;
    std::unique_ptr<UserRepository> m_remoteSource;
    bool m_useRemote = false;

    UserRepository* getSource() const {
        return m_useRemote ? m_remoteSource.get() : m_localSource.get();
    }
};
