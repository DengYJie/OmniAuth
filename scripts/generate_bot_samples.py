"""
generate_bot_samples.py
=======================
生成机器人滑块轨迹负样本数据 (label=0)，供 MLP 行为风控模型训练使用。

生成策略：
  1. 直线匀速滑动（50 条）：固定 Y，X 线性插值，时间戳均匀递增。
  2. 三次贝塞尔曲线滑动（50 条）：平滑曲线轨迹，Y 轴几乎无抖动。

输出格式与 C++ BehaviorTracker::extractFeatures() 完全对齐的 10 维特征 CSV。
"""

import csv
import math
import os
import random
from pathlib import Path

# ─── 10 维特征提取（与 C++ BehaviorTracker 完全一致） ───────────────────

def extract_features(trajectory: list[dict]) -> dict:
    """
    从 (x, y, timestamp) 轨迹序列提取 10 维行为特征。
    与 C++ BehaviorTracker::extractFeatures() 完全对齐。
    """
    n = len(trajectory)
    if n < 3:
        return {k: 0.0 for k in FEATURE_NAMES}

    # 1. 总体拖动耗时
    total_duration = trajectory[-1]["t"] - trajectory[0]["t"]

    # 速度序列 (slide(2) 等效)
    velocities_x = []
    for i in range(n - 1):
        dt = trajectory[i + 1]["t"] - trajectory[i]["t"]
        vx = (trajectory[i + 1]["x"] - trajectory[i]["x"]) / dt if dt > 0 else 0.0
        velocities_x.append(vx)

    if not velocities_x:
        return {k: 0.0 for k in FEATURE_NAMES}

    # 2. X轴最大速度
    max_speed_x = max(abs(v) for v in velocities_x)

    # 3. X轴平均速度
    avg_speed_x = sum(velocities_x) / len(velocities_x)

    # 4. Y轴位移方差
    y_values = [p["y"] for p in trajectory]
    y_mean = sum(y_values) / n
    var_y = sum((y - y_mean) ** 2 for y in y_values) / n

    # 5. 轨迹折返次数
    reversal_count = sum(1 for v in velocities_x if v < 0)

    # 6. 停顿次数 (|v| < 0.005)
    pause_threshold = 0.005
    pause_count = sum(1 for v in velocities_x if abs(v) < pause_threshold)

    # 7. X轴最大加速度
    max_accel_x = 0.0
    if len(velocities_x) >= 2:
        accels = [abs(velocities_x[i + 1] - velocities_x[i]) for i in range(len(velocities_x) - 1)]
        max_accel_x = max(accels) if accels else 0.0

    # 8. 速度变化熵值 (Shannon entropy, 10 buckets)
    speed_entropy = 0.0
    v_min = min(velocities_x)
    v_max = max(velocities_x)
    v_range = v_max - v_min
    num_buckets = 10
    if v_range > 1e-9:
        hist = [0] * num_buckets
        for v in velocities_x:
            idx = int((v - v_min) / v_range * (num_buckets - 1))
            idx = max(0, min(num_buckets - 1, idx))
            hist[idx] += 1
        total = len(velocities_x)
        for count in hist:
            if count > 0:
                p = count / total
                speed_entropy -= p * math.log2(p)

    # 9. 直线度比
    net_displacement = abs(trajectory[-1]["x"] - trajectory[0]["x"])
    total_path = 0.0
    for i in range(n - 1):
        dx = trajectory[i + 1]["x"] - trajectory[i]["x"]
        dy = trajectory[i + 1]["y"] - trajectory[i]["y"]
        total_path += math.sqrt(dx * dx + dy * dy)
    straightness_ratio = net_displacement / total_path if total_path > 1e-6 else 1.0

    # 10. 末段减速比
    tail_start = len(velocities_x) * 4 // 5
    end_slowdown_ratio = 1.0
    if tail_start < len(velocities_x) and abs(avg_speed_x) > 1e-6:
        tail_avg = sum(velocities_x[tail_start:]) / (len(velocities_x) - tail_start)
        end_slowdown_ratio = tail_avg / avg_speed_x

    return {
        "totalDuration": total_duration,
        "maxSpeedX": max_speed_x,
        "avgSpeedX": avg_speed_x,
        "varDisplacementY": var_y,
        "reversalCount": reversal_count,
        "pauseCount": pause_count,
        "maxAccelX": max_accel_x,
        "speedEntropy": speed_entropy,
        "straightnessRatio": straightness_ratio,
        "endSlowdownRatio": end_slowdown_ratio,
    }


FEATURE_NAMES = [
    "totalDuration", "maxSpeedX", "avgSpeedX", "varDisplacementY",
    "reversalCount", "pauseCount", "maxAccelX", "speedEntropy",
    "straightnessRatio", "endSlowdownRatio",
]


# ─── 轨迹生成策略 ──────────────────────────────────────────────────────

def generate_linear_trajectory(
    target_x: int = 180,
    y_base: int = 240,
    num_points: int = 30,
    duration_ms: int | None = None,
) -> list[dict]:
    """
    策略 1：直线匀速滑动（Y 完全固定，X 线性等分，时间均匀递增）。
    典型机器脚本特征：Y 方差 = 0, 折返 = 0, 速度熵极低, 直线度 ≈ 1.0
    """
    if duration_ms is None:
        duration_ms = random.randint(80, 300)  # 机器人通常很快

    trajectory = []
    for i in range(num_points):
        t_frac = i / (num_points - 1)
        x = int(t_frac * target_x)
        y = y_base  # 完美固定 Y
        t = int(t_frac * duration_ms)
        trajectory.append({"x": x, "y": y, "t": t})
    return trajectory


def cubic_bezier(t: float, p0: float, p1: float, p2: float, p3: float) -> float:
    """三次贝塞尔插值"""
    u = 1 - t
    return u**3 * p0 + 3 * u**2 * t * p1 + 3 * u * t**2 * p2 + t**3 * p3


def generate_bezier_trajectory(
    target_x: int = 180,
    y_base: int = 240,
    num_points: int = 40,
    duration_ms: int | None = None,
) -> list[dict]:
    """
    策略 2：三次贝塞尔曲线滑动。
    X 轴走贝塞尔平滑曲线（看似有速度变化但极度平滑），Y 轴几乎无抖动（±0~1px）。
    """
    if duration_ms is None:
        duration_ms = random.randint(200, 600)

    # 贝塞尔控制点 (X 方向)
    x_p0 = 0.0
    x_p1 = random.uniform(0.3, 0.5) * target_x   # 前1/3加速
    x_p2 = random.uniform(0.7, 0.9) * target_x   # 后1/3减速
    x_p3 = float(target_x)

    # 贝塞尔控制点 (时间方向 - 模拟匀速/加速)
    t_p1 = random.uniform(0.2, 0.4)
    t_p2 = random.uniform(0.6, 0.8)

    trajectory = []
    for i in range(num_points):
        u = i / (num_points - 1)
        x = int(cubic_bezier(u, x_p0, x_p1, x_p2, x_p3))
        # Y 轴仅有极微小偏移（模拟"聪明"的机器人但缺乏真人手抖）
        y = y_base + random.choice([0, 0, 0, 1, -1])
        t = int(cubic_bezier(u, 0.0, t_p1 * duration_ms, t_p2 * duration_ms, float(duration_ms)))
        trajectory.append({"x": x, "y": y, "t": t})
    return trajectory


# ─── 主程序 ─────────────────────────────────────────────────────────────

def main():
    script_dir = Path(__file__).resolve().parent
    project_root = script_dir.parent
    data_dir = project_root / "data"
    data_dir.mkdir(exist_ok=True)

    output_path = data_dir / "bot.csv"

    num_linear = 50
    num_bezier = 50

    samples = []

    # 生成直线匀速样本
    for _ in range(num_linear):
        target_x = random.randint(130, 230)
        num_pts = random.randint(20, 50)
        traj = generate_linear_trajectory(target_x=target_x, num_points=num_pts)
        feat = extract_features(traj)
        samples.append(feat)

    # 生成贝塞尔曲线样本
    for _ in range(num_bezier):
        target_x = random.randint(130, 230)
        num_pts = random.randint(25, 60)
        traj = generate_bezier_trajectory(target_x=target_x, num_points=num_pts)
        feat = extract_features(traj)
        samples.append(feat)

    # 写入 CSV
    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow(FEATURE_NAMES + ["label"])
        for feat in samples:
            row = [f"{feat[name]:.4f}" for name in FEATURE_NAMES] + ["0"]
            writer.writerow(row)

    print(f"✅ 成功生成 {len(samples)} 条机器人负样本 -> {output_path}")
    print(f"   - 直线匀速: {num_linear} 条")
    print(f"   - 贝塞尔曲线: {num_bezier} 条")

    # 打印前 3 条样本预览
    print("\n📊 样本预览 (前 3 条):")
    for i, feat in enumerate(samples[:3]):
        vals = ", ".join(f"{feat[k]:.3f}" for k in FEATURE_NAMES)
        print(f"  [{i}] {vals}")


if __name__ == "__main__":
    main()
