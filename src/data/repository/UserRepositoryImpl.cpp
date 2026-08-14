#include "UserRepositoryImpl.h"

UserRepositoryImpl::UserRepositoryImpl(std::unique_ptr<UserRepository> localSource,
                               std::unique_ptr<UserRepository> remoteSource)
    : m_localSource(std::move(localSource)),
      m_remoteSource(std::move(remoteSource)) {}

bool UserRepositoryImpl::init() {
    bool ok = true;
    if (m_localSource) ok = m_localSource->init() && ok;
    if (m_remoteSource) ok = m_remoteSource->init() && ok;
    return ok;
}

void UserRepositoryImpl::close() {
    if (m_localSource) m_localSource->close();
    if (m_remoteSource) m_remoteSource->close();
}

void UserRepositoryImpl::createUserAsync(const QString& username, const QString& pwd_hash, 
                                     const QString& email, const QString& phone,
                                     std::function<void(bool)> callback) {
    getSource()->createUserAsync(username, pwd_hash, email, phone, std::move(callback));
}

void UserRepositoryImpl::getUserByAccountAsync(const QString& account, 
                                           std::function<void(std::optional<UserAuthDTO>)> callback) {
    getSource()->getUserByAccountAsync(account, std::move(callback));
}

void UserRepositoryImpl::getUserByIdAsync(int uid, 
                                      std::function<void(std::optional<User>)> callback) {
    getSource()->getUserByIdAsync(uid, std::move(callback));
}

void UserRepositoryImpl::updatePasswordAsync(int uid, const QString& newPwdHash, 
                                         std::function<void(bool)> callback) {
    getSource()->updatePasswordAsync(uid, newPwdHash, std::move(callback));
}

void UserRepositoryImpl::updateFaceEncodingAsync(int uid, const QByteArray& encryptedFaceData, 
                                             std::function<void(bool)> callback) {
    getSource()->updateFaceEncodingAsync(uid, encryptedFaceData, std::move(callback));
}

void UserRepositoryImpl::removeFaceEncodingAsync(int uid, std::function<void(bool)> callback) {
    getSource()->removeFaceEncodingAsync(uid, std::move(callback));
}

// account 可以是邮箱或手机号
void UserRepositoryImpl::hasUserFaceAsync(const QString& account, std::function<void(bool)> callback) {
    getSource()->hasUserFaceAsync(account, std::move(callback));
}

void UserRepositoryImpl::hasUserFaceAsync(int uid, std::function<void(bool)> callback) {
    getSource()->hasUserFaceAsync(uid, std::move(callback));
}

void UserRepositoryImpl::saveUserFaceFeatureAsync(int uid, const std::vector<float>& feature, 
                                              std::function<void(bool)> callback) {
    getSource()->saveUserFaceFeatureAsync(uid, feature, std::move(callback));
}

void UserRepositoryImpl::getAllFaceFeaturesAsync(std::function<void(std::vector<std::pair<int, std::vector<float>>>)> callback) {
    getSource()->getAllFaceFeaturesAsync(std::move(callback));
}
