# AYDevice Design

> **文档状态（2026-07-27）**：Phase-1/2/3 已落地——窗口 + 键鼠 + `InputMapping`（Action/Axis）+ 手柄（XInput，含震动）+ 触控（`WM_TOUCH`）+ IME 文本（`WM_CHAR`/`WM_IME_COMPOSITION`）+ 可重绑定 `InputProfile`（AYConfig `[Input.*]` 存档桥接）+ `DeviceSubSystem`（GameLoop 集成，独立 `AYDeviceSubSystem` 目标）。**下一薄能力**：`InputMapping` Action 按住时长（§6.3，未实现）。**Phase 4**：触控 `GestureRecognizer`（含 `LongPress`，§8.3）。**XR (OpenXR) 已移入未来引擎增强项**，见 §7。
> **输入栈归属**：键盘/鼠标/手柄、`InputMapping`、Action 查询 **均在 AYDevice**；**不**单独建设 `AYInput` 模块。见 §1.3。

## 1. 概述

AYDevice 是 AY Engine 的**设备子系统**，负责：
- 统一抽象键盘、鼠标、手柄、触控、VR 等输入设备
- **SDL2 窗口管理**（渲染器获取 windowHandle 的来源）
- Action/Axis 输入映射，解耦输入设备和游戏逻辑
- OpenXR 跨平台 VR 支持
- 输入配置、录制回放、手势识别等扩展功能

### 1.1 设计目标

- **统一抽象**：所有输入设备通过相同接口访问
- **窗口管理**：SDL2 Window 作为渲染器 bgfx 初始化的句柄来源
- **输入映射**：Action Mapping + Axis Mapping，支持玩家自定义按键
- **VR 支持**：OpenXR 跨平台标准（SteamVR/Meta Quest/WMR）
- **底层复用**：使用 SDL2/OpenXR，不重复造轮子

### 1.2 在引擎中的位置

```
┌─────────────────────────────────────────────────────────────────┐
│                        Game Engine                               │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────┐    ┌──────────────────────────────────────┐  │
│  │  AYEngine  │───▶│           AYDevice                     │  │
│  │  (引擎层)  │    │                                      │  │
│  └─────────────┘    │  ┌────────────────────────────────┐   │  │
│                      │  │ DeviceManager              │   │  │
│                      │  │  (统一入口，窗口+设备)        │   │  │
│                      │  └──────────────┬───────────────┘   │  │
│                      │                 │                      │  │
│                      │  ┌──────────────▼───────────────┐   │  │
│                      │  │       WindowManager          │   │  │
│                      │  │   (SDL2 窗口管理)            │   │  │
│                      │  └──────────────┬───────────────┘   │  │
│                      │                 │                      │  │
│                      │  ┌──────────────▼───────────────┐   │  │
│                      │  │    InputDevice (基类)       │   │  │
│                      │  │                             │   │  │
│                      │  │  Keyboard  Mouse  Gamepad    │   │  │
│                      │  │  Touch     XRDevice       │   │  │
│                      │  └──────────────┬───────────────┘   │  │
│                      │                 │                      │  │
│                      │  ┌──────────────▼───────────────┐   │  │
│                      │  │      InputMapping           │   │  │
│                      │  │  (Action / Axis 绑定)      │   │  │
│                      │  └─────────────────────────────┘   │  │
│                      └──────────────────────────────────────┘  │
│                                    │                            │
│                         ┌──────────▼──────────┐                │
│                         │   AYEventSystem   │                │
│                         │   (事件分发)       │                │
│                         └──────────────────┘                │
│                                                                  │
│                         ┌──────────▼──────────┐                │
│                         │   AYRenderer      │                │
│                         │   (获取 windowHandle) │                │
│                         └──────────────────┘                │
└─────────────────────────────────────────────────────────────────┘
```

### 1.3 与 AYInput 的关系（锁定 2026-07-11）

**结论：不建设独立 `AYInput` 模块；与 AYDevice 不存在并行实现。**

| 项 | 决策 |
|----|------|
| **`AYRuntime/AYInput/`** | **废弃** — 仓库内仅空 `.git/` 占位，无 CMake/头文件/实现；**不要**新建该目录下的输入库 |
| **输入能力归属** | 全部在 **AYDevice**：原始设备轮询 → `InputMapping`（Action/Axis）→ `InputState` 快照 |
| **GameLoop 子系统** | **`DeviceSubSystem` 唯一** — 每帧 `pollEvents` + 更新 `InputState`；**取消**独立的 `InputSubSystem` 概念（见 `AYApplication/design.md` §3.2） |
| **消费方** | `AYUI` EventBridge、`AYScript` `InputProvider`、`AYEditor` Play 路由 — 均读 **AYDevice** 的 Action/状态 API，不直连 SDL/Win32 |
| **Logia `input.is_pressed("jump")`** | 字符串 = **Action 名**（如 `"jump"`），由 `InputMapping` 绑定物理键；映射表可走 `AYConfig` `[Input.Actions]` 或项目 JSON |

**分层（避免重复造轮子）**：

```
平台 (SDL2 / Win32 message pump)
    → AYDevice 原始设备 (KeyboardDevice / MouseDevice / …)
    → InputMapping (Action "Jump" → Space / Gamepad_A)
    → InputState / query API
    → 消费方 (AYUI / Logia / Editor)
```

**实现分期**（与 §12 一致）：

| Phase | AYDevice 交付 | 备注 |
|-------|---------------|------|
| **Phase-1（当前）** | `WindowManager` + `pollEvents`（窗口事件） | 无键盘/映射 |
| **Phase-2** | `KeyboardDevice` + `MouseDevice` + `InputMapping` | Logia INT-02 依赖此阶段 |
| **Phase-3** | Gamepad(XInput) / Touch(WM_TOUCH) / TextInput(IME) / InputProfile(+AYConfig) | **已完成**（除 XR） |
| **下一薄能力（H0）** | `InputMapping` Action Hold（§6.3） | 按住时长 + 可选阈值边沿；先于完整手势 |
| **Phase-4** | `GestureRecognizer` / InputRecorder / AimAssistance / MotionInput | 触控手势含 LongPress |
| **未来增强** | XR (OpenXR) | 依赖 AYRenderer XR 呈现 + headset，见 §7 |

**明确不做**：

- 平行的 `AYInput` 静态库或第二套 Action 映射表
- 在 `AYScript` / `AYUI` 内嵌 SDL 键盘轮询（必须经 AYDevice）

---

## 2. 核心架构

### 2.1 架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                      DeviceManager                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  WindowManager (SDL2 窗口管理)                              │
│      │                                                        │
│      ├── createWindow() → SDL_Window*                        │
│      ├── getWindowHandle() → void* (hwnd)                    │
│      ├── setWindowTitle() / setWindowSize()                  │
│      └── window events (resize, close, focus)               │
│                                                              │
│  InputDevice (基类)                                         │
│      │                                                        │
│      ├── KeyboardDevice  (SDL2)                             │
│      ├── MouseDevice    (SDL2)                              │
│      ├── GamepadDevice  (SDL2 Gamepad API)                  │
│      ├── TouchDevice    (SDL2 Touch)                        │
│      └── XRDevice      (OpenXR - VR 手柄/头显追踪)            │
│                                                              │
│  InputMapping                                                │
│      │                                                        │
│      ├── ActionBinding  ("Jump" → [Space, Gamepad_A, XR_A])   │
│      ├── AxisBinding     ("MoveX" → [A/D, Gamepad_LX])         │
│      └── ActionHold      (按住时长 / 可选阈值边沿，§6.3)       │
│                                                              │
│  InputState                                                 │
│      │                                                        │
│      └── 当前帧的输入状态快照（用于游戏逻辑查询）                 │
│                                                              │
│  Features (扩展)                                             │
│      │                                                        │
│      ├── InputRecorder (录制/回放)                          │
│      ├── TextInput (IME 文本输入)                            │
│      ├── GestureRecognizer (触控手势：Tap/LongPress/Swipe…)  │
│      ├── InputProfile (玩家配置)                            │
│      ├── HapticFeedback (触觉反馈)                           │
│      └── AimAssistance (瞄准辅助)                           │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 窗口管理 (WindowManager)

### 3.1 设计原因

SDL2 同时管理窗口和输入事件。为了让渲染器获取 `windowHandle`（bgfx 初始化所需），窗口应由 AYDevice 统一管理，而不是在 AYRenderer 中重复创建 SDL2 窗口。

### 3.2 接口设计

```cpp
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // ========== 创建/销毁 ==========

    // 创建窗口（由 Engine 层调用一次）
    bool createWindow(const WindowCreateInfo& info);
    void destroyWindow();

    // ========== 查询 ==========

    bool isWindowValid() const { return m_window != nullptr; }
    SDL_Window* getSDLWindow() const { return m_window; }

    // 获取窗口句柄（用于 bgfx 初始化）
    void* getWindowHandle() const;  // Windows: HWND, macOS: NSWindow*, Linux: Window*

    // 窗口尺寸
    int getWidth() const;
    int getHeight() const;
    void getSize(int& width, int& height) const;

    // ========== 设置 ==========

    void setTitle(const char* title);
    void setSize(int width, int height);
    void setFullscreen(bool enabled);
    void setResizable(bool resizable);

    // ========== 事件回调 ==========

    using WindowCloseCallback = std::function<void()>;
    using WindowResizeCallback = std::function<void(int width, int height)>;
    using WindowFocusCallback = std::function<void(bool focused)>;

    void setWindowCloseCallback(WindowCloseCallback cb);
    void setWindowResizeCallback(WindowResizeCallback cb);
    void setWindowFocusCallback(WindowFocusCallback cb);

    // ========== 事件处理（内部） ==========

    // 处理 SDL 窗口事件（由 pollEvents 调用）
    void processWindowEvent(const SDL_Event& event);

private:
    SDL_Window* m_window = nullptr;
    WindowCloseCallback m_windowCloseCallback;
    WindowResizeCallback m_windowResizeCallback;
    WindowFocusCallback m_windowFocusCallback;
};

struct WindowCreateInfo {
    const char* title = "AY Engine";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    bool resizable = true;
    int minWidth = 800;
    int minHeight = 600;
};
```

### 3.3 获取窗口句柄

```cpp
void* WindowManager::getWindowHandle() const {
    if (!m_window) return nullptr;
    return (void*)SDL_GetWindowData(m_window, "HWND");  // 自定义数据
}

// 或者直接用 SDL_GetWindowWMInfo
void* WindowManager::getWindowHandle() const {
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(m_window, &wmInfo);
    return (void*)wmInfo.info.win.window;  // Windows
}
```

---

## 4. 输入设备接口

### 4.1 输入设备基类

```cpp
class IInputDevice {
public:
    virtual ~IInputDevice() = default;

    // 轮询设备状态（每帧调用）
    virtual void poll() = 0;

    // 连接状态
    virtual bool isConnected() const = 0;

    // 设备类型
    virtual const char* deviceType() const = 0;
};
```

### 4.2 键盘设备

```cpp
class KeyboardDevice : public IInputDevice {
public:
    // 基础查询
    bool isKeyPressed(KeyCode key) const;
    bool isKeyJustPressed(KeyCode key) const;  // 本帧刚按下
    bool isKeyJustReleased(KeyCode key) const;   // 本帧刚释放

    // 模拟量（用于模拟按键）
    float getAnalogValue(KeyCode key) const;  // 0.0 ~ 1.0

    // 所有按键状态快照
    std::span<const uint8_t> getKeyStates() const;
};
```

### 4.3 鼠标设备

```cpp
class MouseDevice : public IInputDevice {
public:
    // 位置
    FVector2 getPosition() const;
    FVector2 getDelta() const;           // 本帧移动量
    FVector2 getPositionInWindow() const;  // 窗口坐标

    // 滚轮
    float getWheelDelta() const;

    // 按钮
    bool isButtonPressed(MouseButton btn) const;
    bool isButtonJustPressed(MouseButton btn) const;
    bool isButtonJustReleased(MouseButton btn) const;
};
```

### 4.4 手柄设备

```cpp
class GamepadDevice : public IInputDevice {
public:
    // 摇杆
    FVector2 getLeftStick() const;
    FVector2 getRightStick() const;

    // 扳机
    float getLeftTrigger() const;
    float getRightTrigger() const;

    // 按钮
    bool isButtonPressed(GamepadButton btn) const;
    bool isButtonJustPressed(GamepadButton btn) const;

    // 手柄信息
    const char* getDeviceName() const;
    int getDeviceIndex() const;
    bool isVibrationSupported() const;

    // 触觉反馈
    void setVibration(float lowFreq, float highFreq, float durationMs);
    void setVibrationLeft(float intensity, float durationMs);
    void setVibrationRight(float intensity, float durationMs);
};
```

### 4.5 触控设备

```cpp
class TouchDevice : public IInputDevice {
public:
    // 触控点
    int getTouchCount() const;

    struct TouchPoint {
        int id;              // 触控点 ID
        FVector2 position;   // 位置
        FVector2 delta;      // 本帧移动量
        float pressure;       // 压力 (0.0 ~ 1.0)
        TouchPhase phase;     // Began/Moved/Ended/Cancelled
    };

    const TouchPoint* getTouch(int index) const;
    const TouchPoint* getTouchById(int id) const;
};
```

---

## 5. 设备管理器 (DeviceManager)

### 5.1 接口设计

```cpp
class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // ========== 初始化 ==========

    // 初始化（创建窗口 + 初始化 SDL2）
    bool initialize(const DeviceConfig& config);
    void shutdown();

    // ========== 窗口访问 ==========

    WindowManager* window() { return &m_windowManager; }
    const WindowManager* window() const { return &m_windowManager; }

    // ========== 设备管理 ==========

    void addDevice(std::unique_ptr<IInputDevice> device);
    void removeDevice(IInputDevice* device);
    template<typename T>
    T* getDevice();

    // ========== 映射设置 ==========

    void setMapping(InputMapping* mapping);

    // ========== 轮询（每帧调用） ==========

    // 处理 SDL 事件 + 轮询设备
    void pollEvents();
    void pollDevices();

    // ========== 查询 ==========

    bool isActionPressed(const char* action) const;
    float getAxisValue(const char* axis) const;

    // ========== 快捷方式 ==========

    KeyboardDevice* keyboard() { return getDevice<KeyboardDevice>(); }
    MouseDevice* mouse() { return getDevice<MouseDevice>(); }
    GamepadDevice* gamepad(int index = 0);
    XRDevice* xr() { return getDevice<XRDevice>(); }

private:
    WindowManager m_windowManager;
    std::vector<std::unique_ptr<IInputDevice>> m_devices;
    InputMapping* m_mapping = nullptr;
};

struct DeviceConfig {
    // 窗口配置
    WindowCreateInfo window;

    // 输入配置
    bool enableKeyboard = true;
    bool enableMouse = true;
    bool enableGamepad = true;
    bool enableTouch = false;
    bool enableXR = false;
};
```

---

## 6. 输入映射

### 6.1 设计原因

```
不用 Mapping:
game.onKeyPressed(KEY_SPACE) → jump()

用 Mapping:
game.onAction("Jump") → jump()

好处：
├── 同一动作可绑定多个按键
├── 玩家可自定义按键
├── 游戏逻辑与具体按键解耦
└── VR 手柄也能绑定同一 Action
```

### 6.2 接口设计

```cpp
class InputMapping {
public:
    // ================ Action 绑定 ================
    // Action = 二值动作 (按下/释放)

    struct ActionBinding {
        std::string actionName;
        std::vector<KeyCode> keyboardKeys;
        std::vector<GamepadButton> gamepadButtons;
        std::vector<const char*> xrActions;  // OpenXR action path
    };

    void bindAction(const char* action,
                    std::span<const KeyCode> keys = {},
                    std::span<const GamepadButton> buttons = {},
                    std::span<const char* const> xrActions = {});

    // ================ Axis 绑定 ================
    // Axis = 连续值 (-1.0 ~ 1.0 或 0.0 ~ 1.0)

    struct AxisBinding {
        std::string axisName;

        struct KeyPair { KeyCode negative; KeyCode positive; };
        std::vector<KeyPair> keyboardKeys;  // 如 A/D

        std::vector<GamepadButton> gamepadPositive;  // 如 Gamepad_Right → 正向
        std::vector<GamepadButton> gamepadNegative; // 如 Gamepad_Left → 负向

        std::vector<const char*> xrActions;

        float sensitivity = 1.0f;
        float deadzone = 0.0f;  // 死区
    };

    void bindAxis(const char* axis,
                  std::span<const AxisBinding::KeyPair> keys = {},
                  std::span<const GamepadButton> gamepadPositive = {},
                  std::span<const GamepadButton> gamepadNegative = {},
                  std::span<const char* const> xrActions = {});

    // ================ 查询 ================
    bool isActionPressed(const char* action) const;
    bool isActionJustPressed(const char* action) const;
    bool isActionJustReleased(const char* action) const;
    float getAxisValue(const char* axis) const;

    // ================ Action 按住时长（§6.3，待实现） ================
    // void updateHoldTimers(float deltaSeconds);   // 由 DeviceManager / DeviceSubSystem 每帧调用
    // float getActionHoldTime(const char* action) const;
    // bool  isActionHeld(const char* action, float thresholdSeconds) const;
    // bool  isActionHoldJustTriggered(const char* action, float thresholdSeconds) const;

    // ================ 内部 ================
    void registerDevice(IInputDevice* device);
    void unregisterDevice(IInputDevice* device);

private:
    std::unordered_map<std::string, ActionBinding> m_actionBindings;
    std::unordered_map<std::string, AxisBinding> m_axisBindings;
    std::vector<IInputDevice*> m_devices;
};
```

### 6.3 Action 按住时长（Hold）— 待实现

> **状态（2026-07-27）**：设计锁定，**未实现**。与 §8.3 `GestureRecognizer` 分工见下。

#### 6.3.1 为什么进引擎层

短按 / 长按 / 蓄力是跨项目高频需求。若玩法层各自对物理键计时，换绑（`InputProfile`）后易与 Action 源脱节。Hold 挂在 **已解析的 Action 按下态** 上，换绑自动生效。

#### 6.3.2 与触控手势的边界（锁定）

| 能力 | 归属 | 输入源 | 语义 |
|------|------|--------|------|
| Action Hold | **`InputMapping`（§6.3）** | 键盘 / 鼠标按钮 / 手柄按钮（经 Action 绑定） | 可重绑定逻辑动作的按住秒数与阈值边沿 |
| `GestureRecognizer` | **Features（§8.3）** | 触控点流（`TouchDevice`） | Tap / DoubleTap / LongPress / Swipe / Pinch / Rotate |

**不做**：把每个 Action 强制二分成「短按 Action / 长按 Action」；阈值与「松开结算 vs 按住持续触发」属玩法策略，由查询 API 组合，不写死绑定表。

#### 6.3.3 目标 API

```cpp
// 每帧在设备轮询之后、玩法查询之前调用（DeviceSubSystem::update 内）。
void updateHoldTimers(float deltaSeconds);

// 当前连续按住时长（秒）。Action 未按下时为 0。
// JustReleased 当帧：仍返回「刚结束的那次按住」的总时长（便于短按判定），
// 下一帧若未再按下则清零。
float getActionHoldTime(std::string_view action) const;

// 是否已按住超过 threshold（持续为 true，直到松开）。
bool isActionHeld(std::string_view action, float thresholdSeconds) const;

// 本帧刚跨越 threshold 的边沿（长按「触发一次」）。
// 同一按住周期内只触发一次；松开后下次再按可再次触发。
bool isActionHoldJustTriggered(std::string_view action, float thresholdSeconds) const;
```

实现要点：

- 计时对象是 **Action 聚合按下态**（所有绑定源 OR），不是单个物理键。
- `thresholdSeconds` **按查询传入**，不存进 `ActionBinding`（同一 Action 可被 UI 用 0.25s、技能用 0.5s）。
- 可选后续（非本步范围）：`setActionDefaultHoldThreshold` 仅作便利默认值，不得替代查询参数。

#### 6.3.4 玩法组合约定

```text
短按：isActionJustReleased(A) && getActionHoldTime(A) < T
长按触发一次：isActionHoldJustTriggered(A, T)
长按持续：isActionHeld(A, T)           // 或 getActionHoldTime(A) >= T
蓄力条：Pressed 期间读 getActionHoldTime(A)
```

现有 `isActionPressed` / `JustPressed` / `JustReleased` **保持不变**；Hold 为叠加查询，不改变边沿语义。

#### 6.3.5 实现分期

| 步 | 交付 | 状态 |
|----|------|------|
| **H0（下一薄能力）** | `updateHoldTimers` + `getActionHoldTime` + `isActionHeld` + `isActionHoldJustTriggered`；单测覆盖跨帧 / 换绑 / JustReleased 当帧时长 | 待做 |
| **H1** | （可选）Logia / `InputProvider` 暴露同名查询 | 待做 |
| **G0** | §8.3 `GestureRecognizer` 触控 LongPress 等 | Phase 4，独立于 H0 |

---

## 7. VR 支持 (OpenXR)

> **状态（2026-07-11）：已移入「未来引擎增强项」，不在当前开发进程内。**
>
> 原因：OpenXR 与其余输入设备不同量级——它需要独占 frame loop（`xrWaitFrame`/`xrBeginFrame`/`xrEndFrame`），`xrCreateSession` **强依赖图形后端绑定**（D3D11/Vulkan device），且必须和 AYRenderer 的 swapchain 呈现路径交织（§7.3 的 `acquireSwapchainImage`/`submitFrame`）。当前 AYRenderer 仍在 bgfx 阶段，无 XR 呈现路径；且无 headset runtime 时 `xrGetSystem` 即失败，无法端到端验证。
>
> **前置条件（满足后再启动）**：① AYRenderer 具备 XR swapchain 双眼呈现；② 有可用 headset runtime（SteamVR/Oculus/WMR）用于验证。届时 `openxr-loader`（vcpkg 端口 1.1.54 可用）经 `AY_DEVICE_USE_OPENXR` 开关引入。
>
> 本节 §7.1–§7.4 为**目标接口设计存档**，非当前待办。

### 7.1 OpenXR 简介

OpenXR 是跨平台 VR 标准，一次开发支持多平台：

| 平台 | 支持 |
|------|------|
| SteamVR (Valve Index) | ✅ |
| Meta Quest | ✅ |
| Windows Mixed Reality | ✅ |

### 7.2 XR 追踪器角色

```cpp
struct XRTracker {
    enum class Role {
        Headset,          // 头显
        LeftController,    // 左手柄
        RightController,   // 右手柄
        LeftHand,          // 左手追踪
        RightHand          // 右手追踪
    };

    Role role;
    bool isTracked = false;
    bool isConnected = false;

    // 变换
    FVector3 position;
    FQuaternion rotation;
    FVector3 linearVelocity;
    FVector3 angularVelocity;

    // 按钮状态
    static constexpr int MAX_BUTTONS = 32;
    bool buttons[MAX_BUTTONS];
    float triggers[2];        // 扳机
    FVector2 thumbsticks[2]; // 摇杆

    // 触觉反馈
    void hapticPulse(float intensity, float durationMs);
};
```

### 7.3 XR 设备接口

```cpp
class XRDevice : public IInputDevice {
public:
    // ================ 追踪 ================
    const XRTracker& getTracker(XRTracker::Role role) const;
    std::span<const XRTracker> getAllTrackers() const;

    // ================ 会话管理 ================
    bool isSessionRunning() const;
    bool startSession(const XRSessionCreateInfo& info);
    void endSession();

    // ================ 渲染（VR 需要特殊处理）============
    // 获取下一帧的交换链图像
    XRSwapchainImage acquireSwapchainImage();
    // 提交渲染好的图像
    void submitFrame(const XRSwapchainImage& image, XRTracker::Role eye);

    // ================ 参考空间 ================
    void setReferenceSpace(const char* spaceName);  // "Local", "Stage"

    // ================ 输入 Action ================
    // OpenXR Action 绑定到输入
    bool isActionPressed(const char* action) const;
    bool isActionJustPressed(const char* action) const;
    float getActionValue(const char* action) const;
    FVector2 getActionVector2(const char* action) const;

    // ================ 生命周期 ================
    const char* deviceType() const override { return "XR"; }
    void poll() override;
    bool isConnected() const override;
};
```

### 7.4 OpenXR 初始化流程

```cpp
// 初始化示例
bool initXR(XRDevice* device) {
    // 1. 创建会话
    XRSessionCreateInfo info;
    info.graphicsPlugin = "D3D11"; // 或 "Vulkan"
    info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    if (!device->startSession(info)) {
        return false;
    }

    // 2. 设置参考空间
    device->setReferenceSpace("Local");

    // 3. 创建 Action (可在配置文件中定义)
    // createAction("Grab", XR_ACTION_TYPE_BOOLEAN_INPUT);
    // createAction("Trigger", XR_ACTION_TYPE_FLOAT_INPUT);
    // createAction("Pose", XR_ACTION_TYPE_POSE_INPUT);

    return true;
}
```

---

## 8. 扩展功能

### 8.1 输入录制与回放

```cpp
// 用于自动化测试、回放、系统诊断
class InputRecorder {
public:
    enum class Mode { Disabled, Recording, Playing };

    // 录制
    void startRecording(const char* filename);
    void stopRecording();
    bool isRecording() const { return m_mode == Mode::Recording; }

    // 回放
    void startPlayback(const char* filename);
    void stopPlayback();
    bool isPlaying() const { return m_mode == Mode::Playing; }
    void setPlaybackPosition(float timeInSeconds);
    float getPlaybackPosition() const;

    // 帧精确回放
    void setFrame(uint32_t frame);
    uint32_t getCurrentFrame() const { return m_currentFrame; }

private:
    Mode m_mode = Mode::Disabled;
    std::string m_filename;
    uint32_t m_currentFrame = 0;

    std::vector<InputFrame> m_recordedFrames;
};
```

### 8.2 文本输入 (IME 支持)

```cpp
// 支持中文、日文等 IME 输入
class TextInput {
public:
    // 开始/结束组合
    void beginComposition();
    void commitComposition();
    void cancelComposition();

    // 组合状态
    bool isComposing() const { return m_isComposing; }
    const char* getCompositionString() const { return m_composition.c_str(); }
    int getCompositionCursor() const { return m_cursorPos; }

    // 事件回调
    std::function<void(const char*)> onCommit;      // 组合完成
    std::function<void(const char*, int)> onComposition;  // 组合中

private:
    bool m_isComposing = false;
    std::string m_composition;
    int m_cursorPos = 0;
};
```

### 8.3 手势识别（触控）

> **范围**：仅消费 `TouchDevice` 触控点流。键盘 / 鼠标 / 手柄上的「长按」走 **`InputMapping` Action Hold（§6.3）**，不经本类。
>
> **状态**：Phase 4，未实现。触控 `LongPress` 与 Action Hold 可并存（例如 UI 触控长按菜单 vs 手柄 `Interact` 长按）；二者阈值独立配置。

```cpp
class GestureRecognizer {
public:
    enum class Gesture {
        None,
        Tap,                // 点击
        DoubleTap,          // 双击
        LongPress,         // 长按（触控）
        SwipeLeft, SwipeRight, SwipeUp, SwipeDown,  // 滑动
        Pinch,             // 缩放
        Rotate             // 旋转
    };

    void reset();

    // 添加触控点
    void addTouchPoint(int id, const FVector2& pos);
    void updateTouchPoint(int id, const FVector2& pos);
    void removeTouchPoint(int id);

    // 可选：长按阈值（秒），默认实现自定；与 §6.3 Action Hold 的 threshold 无关
    void setLongPressThreshold(float seconds);

    // 查询
    Gesture getRecognizedGesture() const { return m_gesture; }
    FVector2 getGestureDirection() const { return m_direction; }
    float getGestureVelocity() const { return m_velocity; }
    float getPinchScale() const { return m_pinchScale; }
    float getRotationAngle() const { return m_rotationAngle; }

private:
    Gesture m_gesture = Gesture::None;
    FVector2 m_direction;
    float m_velocity = 0.0f;
    float m_pinchScale = 1.0f;
    float m_rotationAngle = 0.0f;
    float m_longPressThreshold = 0.5f;

    std::unordered_map<int, FVector2> m_touchPoints;
};
```

### 8.4 玩家配置

```cpp
// 玩家可自定义按键
class InputProfile {
public:
    std::string name;  // "默认", "左手方案"

    // 加载/保存
    bool saveToFile(const char* path) const;
    bool loadFromFile(const char* path);

    // Action 绑定重映射
    void rebindAction(const char* action,
                      std::span<const KeyCode> keys,
                      std::span<const GamepadButton> buttons = {},
                      std::span<const char* const> xrActions = {});

    // 导出为默认
    static InputProfile getDefault();
};
```

### 8.5 触觉反馈

```cpp
class HapticFeedback {
public:
    // 恒定震动
    void setConstant(float intensity);  // 0.0 ~ 1.0

    // 振动 (手柄特有)
    void setRumble(float lowFreq, float highFreq);  // 低频 + 高频

    // 脉冲
    void pulse(float intensity, float durationMs);

    // 梯度震动
    void setFriction(float resistance);  // 阻力感
    void setSpring(float stiffness, float damping);
};
```

### 8.6 瞄准辅助

```cpp
// 射击游戏用
class AimAssistance {
public:
    // 吸附
    void setSnapAngle(float degrees);       // 吸附角度
    void setSnapDistance(float worldUnits); // 吸附距离
    void setSnapEnabled(bool enabled);

    // 平滑
    void setAimSmoothing(float factor);    // 0.0 ~ 1.0

    // 后坐力补偿
    void setRecoilCompensation(bool enabled);
    void addRecoil(const FVector2& recoil);

    // 应用到输入
    FVector2 apply(const FVector2& rawInput) const;
};
```

### 8.7 运动输入

```cpp
// 加速度计、陀螺仪（移动端、Joy-Con、VR 控制器）
class MotionInput {
public:
    // 当前状态
    FVector3 getAcceleration() const;
    FVector3 getAngularVelocity() const;
    FVector3 getOrientation() const;  // 欧拉角

    // 启用/禁用
    void enableMotion(bool enable);
    bool isMotionEnabled() const { return m_enabled; }

private:
    bool m_enabled = false;
};
```

---

## 9. 事件系统

### 9.1 输入事件

```cpp
// 通过 AYEventSystem 分发

struct InputActionEvent {
    std::string actionName;
    bool pressed;  // true=按下，false=释放
    float analogValue;  // 模拟量 (0.0 ~ 1.0)
};

struct InputAxisEvent {
    std::string axisName;
    float value;  // -1.0 ~ 1.0 或 0.0 ~ 1.0
};

struct InputKeyEvent {
    KeyCode key;
    bool pressed;
    bool justPressed;
    bool justReleased;
};

struct InputMouseEvent {
    MouseButton button;
    FVector2 position;
    FVector2 delta;
    float wheelDelta;
};

struct InputTouchEvent {
    int touchId;
    TouchDevice::TouchPoint touch;
    TouchPhase phase;
};

struct XRTrackerEvent {
    XRTracker::Role role;
    bool connected;
    bool tracked;
    const XRTracker* tracker;
};
```

---

## 10. 与其他模块的接口

### 10.1 与 AYRenderer

```cpp
// AYDevice 提供窗口句柄，AYRenderer 使用它初始化 bgfx

class DeviceManager {
    WindowManager* window() { return &m_windowManager; }
};

// AYRenderer 初始化
bool AYRenderer::initialize(const RendererSettings& settings) {
    // 从 AYDevice 获取窗口句柄
    auto* deviceManager = AYEngine::instance().device();
    auto* window = deviceManager->window();

    // bgfx 初始化
    bgfx::Init init;
    init.type = settings.backend;
    init.platformData.ndt = nullptr;
    init.platformData.nwh = window->getWindowHandle();  // 来自 AYDevice
    init.resolution.width = window->getWidth();
    init.resolution.height = window->getHeight();

    if (!bgfx::init(init)) {
        return false;
    }

    // ...
}
```

### 10.2 与 AYEventSystem

```cpp
// 输入事件通过 AYEventSystem 分发
class DeviceManager {
    void pollDevices() {
        // 分发 Action 事件
        if (isActionJustPressed("Jump")) {
            EventBus::send(InputActionEvent{"Jump", true, 1.0f});
        }

        // 分发 Axis 事件
        float moveX = getAxisValue("MoveX");
        if (moveX != 0.0f) {
            EventBus::send(InputAxisEvent{"MoveX", moveX});
        }
    }
};
```

### 10.4 与 AYEditor

Editor 窗口演进（详见 [AYEditor/design.md §2.3–§2.4](../AYEditor/design.md#23-viewport-presentation-interim-vs-target)）：

| 阶段 | 窗口来源 | AYDevice 职责 |
|------|----------|---------------|
| E2-interim（当前） | Demo 内 raw Win32 + 视口子 HWND | **无** — 不重复造窗口 |
| E3（`EditorApp`） | **AYDevice `WindowManager`** | 主窗口创建、`getWindowHandle()`、resize/close 回调 |
| E2-composite（可选，与 E3 并行） | 同一主窗口 | 同上；视口仅 rect，不再需要子 HWND |

**Phase-1 骨架范围（满足 E3，不必等 OpenXR / 完整 InputMapping）：**

- `WindowManager` + `DeviceManager::pollEvents`（SDL 窗口事件）
- 可选：`createChildSurface()` 若 E3 仍保留子视口 HWND 过渡

**不在 Phase-1 骨架内：** Gamepad、Action/Axis 映射、XR、Gesture — 编辑器工具栏仍可由 AYUI 鼠标事件驱动。

### 10.3 与 AYFont

```
TextInput 接收 IME 文本
        ↓
发送 TextInputEvent
        ↓
AYUI / 文本渲染器
        ↓
AYFont 渲染文本
```

---

## 11. 目录结构

```
AYDevice/
├── design.md
├── CMakeLists.txt
├── include/
│   └── AYDevice/
│       ├── AYDevice.h              # 主入口
│       │
│       ├── Core/
│       │   ├── AYDevice/DeviceManager.h    # 管理器（窗口+设备）
│       │   ├── AYDevice/WindowManager.h     # SDL2 窗口管理
│       │   ├── AYDevice/InputDevice.h     # 设备基类
│       │   ├── AYDevice/KeyboardDevice.h   # 键盘
│       │   ├── AYDevice/MouseDevice.h     # 鼠标
│       │   ├── AYDevice/GamepadDevice.h   # 手柄
│       │   ├── AYDevice/TouchDevice.h    # 触控
│       │   └── InputManager.h    # 输入管理器（废弃，合并到 DeviceManager）
│       │
│       ├── Mapping/
│       │   └── AYDevice/InputMapping.h    # Action/Axis 映射
│       │
│       ├── XR/
│       │   └── XRDevice.h        # OpenXR VR 设备
│       │
│       └── Features/
│           ├── InputRecorder.h    # 录制回放
│           ├── AYDevice/TextInput.h        # IME 文本输入
│           ├── GestureRecognizer.h # 手势识别
│           ├── AYDevice/InputProfile.h     # 玩家配置
│           ├── HapticFeedback.h   # 触觉反馈
│           ├── AimAssistance.h    # 瞄准辅助
│           └── MotionInput.h      # 运动输入
│
└── src/
    ├── AYDevice.cpp
    ├── DeviceManager.cpp
    ├── WindowManager.cpp
    ├── KeyboardDevice.cpp
    ├── MouseDevice.cpp
    ├── GamepadDevice.cpp
    ├── TouchDevice.cpp
    ├── InputMapping.cpp
    ├── XRDevice.cpp
    └── Features/
        ├── InputRecorder.cpp
        ├── TextInput.cpp
        ├── GestureRecognizer.cpp
        ├── InputProfile.cpp
        ├── HapticFeedback.cpp
        ├── AimAssistance.cpp
        └── MotionInput.cpp
```

---

## 12. 实现优先级

> **子系统**：仅注册 **`DeviceSubSystem`**（窗口 + 输入轮询 + 映射更新），不注册 `InputSubSystem`。

### Phase 1: 核心
- [x] WindowManager (SDL2 窗口)
- [x] IInputDevice 基类
- [x] KeyboardDevice (SDL2)
- [x] MouseDevice (SDL2)
- [x] DeviceManager
- [x] InputMapping (Action/Axis)

### Phase 2: 手柄 + VR
- [x] GamepadDevice (XInput 后端；SDL2 Gamepad API 待补)
- [~] XRDevice (OpenXR) — **移入未来引擎增强项**，见 §7 状态说明（依赖 AYRenderer XR 呈现 + headset）
- [ ] HapticFeedback（手柄震动已在 `GamepadDevice::setVibration`；独立 HapticFeedback 抽象待做）

### Phase 3: 扩展
- [x] TouchDevice（Win32 `WM_TOUCH`；SDL2 Touch 待补）
- [x] TextInput (IME)（Win32 `WM_CHAR` + `WM_IME_COMPOSITION`，UTF-8）
- [x] InputProfile（`InputProfile` + `AYInputNames` + AYConfig 桥接 `AYInputProfileConfig`）

### Phase 3.5 / 下一薄能力: Action Hold（§6.3）
- [ ] `InputMapping::updateHoldTimers` + `getActionHoldTime` / `isActionHeld` / `isActionHoldJustTriggered`
- [ ] 单测：跨帧累加、JustReleased 当帧时长、换绑后计时跟 Action、阈值边沿只触发一次
- [ ] （可选）`DeviceSubSystem` / Logia `InputProvider` 转发

### Phase 4: 高级
- [ ] InputRecorder
- [ ] GestureRecognizer（触控；含 `LongPress`，与 §6.3 Action Hold 分工见 §6.3.2 / §8.3）
- [ ] AimAssistance
- [ ] MotionInput

---

## 13. 参考

- [SDL2 Input](https://wiki.libsdl.org/CategoryInput)
- [SDL2 Window Management](https://wiki.libsdl.org/CategoryVideo)
- [OpenXR Specification](https://www.khronos.org/registry/OpenXR/)
- [OpenXR-SDK-Source](https://github.com/KhronosGroup/OpenXR-SDK-Source)
- Unreal Engine Input System
- Unity Input System

---

## 14. 变更记录

| 日期 | 变更 |
|------|------|
| 2026-07-11 | **§1.3**：锁定 AYInput 废弃、输入栈统一归属 AYDevice；`DeviceSubSystem` 替代 `InputSubSystem` |
| 2026-07-11 | **Phase-2 落地**：`KeyboardDevice` / `MouseDevice`（帧边沿检测）+ `InputMapping`（Action/Axis）+ Win32 键鼠消息翻译，`DeviceManager` 集成键鼠与映射查询 |
| 2026-07-11 | **Phase-3 手柄落地**：`GamepadDevice`（XInput，最多 4 槽；摇杆死区归一化、扳机、按钮边沿、震动）+ `InputMapping` 手柄按钮/模拟轴源，`DeviceManager` 每帧轮询手柄槽位 |
| 2026-07-11 | **Phase-3 输入配置落地**：`InputProfile`（token 化可重绑定 + `applyTo(InputMapping)`）+ `AYInputNames`（枚举↔字符串）+ AYConfig 桥接 `AYInputProfileConfig`（`Input.*` dot-key，独立 `AYDeviceConfig` 目标，核心库不引 AYConfig） |
| 2026-07-11 | **Phase-3 触控 + IME 落地**：`TouchDevice`（`WM_TOUCH` 多点、帧 phase/delta、`RegisterTouchWindow`）+ `TextInput`（`WM_CHAR` 代理对→UTF-8 + `WM_IME_COMPOSITION` 组合串，`imm32`），`DeviceManager` 集成 `touch()`/`textInput()` |
| 2026-07-11 | **XR 决策**：OpenXR 移入「未来引擎增强项」，退出当前进程。前置依赖：AYRenderer XR swapchain 呈现 + 可用 headset runtime。`openxr-loader` vcpkg 端口(1.1.54)可用但未安装；接口设计存档于 §7。 |
| 2026-07-11 | **GameLoop 集成落地**：`DeviceSubSystem`（"Device" 子系统，priority 0 / Unscaled；`initialize`→建窗+设备，`update`→`pollEvents`，`shutdown`→拆除）+ `setBootstrapConfig`/`registerSubSystem`/`findRegistered`（静态库安全显式注册，仿 `RendererSubSystem`）。独立 `AYDeviceSubSystem` 目标，核心库不引 AYGameLoop。 |
| 2026-07-11 | **AYApplication + Renderer 打通**：`DeviceSubSystem::makeWindowProvider()` 返回 `std::function<bool(void*&,uint32_t&,uint32_t&)>`，经 `RendererSubSystem::setWindowProvider` 把窗口句柄喂给渲染器（两模块仅靠 function 签名互通，互不依赖对方头，绕开 Renderer C++17 / Device C++20 冲突）。Renderer 声明 `"Device"` 依赖保证初始化顺序。AYApplication `registerSubSystems()` 注册 DeviceSubSystem（`AYDeviceSubSystem` PRIVATE 链）。 |
| 2026-07-27 | **§6.3 Action Hold 设计锁定（未实现）**：`getActionHoldTime` / `isActionHeld` / `isActionHoldJustTriggered`；阈值按查询传入，不强制短/长按二分。与 §8.3 触控 `GestureRecognizer::LongPress` 明确分工。架构图、§1.3 分期、Phase 3.5 清单同步；文档状态抬头更新。 |