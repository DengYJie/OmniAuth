#pragma once

#include "domain/entity/User.h"
#include <QString>

/**
 * @brief 包含用户实体及鉴权凭证的数据传输对象 (Data Transfer Object)
 *
 * 作为领域仓库接口的契约类型，供 data 层实现与 domain 层业务代码使用。
 * 绝对不允许将包含此类敏感凭证的对象泄露到展示层 (UI/View)。
 */
struct UserAuthDTO {
    User user;
    QString pwdHash;
    QString salt;
    QString encPhone; // 加密存储的真实手机号
};
