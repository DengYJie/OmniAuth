#pragma once

#include "domain/repository/CaptchaRepository.h"
#include <memory>

/**
 * @brief 验证码服务 (业务服务层，负责图像算法与验证数据生成)
 */
class CaptchaService {
public:
    explicit CaptchaService(std::shared_ptr<CaptchaRepository> repository);
    
    void fetchCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback);

private:
    std::shared_ptr<CaptchaRepository> m_repository;
};
