#include "RemoteCaptchaDataSource.h"
#include <thread>

void RemoteCaptchaDataSource::generateCaptchaAsync(std::function<void(std::expected<CaptchaData, QString>)> callback) {
    // 模拟网络请求
    std::thread([callback]() {
        // 生产环境应当通过 QNetworkAccessManager 等向服务端请求验证码，此处打桩
        if (callback) {
            callback(std::unexpected("Remote captcha service is not implemented yet."));
        }
    }).detach();
}
