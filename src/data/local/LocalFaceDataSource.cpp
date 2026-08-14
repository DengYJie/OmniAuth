#include "data/local/AntiSpoofingEngine.h"
#include "data/local/ArcFaceEngine.h"
#include "data/local/RetinaFaceEngine.h"
#include "LocalFaceDataSource.h"
#include <onnxruntime_cxx_api.h>
#include <QCoreApplication>
#include <QDir>

LocalFaceDataSource::LocalFaceDataSource(std::shared_ptr<UserRepository> userRepository)
    : m_userRepo(std::move(userRepository)),
    m_ortEnv(std::make_unique<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "OmniAuth")) {}

LocalFaceDataSource::~LocalFaceDataSource() = default;

bool LocalFaceDataSource::init() {
    try {
        QString basePath = QCoreApplication::applicationDirPath() + "/models/";
        m_retinaEngine = std::make_unique<RetinaFaceEngine>(*m_ortEnv, (basePath + "retinaface.onnx").toStdString());
        m_antiSpoofEngine = std::make_unique<AntiSpoofingEngine>(*m_ortEnv, (basePath + "minifasnet.onnx").toStdString());
        m_arcFaceEngine = std::make_unique<ArcFaceEngine>(*m_ortEnv, (basePath + "arcface.onnx").toStdString());
        reloadFaceFeatureCache();
        return true;
    }
    catch (const std::exception& e) {
        qWarning("Failed to initialize local face engines: %s", e.what());
        return false;
    }
}

void LocalFaceDataSource::reloadFaceFeatureCache() {
    if (!m_userRepo) return;

    m_userRepo->getAllFaceFeaturesAsync([this](std::vector<std::pair<int, std::vector<float>>> features) {
        m_featureCache = std::move(features);
        });
}
void LocalFaceDataSource::detectFaceAsync(const cv::Mat& frame, std::function<void(std::expected<FaceDetectionResult, AuthError>)> callback) {
    if (!m_retinaEngine) {
        if (callback) callback(std::unexpected(AuthError::ModelError));
        return;
    }
    auto res = m_retinaEngine->detect(frame);
    if (callback) callback(res);
}

void LocalFaceDataSource::checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold,
    std::function<void(std::expected<float, AuthError>)> callback) {
    if (!m_antiSpoofEngine) {
        if (callback) callback(std::unexpected(AuthError::ModelError));
        return;
    }
    auto res = m_antiSpoofEngine->checkLiveness(frame, faceBox, threshold);
    if (callback) callback(res);
}

void LocalFaceDataSource::extractFeatureAsync(const cv::Mat& alignedFace,
    std::function<void(std::expected<std::vector<float>, AuthError>)> callback) {
    if (!m_arcFaceEngine) {
        if (callback) callback(std::unexpected(AuthError::ModelError));
        return;
    }
    auto res = m_arcFaceEngine->extractFeature(alignedFace);
    if (callback) callback(res);
}

void LocalFaceDataSource::matchFeatureAsync(const std::vector<float>& feature, float threshold,
    std::function<void(bool matched, int uid)> callback) {
    if (feature.empty() || m_featureCache.empty()) {
        if (callback) callback(false, -1);
        return;
    }

    // Refresh cache before matching, to ensure we have latest (could optimize this later if needed)
    // Actually, in a real system we might hook up a signal or use a dirty flag.
    // For now, we will do the matching directly on the current cache.
    // A slight optimization is to reload if cache is empty, but we did it on init().

    float bestSimilarity = -1.0f;
    int bestMatchUid = -1;

    for (const auto& [uid, dbFeature] : m_featureCache) {
        if (dbFeature.size() != feature.size()) continue;

        float dotProduct = 0.0f;
        for (size_t i = 0; i < dbFeature.size(); ++i) {
            dotProduct += feature[i] * dbFeature[i];
        }

        if (dotProduct > bestSimilarity) {
            bestSimilarity = dotProduct;
            bestMatchUid = uid;
        }
    }

    if (bestSimilarity >= threshold && bestMatchUid != -1) {
        if (callback) callback(true, bestMatchUid);
    }
    else {
        if (callback) callback(false, -1);
    }
}
