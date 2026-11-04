#pragma once

#include "slsf/core/Configuration.h"
#include "slsf/core/Types.h"
#include "slsf/io/JsonTransfer.h"
#include "slsf/runtime/Lifecycle.h"

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <variant>

namespace slsf::config {

class ConfigurationFile {
public:
    virtual ~ConfigurationFile() = default;
    [[nodiscard]] virtual bool Exists() = 0;
    [[nodiscard]] virtual Result<std::string> Read() = 0;
    [[nodiscard]] virtual Result<std::monostate> Write(const std::string& bytes) = 0;
};

class DiskConfigurationFile final : public ConfigurationFile {
public:
    explicit DiskConfigurationFile(std::string path);
    [[nodiscard]] bool Exists() override;
    [[nodiscard]] Result<std::string> Read() override;
    [[nodiscard]] Result<std::monostate> Write(const std::string& bytes) override;

private:
    std::string path_;
};

class MemoryConfigurationFile final : public ConfigurationFile {
public:
    explicit MemoryConfigurationFile(std::string initialBytes);
    [[nodiscard]] bool Exists() override;
    [[nodiscard]] Result<std::string> Read() override;
    [[nodiscard]] Result<std::monostate> Write(const std::string& bytes) override;

    [[nodiscard]] const std::string& Bytes() const noexcept { return bytes_; }
    void OverwriteExternally(std::string bytes) { bytes_ = std::move(bytes); }
    void FailNextWrite() noexcept { failNextWrite_ = true; }

private:
    std::string bytes_;
    bool failNextWrite_{false};
};

using ConfigurationNotify = std::function<void(const ConfigurationSnapshot&)>;

class ConfigurationStore final : public IConfigurationSource {
public:
    static std::unique_ptr<ConfigurationStore> Open(std::unique_ptr<ConfigurationFile> file);
    ~ConfigurationStore() override;

    void BindNotify(ConfigurationNotify notify);

    [[nodiscard]] static ConfigurationData LegacyDefaults();
    [[nodiscard]] static io::JsonPtr Encode(const ConfigurationData& data);
    [[nodiscard]] static Result<ConfigurationData> Decode(const io::JsonPtr& root,
                                                          const ConfigurationData& base);
    [[nodiscard]] static std::string Validate(const ConfigurationData& data);

    [[nodiscard]] ConfigurationSnapshot Current() const override;
    [[nodiscard]] ConfigurationUpdateResult Reload() override;
    [[nodiscard]] ConfigurationUpdateResult ApplyEdit(ConfigurationData edit);

    [[nodiscard]] std::size_t NotifyCount() const noexcept { return notifyCount_.load(std::memory_order_relaxed); }

private:
    explicit ConfigurationStore(std::unique_ptr<ConfigurationFile> file);

    [[nodiscard]] ConfigurationUpdateResult ReloadLocked(ConfigurationSnapshot* notify);
    void EmitNotification(const ConfigurationSnapshot& snapshot);
    [[nodiscard]] ConfigurationUpdateResult FinishWithCurrentLocked(ConfigurationUpdateStatus status,
                                                                   std::string message);

    std::unique_ptr<ConfigurationFile> file_;
    mutable std::mutex mutex_;
    ConfigurationSnapshot snapshot_;
    std::string lastText_;
    bool synced_{false};
    ConfigurationNotify notify_;
    std::atomic<std::size_t> notifyCount_{0};
};

[[nodiscard]] std::string UpdateMessage(ConfigurationUpdateResult result);

} // namespace slsf::config
