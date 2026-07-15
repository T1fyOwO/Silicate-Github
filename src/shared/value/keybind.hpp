#pragma once

#include <glaze/glaze.hpp>
#include <memory>
#include <sstream>

enum class KeybindType { Hold = 0, Toggle, Override };

struct RawKeybind {
    int m_key;
    int m_modifiers;
    std::string m_valueTag;

    KeybindType m_type;
    bool m_active = true;
    std::string m_value;
};

// clang-format off
template <>
struct glz::meta<RawKeybind> {
    using T = RawKeybind;
    static constexpr auto value = object(
        "key", &T::m_key,
        "modifiers", &T::m_modifiers,
        "tag", &T::m_valueTag,
        "type", &T::m_type,
        "value", &T::m_value,
        "active", &T::m_active
    );
};
// clang-format on

namespace keybind {

enum Modifier : int {
    Shift = 0b100,
    Ctrl = 0b010,
    Alt = 0b001,
};

inline int packModifiers(bool ctrl, bool shift, bool alt) {
    return (ctrl ? Ctrl : 0) | (shift ? Shift : 0) | (alt ? Alt : 0);
}

inline int makeHash(int key, int modifiers) { return key | (modifiers << 20); }

inline bool isModifierKey(int key) {
    switch (key) {
        case 0x10:  // VK_SHIFT
        case 0x11:  // VK_CONTROL
        case 0x12:  // VK_MENU
        case 0xA0:  // VK_LSHIFT
        case 0xA1:  // VK_RSHIFT
        case 0xA2:  // VK_LCONTROL
        case 0xA3:  // VK_RCONTROL
        case 0xA4:  // VK_LMENU
        case 0xA5:  // VK_RMENU
            return true;
        default:
            return false;
    }
}

inline std::string keyName(int key) {
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z'))
        return std::string(1, static_cast<char>(key));
    if (key >= 0x70 && key <= 0x87)  // VK_F1 .. VK_F24
        return "F" + std::to_string(key - 0x6F);

    switch (key) {
        case 0x08:
            return "Backspace";
        case 0x09:
            return "Tab";
        case 0x0D:
            return "Enter";
        case 0x1B:
            return "Escape";
        case 0x20:
            return "Space";
        case 0x21:
            return "Page Up";
        case 0x22:
            return "Page Down";
        case 0x23:
            return "End";
        case 0x24:
            return "Home";
        case 0x25:
            return "Left";
        case 0x26:
            return "Up";
        case 0x27:
            return "Right";
        case 0x28:
            return "Down";
        case 0x2D:
            return "Insert";
        case 0x2E:
            return "Delete";
        case 0x97:
            return "Wheel Up";
        case 0x98:
            return "Wheel Down";
        default:
            return "Key " + std::to_string(key);
    }
}

inline std::string keyLabel(int key, int modifiers) {
    std::string label;
    if (modifiers & Ctrl) label += "Ctrl + ";
    if (modifiers & Shift) label += "Shift + ";
    if (modifiers & Alt) label += "Alt + ";
    label += keyName(key);
    return label;
}

}  // namespace keybind

class KeybindControl {
   protected:
    KeybindType m_type;
    bool m_enabled = false;
    bool m_active = true;

   public:
    std::string m_valueTag;
    int m_key;
    int m_modifiers;

    using HashT = int;

    HashT getHash() { return keybind::makeHash(m_key, m_modifiers); }
    const std::string& getTag() { return m_valueTag; }
    std::string getKeyLabel() { return keybind::keyLabel(m_key, m_modifiers); }

    virtual void applyValue(bool pressed, void* value, void* previous) = 0;
    virtual void fromString(const std::string& str) = 0;
    const virtual std::string toString() = 0;

    virtual ~KeybindControl() = default;

    friend glz::meta<KeybindControl>;
};

template <typename T>
class SLKeybind : public KeybindControl {
    using Self = SLKeybind<T>;

   public:
    /**
     * Behavior explanation:
     *
     * ## Hold
     * - The keybind will only be active while the key is being held down
     * - The keybind will be disabled when the key is released
     * - If the value is already set to the keybind's value, it won't be changed
     *
     * ## Toggle
     * - The keybind will be activated when the key is pressed
     * - The keybind will be deactivated when the key is pressed again
     * - If the value is already set to the keybind's value, it'll go back to
     * the previous known value
     *
     * ## Override
     * - Set the value on key press, but don't change it back
     */

   private:
    // What value to set when the keybind is pressed
    T m_value;

   public:
    SLKeybind(int key, int modifiers, KeybindType type, T value,
              std::string tag)
        : m_value(value) {
        m_key = key;
        m_modifiers = modifiers;
        m_valueTag = tag;
        m_type = type;
    }

    static std::shared_ptr<SLKeybind<T>> create(std::string tag, int key,
                                                KeybindType type, T value,
                                                int modifiers = 0) {
        return std::make_shared<SLKeybind<T>>(key, modifiers, type, value, tag);
    }

    static std::shared_ptr<SLKeybind<T>> createFromString(std::string tag,
                                                          int key,
                                                          KeybindType type,
                                                          std::string value,
                                                          int modifiers = 0) {
        return std::make_shared<SLKeybind<T>>(key, modifiers, type,
                                              Self::readFromString(value), tag);
    }

    void applyValue(bool pressed, void* value, void* previous) override {
        if (!m_active) return;

        T& valueRef = *(T*)value;
        T& previousRef = *(T*)previous;

        switch (m_type) {
            case KeybindType::Toggle: {
                if (!pressed) return;

                if (valueRef == m_value) {
                    valueRef = previousRef;
                } else {
                    previousRef = valueRef;
                    valueRef = m_value;
                }

                break;
            }
            case KeybindType::Hold: {
                if (!m_enabled && pressed) {
                    previousRef = valueRef;
                    valueRef = m_value;
                    m_enabled = true;
                } else if (m_enabled && !pressed) {
                    valueRef = previousRef;
                    previousRef = m_value;
                    m_enabled = false;
                }

                break;
            }
            case KeybindType::Override: {
                previousRef = valueRef;
                valueRef = m_value;
                break;
            }
        }
    }

    static T readFromString(const std::string& str) {
        std::istringstream iss(str);
        T value;
        iss >> value;
        return value;
    }

    void fromString(const std::string& str) override {
        std::istringstream iss(str);
        iss >> m_value;
    }

    const std::string toString() override {
        std::ostringstream oss;
        oss << m_value;
        return oss.str();
    }

    friend glz::meta<SLKeybind<T>>;
};

// clang-format off
template <>
struct glz::meta<KeybindControl> {
    using T = KeybindControl;
    static constexpr auto value = object(
        "key", &T::m_key,
        "modifiers", &T::m_modifiers,
        "tag", &T::m_valueTag,
        "type", &T::m_type,
        "value", custom<&T::fromString, &T::toString>,
        "active", &T::m_active,
        "enabled", hide{&T::m_enabled}
    );
};
// clang-format on

template <typename T>
using KeybindPtr = std::shared_ptr<SLKeybind<T>>;

template <typename T>
class KeybindContainer {
   protected:
    std::string m_tag;
    std::vector<SLKeybind<T>> m_keybinds;

   public:
    std::vector<SLKeybind<T>>& getKeybinds() { return m_keybinds; }
};
