# /// script
# requires-python = ">=3.9"
# dependencies = [
#     "pandas",
#     "numpy",
#     "torch",
#     "scikit-learn",
#     "joblib",
#     "onnx",
#     "onnxscript",
# ]
# ///
"""
train_behavior_mlp.py
=====================
使用 PyTorch 训练极轻量级 MLP（多层感知机）行为风控模型。
读取 `data/human.csv` (label=1) 和 `data/bot.csv` (label=0)，
进行训练与验证，最终导出为 ONNX 格式供 C++ (ONNX Runtime) 调用。
"""

import os
import pandas as pd
import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import Dataset, DataLoader
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from pathlib import Path
import joblib

# ==========================================
# 1. 超参数配置
# ==========================================
INPUT_DIM = 10
HIDDEN_1 = 16
HIDDEN_2 = 8
BATCH_SIZE = 16
EPOCHS = 100
LEARNING_RATE = 0.005

SCRIPT_DIR = Path(__file__).resolve().parent
PROJECT_ROOT = SCRIPT_DIR.parent
DATA_DIR = PROJECT_ROOT / "data"
MODEL_DIR = PROJECT_ROOT / "res" / "models"
MODEL_DIR.mkdir(parents=True, exist_ok=True)

ONNX_OUTPUT_PATH = MODEL_DIR / "behavior_mlp.onnx"
SCALER_OUTPUT_PATH = MODEL_DIR / "scaler.pkl" # 用于记录归一化参数，C++ 端也需要同样的归一化

# ==========================================
# 2. 定义极轻量级 MLP 模型
# ==========================================
class BehaviorMLP(nn.Module):
    def __init__(self, input_dim=INPUT_DIM):
        super(BehaviorMLP, self).__init__()
        # 极轻量级网络，确保极低的推理延迟
        self.net = nn.Sequential(
            nn.Linear(input_dim, HIDDEN_1),
            nn.ReLU(),
            nn.Linear(HIDDEN_1, HIDDEN_2),
            nn.ReLU(),
            nn.Linear(HIDDEN_2, 1),
            nn.Sigmoid() # 输出 0~1 的真实用户置信度
        )

    def forward(self, x):
        return self.net(x)

# ==========================================
# 3. 数据集定义
# ==========================================
class BehaviorDataset(Dataset):
    def __init__(self, X, y):
        self.X = torch.tensor(X, dtype=torch.float32)
        self.y = torch.tensor(y, dtype=torch.float32).unsqueeze(1) # 形状 [N, 1]

    def __len__(self):
        return len(self.X)

    def __getitem__(self, idx):
        return self.X[idx], self.y[idx]

def load_and_preprocess_data():
    bot_csv = DATA_DIR / "bot.csv"

    # 由于 C++ 端在执行时可能会把 human.csv 写入可执行文件所在目录（如 build/debug/data/）
    # 这里自动搜索 human.csv 的实际位置
    human_csv = None
    possible_human_paths = [
        DATA_DIR / "human.csv",
        PROJECT_ROOT / "build" / "debug" / "data" / "human.csv",
        PROJECT_ROOT / "build" / "Release" / "data" / "human.csv",
        PROJECT_ROOT / "build" / "data" / "human.csv"
    ]
    for path in possible_human_paths:
        if path.exists():
            human_csv = path
            break

    if human_csv is None or not bot_csv.exists():
        print(f"❌ 找不到数据文件！")
        print(f"请确保 bot_csv 存在: {bot_csv}")
        print(f"请确保 human_csv 存在（脚本已在 build/debug 等目录自动查找但未找到）。")
        return None, None, None, None, None

    # 读取数据
    df_human = pd.read_csv(human_csv)
    df_bot = pd.read_csv(bot_csv)

    print(f"✅ 载入数据: {len(df_human)} 条真人样本 (label=1), {len(df_bot)} 条机器样本 (label=0)")

    # 合并数据
    df = pd.concat([df_human, df_bot], ignore_index=True)

    # 提取特征和标签
    X = df.drop("label", axis=1).values
    y = df["label"].values

    # 划分训练集和测试集 (80% 训练, 20% 测试)
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)

    # 数据归一化 (StandardScaler: z = (x - u) / s)
    # 注意：C++ 推理时也需要用到这里的均值和标准差
    scaler = StandardScaler()
    X_train_scaled = scaler.fit_transform(X_train)
    X_test_scaled = scaler.transform(X_test)

    # 保存 scaler 参数供参考或供 C++ 端硬编码/读取
    joblib.dump(scaler, SCALER_OUTPUT_PATH)
    print(f"✅ 归一化参数已保存至 {SCALER_OUTPUT_PATH}")
    print(f"   Means: {scaler.mean_}")
    print(f"   Scales: {scaler.scale_}")

    return X_train_scaled, X_test_scaled, y_train, y_test, scaler

# ==========================================
# 4. 训练与评估
# ==========================================
def train():
    X_train, X_test, y_train, y_test, scaler = load_and_preprocess_data()
    if X_train is None:
        return

    train_dataset = BehaviorDataset(X_train, y_train)
    test_dataset = BehaviorDataset(X_test, y_test)

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    test_loader = DataLoader(test_dataset, batch_size=BATCH_SIZE, shuffle=False)

    model = BehaviorMLP()
    criterion = nn.BCELoss() # 二元交叉熵损失
    optimizer = optim.Adam(model.parameters(), lr=LEARNING_RATE)

    print("\n🚀 开始训练 MLP 模型...")
    for epoch in range(EPOCHS):
        model.train()
        epoch_loss = 0.0
        correct = 0
        total = 0

        for batch_X, batch_y in train_loader:
            optimizer.zero_grad()
            outputs = model(batch_X)
            loss = criterion(outputs, batch_y)
            loss.backward()
            optimizer.step()

            epoch_loss += loss.item()
            predicted = (outputs >= 0.5).float()
            total += batch_y.size(0)
            correct += (predicted == batch_y).sum().item()

        train_acc = 100 * correct / total

        # 每 10 轮输出一次信息
        if (epoch + 1) % 10 == 0:
            model.eval()
            val_loss = 0.0
            val_correct = 0
            val_total = 0
            with torch.no_grad():
                for batch_X, batch_y in test_loader:
                    outputs = model(batch_X)
                    loss = criterion(outputs, batch_y)
                    val_loss += loss.item()
                    predicted = (outputs >= 0.5).float()
                    val_total += batch_y.size(0)
                    val_correct += (predicted == batch_y).sum().item()
            
            val_acc = 100 * val_correct / val_total
            print(f"Epoch [{epoch+1:3d}/{EPOCHS}] "
                  f"Loss: {epoch_loss/len(train_loader):.4f} | "
                  f"Train Acc: {train_acc:.2f}% | "
                  f"Val Loss: {val_loss/len(test_loader):.4f} | "
                  f"Val Acc: {val_acc:.2f}%")

    print("\n🎉 训练完成！")

    # ==========================================
    # 5. 导出端到端 ONNX (包含数据归一化节点)
    # ==========================================
    class EndToEndBehaviorModel(nn.Module):
        def __init__(self, trained_mlp, scaler):
            super().__init__()
            self.mlp = trained_mlp
            # 保存为普通的 Python List，避免 register_buffer 触发外部数据分离 (.data)
            self.mean_list = scaler.mean_.tolist()
            self.scale_list = scaler.scale_.tolist()

        def forward(self, x):
            # 在前向传播时直接构造 Tensor，ONNX 导出时会将其折叠为内部常量 (Constant Node)
            device = x.device
            mean_tensor = torch.tensor(self.mean_list, dtype=torch.float32, device=device)
            scale_tensor = torch.tensor(self.scale_list, dtype=torch.float32, device=device)
            
            x_normalized = (x - mean_tensor) / scale_tensor
            return self.mlp(x_normalized)

    end_to_end_model = EndToEndBehaviorModel(model, scaler)
    end_to_end_model.eval()

    dummy_input = torch.randn(1, INPUT_DIM, dtype=torch.float32) # [batch_size=1, input_dim=10]
    
    torch.onnx.export(
        end_to_end_model, 
        dummy_input, 
        ONNX_OUTPUT_PATH,
        export_params=True,
        opset_version=14, # 选择合适的 opset 版本
        do_constant_folding=True,
        input_names=['input'],
        output_names=['output'],
        dynamic_axes={'input': {0: 'batch_size'}, 'output': {0: 'batch_size'}}
    )
    
    print(f"💾 端到端模型已初步导出至: {ONNX_OUTPUT_PATH}")
    
    # ==========================================
    # 6. 后处理: 合并外部数据，确保单文件输出
    # ==========================================
    import onnx
    import os
    print("正在将外部权重 (.data) 重新合并入单一 ONNX 文件...")
    try:
        onnx_model = onnx.load(ONNX_OUTPUT_PATH)
        # 覆盖保存，默认 save_as_external_data=False，将所有数据内联
        onnx.save(onnx_model, ONNX_OUTPUT_PATH)
        
        # 删除产生的 .data 文件
        data_file = str(ONNX_OUTPUT_PATH) + ".data"
        if os.path.exists(data_file):
            os.remove(data_file)
            print("清理了残留的 .data 文件。")
    except Exception as e:
        print(f"合并单文件失败: {e}")

    print("\n✅ C++ 端集成极简体验:")
    print("归一化 (StandardScaler) 已作为数学节点固化到 ONNX 模型内部。")
    print("在 C++ 端调用时，直接传入原始的 10 维特征数据即可，无需进行任何预处理！")

if __name__ == "__main__":
    train()
