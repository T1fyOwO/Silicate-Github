#pragma once
#include <Geode/Geode.hpp>
#include <algorithm>
#include <functional>
#include <glaze/json/read.hpp>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "keybind.hpp"

struct SLBindingInterface {
    const virtual std::string& getId() = 0;

    virtual void* getValue() = 0;
    virtual void* getPrevious() = 0;

    virtual void notifyChange() = 0;
    virtual std::shared_ptr<KeybindControl> createKeybind(RawKeybind& kb) = 0;

    virtual ~SLBindingInterface() = default;
};

class SLBindingManager {
    std::unordered_map<std::string, std::shared_ptr<SLBindingInterface>>
        m_values;

    using KeybindVector = std::vector<std::shared_ptr<KeybindControl>>;
    std::unordered_map<KeybindControl::HashT, KeybindVector> m_keybinds;

    SLBindingManager() = default;

    bool m_needsNewKey = false;
    KeybindControl* m_currentlyEdited = nullptr;

    bool m_hasRead = false;

    void loadDefaults() {
        struct DefaultBind {
            int key;
            int modifiers;
            const char* tag;
        };

        static constexpr DefaultBind defaults[] = {
            {70, 0, "ui.visible"},             // F
            {86, 0, "updater.frame_advance"},  // V
            {66, 0, "updater.advance_back"},   // B
            {67, 0, "updater.advance_one"},    // C
            {84, 0, "trajectory.enabled"},     // T
            {18, keybind::Alt, "ui.visible"},  // Alt
        };

        for (const auto& d : defaults) {
            auto it = m_values.find(d.tag);
            if (it == m_values.end()) {
                geode::log::error("No value registered for default keybind {}",
                                  d.tag);
                continue;
            }

            RawKeybind raw{d.key, d.modifiers, d.tag, KeybindType::Toggle,
                           true,  "1"};
            this->registerKeybind(it->second->createKeybind(raw));
        }
    }

   public:
    static SLBindingManager* get() {
        static SLBindingManager instance;
        return &instance;
    }

    void writeToFile(const std::filesystem::path& path) {
        KeybindVector vec;
        for (auto it = m_keybinds.begin(); it != m_keybinds.end(); ++it) {
            for (auto& kb : it->second) {
                vec.push_back(kb);
            }
        }

        std::string buf;
        (void)glz::write<glz::opts{.prettify = true}>(vec, buf);

        std::ofstream fd(path);
        fd << buf;
    }

    void readFromFile(const std::filesystem::path& path) {
        if (m_hasRead) return;
        m_hasRead = true;

        std::vector<RawKeybind> vec;
        auto ec = glz::read_file_json(vec, path.string(), std::string{});
        if (ec) {
            std::string helpful = glz::format_error(ec, std::string{});
            geode::log::error(
                "Failed to read keybinds: {}, assuming default keybinds",
                helpful);

            this->loadDefaults();
            return;
        } else {
            geode::log::info("Read keybinds successfully!");
        }

        for (auto& kb : vec) {
            if (!m_values.contains(kb.m_valueTag)) {
                geode::log::error(
                    "Failed to register keybind for tag {}. Does it exist?",
                    kb.m_valueTag);
                continue;
            }

            auto value = m_values[kb.m_valueTag];
            this->registerKeybind(value->createKeybind(kb));
        }
    }

    void startEdit(KeybindControl* k) {
        m_currentlyEdited = k;
        m_needsNewKey = true;
    }

    void stopEdit() {
        m_currentlyEdited = nullptr;
        m_needsNewKey = false;
    }

    bool wantsCapture() { return m_needsNewKey; }
    bool isEditing(KeybindControl* k) {
        return m_currentlyEdited == k && m_needsNewKey;
    }

    void takeInput(int keyCode, bool shift, bool ctrl, bool alt) {
        if (m_currentlyEdited == nullptr) {
            m_needsNewKey = false;
            return;
        }

        if (keybind::isModifierKey(keyCode)) return;

        // shitcode
        auto& bucket = m_keybinds[m_currentlyEdited->getHash()];
        auto it = std::ranges::find_if(
            bucket, [this](auto& k) { return k.get() == m_currentlyEdited; });

        std::shared_ptr<KeybindControl> kb;
        if (it != bucket.end()) {
            kb = *it;
            bucket.erase(it);
        }

        m_currentlyEdited->m_key = keyCode;
        m_currentlyEdited->m_modifiers =
            keybind::packModifiers(ctrl, shift, alt);

        if (kb) this->registerKeybind(kb);

        m_currentlyEdited = nullptr;
        m_needsNewKey = false;
    }

    SLBindingManager(SLBindingManager const&) = delete;
    void operator=(SLBindingManager const&) = delete;

    void registerValue(std::shared_ptr<SLBindingInterface> value) {
        m_values[value->getId()] = value;
    }

    const std::unordered_map<KeybindControl::HashT, KeybindVector>&
    getKeybinds() {
        return m_keybinds;
    }

    void registerKeybind(std::shared_ptr<KeybindControl> kb) {
        geode::log::info("Registering new keybind for {}", kb->getTag());
        if (m_keybinds.contains(kb->getHash())) {
            m_keybinds[kb->getHash()].push_back(kb);
        } else {
            m_keybinds[kb->getHash()] = {kb};
        }
    }

    void processKeyEvent(int key, bool pressed, bool ctrl, bool shift,
                         bool alt) {
        KeybindControl::HashT hash =
            keybind::makeHash(key, keybind::packModifiers(ctrl, shift, alt));

        auto bucket = m_keybinds.find(hash);
        if (bucket == m_keybinds.end()) return;

        for (const auto& kb : bucket->second) {
            auto value = m_values.find(kb->getTag());
            if (value == m_values.end()) continue;

            kb->applyValue(pressed, value->second->getValue(),
                           value->second->getPrevious());
            value->second->notifyChange();
        }
    }
};

template <typename T>
struct SLValuePtr;

template <typename T>
class SLValue : public KeybindContainer<T>, public SLBindingInterface {
   private:
    T* m_value;
    T m_previousValue;

    std::optional<std::function<void(T&)>> m_callback;

   public:
    explicit operator bool() const = delete;

    SLValue(std::string tag, T* value)
        : m_value(value), m_previousValue(*value) {
        this->m_tag = tag;
    }

    T& inner() { return *m_value; }
    const T& inner() const { return *m_value; }

    SLValue& operator=(T value) {
        *m_value = value;
        return *this;
    }

    T operator()() { return *m_value; }

    void handle(std::function<void(T&)> callback) { m_callback = callback; }

    const std::string& getId() override { return this->m_tag; }

    void notifyChange() override {
        if (m_callback.has_value()) {
            m_callback.value()(*m_value);
        }
    }

    void* getValue() override { return m_value; }

    void* getPrevious() override { return &m_previousValue; }

    static SLValuePtr<T> create(std::string tag, T* value) {
        auto binding = SLValuePtr<T>::make(tag, value);
        SLBindingManager::get()->registerValue(binding.inner);
        return binding;
    }

    std::shared_ptr<KeybindControl> createKeybind(RawKeybind& kb) override {
        return SLKeybind<T>::createFromString(
            kb.m_valueTag, kb.m_key, kb.m_type, kb.m_value, kb.m_modifiers);
    }
};

template <typename T>
struct SLValuePtr {
   private:
    using Self = SLValuePtr<T>;
    std::shared_ptr<SLValue<T>> inner;
    SLValuePtr() = default;

   public:
    explicit operator bool() const = delete;

    template <typename... Args>
    static Self make(Args&&... args) {
        Self instance;
        instance.inner =
            std::make_shared<SLValue<T>>(std::forward<decltype(args)>(args)...);
        return instance;
    }

    SLValue<T>* operator->() { return inner.get(); }
    SLValue<T> const* operator->() const { return inner.get(); }

    friend class SLBindingManager;
    friend class SLValue<T>;
};
