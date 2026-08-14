#include "FaceAuthRepositoryImpl.h"

FaceAuthRepositoryImpl::FaceAuthRepositoryImpl(std::unique_ptr<FaceAuthRepository> localSource,
                                       std::unique_ptr<FaceAuthRepository> remoteSource)
    : m_localSource(std::move(localSource)), m_remoteSource(std::move(remoteSource)) {}

void FaceAuthRepositoryImpl::setUseRemote(bool useRemote) {
    m_useRemote = useRemote;
}

bool FaceAuthRepositoryImpl::init() {
    bool ok = true;
    if (m_localSource) ok = m_localSource->init() && ok;
    if (m_remoteSource) ok = m_remoteSource->init() && ok;
    return ok;
}

void FaceAuthRepositoryImpl::detectFaceAsync(const cv::Mat& frame, std::function<void(std::expected<FaceDetectionResult, AuthError>)> cb) {
    if (m_useRemote && m_remoteSource) {
        m_remoteSource->detectFaceAsync(frame, std::move(cb));
    } else if (m_localSource) {
        m_localSource->detectFaceAsync(frame, std::move(cb));
    } else if (cb) {
        cb(std::unexpected(AuthError::ModelError));
    }
}

void FaceAuthRepositoryImpl::checkLivenessAsync(const cv::Mat& frame, const FaceBox& faceBox, float threshold, std::function<void(std::expected<float, AuthError>)> cb) {
    if (m_useRemote && m_remoteSource) {
        m_remoteSource->checkLivenessAsync(frame, faceBox, threshold, std::move(cb));
    } else if (m_localSource) {
        m_localSource->checkLivenessAsync(frame, faceBox, threshold, std::move(cb));
    } else if (cb) {
        cb(std::unexpected(AuthError::ModelError));
    }
}

void FaceAuthRepositoryImpl::extractFeatureAsync(const cv::Mat& alignedFace, std::function<void(std::expected<std::vector<float>, AuthError>)> cb) {
    if (m_useRemote && m_remoteSource) {
        m_remoteSource->extractFeatureAsync(alignedFace, std::move(cb));
    } else if (m_localSource) {
        m_localSource->extractFeatureAsync(alignedFace, std::move(cb));
    } else if (cb) {
        cb(std::unexpected(AuthError::ModelError));
    }
}

void FaceAuthRepositoryImpl::matchFeatureAsync(const std::vector<float>& feature, float threshold, std::function<void(bool, int)> cb) {
    if (m_useRemote && m_remoteSource) {
        m_remoteSource->matchFeatureAsync(feature, threshold, std::move(cb));
    } else if (m_localSource) {
        m_localSource->matchFeatureAsync(feature, threshold, std::move(cb));
    } else if (cb) {
        cb(false, -1);
    }
}
