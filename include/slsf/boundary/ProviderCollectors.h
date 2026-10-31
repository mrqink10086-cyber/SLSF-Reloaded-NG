#pragma once
#include "slsf/core/Types.h"
#include <functional>
namespace RE::BSScript { class IVirtualMachine; }
namespace SKSE { class SerializationInterface; }
namespace slsf::boundary {
inline constexpr std::uint32_t kCollectorCallsRecord = 0x51434C53;
void InitializeProviderCollectors(std::function<Epoch()> epoch,
                                  std::function<bool(const Epoch&)> isCurrent);
bool RegisterProviderCollectors(RE::BSScript::IVirtualMachine* vm);
bool SaveProviderCollectorCalls(SKSE::SerializationInterface* serialization);
bool LoadProviderCollectorCalls(SKSE::SerializationInterface* serialization,
                                std::uint32_t version, std::uint32_t length);
void ResetProviderCollectorCalls();
void ResumeProviderCollectorCalls(bool loaded);
}
