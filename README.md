# OmniAuth

OmniAuth 是一个基于 **Qt 6 + C++** 的桌面端智能认证系统，提供账号密码登录、人脸识别登录、行为风控验证码、短信验证码登录与密码重置等能力。项目采用本地推理与本地数据存储，适合作为智能认证、桌面安全交互和本地 AI 集成的参考实现。

## 功能特性

- 账号密码登录
- 人脸识别登录
- 活体检测与人脸特征比对
- 行为式滑块验证码 / AI 风控辅助验证
- 短信验证码登录
- 密码重置
- SQLite 本地用户数据管理
- ONNX Runtime 本地模型推理
- Qt Widgets + 第三方 Fluent 风格桌面界面

## 技术栈

- **Language**: C++23
- **UI**: Qt 6 (`Core`, `Gui`, `Widgets`, `Sql`)
- **Build**: CMake + Ninja
- **Database**: SQLite
- **Computer Vision**: OpenCV
- **Inference**: ONNX Runtime
- **Crypto**: libsodium
- **UI Libraries**: FluentQt, QWindowKit
- **Model Tooling**: Python, PyTorch, scikit-learn, ONNX

## 核心能力

### 账号密码登录

基于本地 SQLite 用户库完成账号认证，密码校验使用 `libsodium crypto_pwhash_str`。

### 人脸识别登录

通过摄像头采集实时画面，结合 `RetinaFace`、`MiniFASNet` 与 `ArcFace` 完成人脸检测、活体检测和特征匹配。

### 行为风控验证

记录滑块拖动轨迹并提取行为特征，使用轻量级 ONNX 模型判断操作是否更接近真人行为。

## 项目结构

```text
src/
├─ ui/                  # 页面、窗口、ViewModel、导航与自定义控件
├─ domain/              # 实体、DTO、仓储接口、用例、行为模型
├─ data/
│  ├─ local/            # SQLite、本地推理引擎、本地数据源
│  ├─ remote/           # 远端数据源
│  ├─ repository/       # Repository 实现
│  └─ di/               # 依赖装配
├─ core/                # 加密与基础工具
res/
├─ models/              # ONNX 模型
└─ assets/              # 静态资源
scripts/                # 数据生成与模型训练脚本
```

关键入口：

- `src/main.cpp`
- `src/data/di/AppContainer.cpp`
- `src/ui/screen/main/AuthWindow.cpp`
- `src/ui/screen/main/MainWindow.cpp`

## 快速开始

### 配置

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DCMAKE_PREFIX_PATH="$env:QTDIR"
```

### 编译

```powershell
cmake --build build
```

### 运行

从构建输出目录启动可执行文件。构建完成后，模型文件与相关运行时库会被复制到输出目录。

## 模型文件

运行时依赖以下模型文件：

- `res/models/retinaface.onnx`
- `res/models/minifasnet.onnx`
- `res/models/arcface.onnx`
- `res/models/behavior_mlp.onnx`

## 数据存储

- 用户数据使用 SQLite 管理
- 运行时数据库位于 `QStandardPaths::AppDataLocation/omniauth_core.db`
- 手机号与人脸特征会先加密再存储
- 人脸特征保存在 `sys_users.face_encodings`

## 训练脚本

项目包含行为风控模型训练相关脚本：

- `scripts/generate_bot_samples.py`
- `scripts/train_behavior_mlp.py`

示例命令：

```powershell
python scripts/generate_bot_samples.py
python scripts/train_behavior_mlp.py
```

训练输出：

- `res/models/behavior_mlp.onnx`

## 说明

- 当前主流程默认以本地模式启动：`AppContainer::init(false)`
- 远端数据源模块已预留接口，当前以本地能力为主
- 项目目前未包含完整自动化测试与 CI 配置
